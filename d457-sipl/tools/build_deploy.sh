#!/usr/bin/env bash
# build_deploy.sh — build the D457 SIPL components on the rig and install them.
#
# Builds (from the rig's SIPL SDK tree) the three D457 artifacts and installs the .so's into the SIPL
# driver dir so the next `nvsipl_camera -c D457_Camera` picks them up:
#   - libuddf_d457cameramodule_library.so   (D457 sensor/module driver + MAX9295 serializer HSL)
#   - libnvuddf_max96712_library.so         (MAX96712 deserializer: pixel-map / PHY HSL)
#   - libnvsipl_qry_d457.so                 (query plugin: camera-config JSON)
#
# Run ON the rig (fw-advantech-thor-1). No reboot needed unless you change the cdi-mgr DT overlay.
# Companion docs: ../docs/debug-guide.md §A (Build) / §B (Deploy), ../docs/nvsipl_camera-guide.md.
#
# USAGE
#   ./build_deploy.sh                       # build all 3 from the SDK tree as-is + install
#   ./build_deploy.sh -c driver             # build+install only the camera-module driver
#   ./build_deploy.sh -c deser,query        # subset (comma list of: driver,deser,query)
#   ./build_deploy.sh -r /path/to/repo      # FIRST sync sources from a repo checkout + apply patches
#   ./build_deploy.sh -r /path/to/repo -p   # ... and (re)apply the SerDes SDK patches explicitly
#   RIG_SUDO_PW=<rig sudo password> ./build_deploy.sh   # non-interactive sudo (else sudo cached)
#
# ENV OVERRIDES: SIPL, NVSIPL_DRV, QUERY_SRC, JOBS (see defaults below).
set -euo pipefail

SIPL=${SIPL:-$HOME/sipl_full/usr/src/jetson_sipl_api/sipl}
NVSIPL_DRV=${NVSIPL_DRV:-/usr/lib/nvsipl_drv}
QUERY_SRC=${QUERY_SRC:-$HOME/d457_query.cpp}
JOBS=${JOBS:-4}

REPO=""
APPLY_PATCHES=0
COMPONENTS="driver,deser,query"

usage() { sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
    case "$1" in
        -r|--repo)       REPO="$2"; shift 2 ;;
        -p|--patches)    APPLY_PATCHES=1; shift ;;
        -c|--components) COMPONENTS="$2"; shift 2 ;;
        -j|--jobs)       JOBS="$2"; shift 2 ;;
        -h|--help)       usage 0 ;;
        *) echo "unknown arg: $1" >&2; usage 1 ;;
    esac
done

# sudo helper — prime with RIG_SUDO_PW if given, else rely on a cached/NOPASSWD sudo.
[ -n "${RIG_SUDO_PW:-}" ] && echo "$RIG_SUDO_PW" | sudo -S -v 2>/dev/null || true
say()  { echo ">> $*"; }
die()  { echo "FATAL: $*" >&2; exit 1; }
has()  { case ",$COMPONENTS," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

[ -d "$SIPL/build" ] || die "SDK build dir not found: $SIPL/build (set SIPL=...)"

# ── 1. (optional) sync sources from a repo checkout ───────────────────────────────────────────────
if [ -n "$REPO" ]; then
    D="$REPO/d457-sipl"
    [ -d "$D" ] || die "repo dir has no d457-sipl/: $REPO"
    say "syncing sources from $D into the SDK tree"
    cp "$D"/uddf_driver/D457Sensor.cpp "$D"/uddf_driver/D457Sensor.hpp \
       "$D"/uddf_driver/D457Module.cpp "$D"/uddf_driver/D457Module.hpp \
       "$D"/uddf_driver/ID457CameraControl.hpp \
       "$D"/d457_ds5_registers.h  "$SIPL/uddf/cdd_d457/"
    cp "$D"/query/d457_query.cpp "$QUERY_SRC"
    APPLY_PATCHES=1   # fresh sources -> the SerDes HSL/.cpp patches must be (re)applied
fi

# ── 2. (optional) apply the SerDes SDK patches (idempotent; no-op if already applied) ──────────────
if [ "$APPLY_PATCHES" = "1" ]; then
    [ -n "$REPO" ] || die "--patches needs --repo (the patch_*.py live in d457-sipl/sdk-patches/)"
    say "applying SerDes SDK patches"
    python3 "$REPO/d457-sipl/sdk-patches/patch_max9295_d457.py"  "$SIPL"
    python3 "$REPO/d457-sipl/sdk-patches/patch_max96712_d457.py" "$SIPL"
fi

# ── 3. build ───────────────────────────────────────────────────────────────────────────────────────
MAKE_TARGETS=""
has driver && MAKE_TARGETS="$MAKE_TARGETS uddf_d457cameramodule_library"
has deser  && MAKE_TARGETS="$MAKE_TARGETS uddf_max96712_library"
if [ -n "$MAKE_TARGETS" ]; then
    say "building:$MAKE_TARGETS (make -j$JOBS)"
    ( cd "$SIPL/build" && make -j"$JOBS" $MAKE_TARGETS )
fi

QRY_BUILT=""
if has query; then
    [ -f "$QUERY_SRC" ] || die "query source not found: $QUERY_SRC (set QUERY_SRC=...)"
    # Build into a private 0700 dir. A predictable /tmp path lets any local user pre-create the
    # target as a symlink and have it sudo cp'd into the driver dir below, i.e. root-loaded by SIPL.
    QRY_TMPDIR=$(mktemp -d) || die "mktemp failed"
    chmod 700 "$QRY_TMPDIR"
    trap 'rm -rf "${QRY_TMPDIR:-}"' EXIT
    QRY_BUILT="$QRY_TMPDIR/libnvsipl_qry_d457.so"
    say "building query plugin from $QUERY_SRC"
    g++ -shared -fPIC -O2 -o "$QRY_BUILT" "$QUERY_SRC"
fi

# ── 4. install ───────────────────────────────────────────────────────────────────────────────────
DRIVER_SO="$SIPL/build/uddf/cdd_d457/libuddf_d457cameramodule_library.so"
DESER_SO="$SIPL/build/uddf/libraries/deserMAX96712/libnvuddf_max96712_library.so"
say "installing into $NVSIPL_DRV"
has driver && { [ -f "$DRIVER_SO" ] || die "missing $DRIVER_SO"; sudo cp "$DRIVER_SO" "$NVSIPL_DRV/"; }
has deser  && { [ -f "$DESER_SO"  ] || die "missing $DESER_SO";  sudo cp "$DESER_SO"  "$NVSIPL_DRV/"; }
has query  && sudo cp "$QRY_BUILT" "$NVSIPL_DRV/libnvsipl_qry_d457.so"

# ── 5. verify ────────────────────────────────────────────────────────────────────────────────────
say "installed (timestamps):"
ls -la "$NVSIPL_DRV"/libuddf_d457cameramodule_library.so \
       "$NVSIPL_DRV"/libnvuddf_max96712_library.so \
       "$NVSIPL_DRV"/libnvsipl_qry_d457.so 2>/dev/null | sed 's/^/   /'
say "done. Run:  sudo nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -r 15 -s"
say "(power-cycle the DS5 first if not using SIPL-owned PoC; pick streams with D457_STREAMS=depth,rgb,ir)"
