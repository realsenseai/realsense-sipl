#!/usr/bin/env bash
# graft_hsb.sh — overlay ONE project's Holoscan code onto a Holoscan Sensor Bridge checkout.
#
# d457-sipl (D457 over GMSL) and d555-sipl (D555 over CoE) are independent projects that happen to
# share this repository. They target different Jetson releases and different, mutually exclusive
# SIPL APIs:
#
#   d555-sipl : L4T R38   — CameraSystemConfig / GetCameraSystemConfig, the API upstream HSB 2.5.0
#                            itself is written against
#   d457-sipl : L4T R39.2 — sensorconfig::SensorSystemConfig, which replaced it; the upstream
#                            sipl_capture / d555_sipl_capture operators do not compile there
#
# So holoscan-sensor-bridge/ is kept at (near) upstream and carries NO camera-specific code.
# Each project keeps its own operators, examples and wiring under <project>/hsb/, and this script
# grafts exactly one of them onto an HSB tree before building.
#
# You graft ONE project at a time — grafting the other overwrites the wiring, by design. That is
# the single-repository equivalent of how these projects used to be kept apart: the superproject
# had one branch each, pinning the HSB submodule to a different HSB branch, so the two wirings
# never met in one working tree.
#
# USAGE
#   tools/graft_hsb.sh d457 [<hsb-tree>]
#   tools/graft_hsb.sh d555 [<hsb-tree>]
#   tools/graft_hsb.sh --check <hsb-tree>     # report which project (if any) is currently grafted
#
# <hsb-tree> defaults to ./holoscan-sensor-bridge.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
say() { echo ">> $*"; }
die() { echo "FATAL: $*" >&2; exit 1; }

PROJECT="${1:-}"
[ -n "$PROJECT" ] || die "usage: $0 {d457|d555|--check} [hsb-tree]"

if [ "$PROJECT" = "--check" ]; then
    HSB="${2:-$REPO/holoscan-sensor-bridge}"
    if [ -d "$HSB/src/hololink/operators/d457_sipl_capture" ]; then echo "d457 grafted"
    elif [ -d "$HSB/src/hololink/operators/d555_sipl_capture" ]; then echo "d555 grafted"
    else echo "none (pristine upstream HSB)"; fi
    exit 0
fi

case "$PROJECT" in
    d457|d555) ;;
    *) die "unknown project '$PROJECT' (expected d457 or d555)" ;;
esac

SRC="$REPO/$PROJECT-sipl/hsb"
HSB="${2:-$REPO/holoscan-sensor-bridge}"
[ -d "$SRC" ]     || die "no such project overlay: $SRC"
[ -d "$HSB/src" ] || die "not an HSB tree: $HSB"

# Refuse to stack one project on top of the other.
OTHER=$([ "$PROJECT" = "d457" ] && echo d555 || echo d457)
if [ -d "$HSB/src/hololink/operators/${OTHER}_sipl_capture" ]; then
    die "$HSB already has the $OTHER graft. Start from a clean HSB checkout (git -C '$HSB' checkout -- . && git -C '$HSB' clean -fd)."
fi

say "grafting $PROJECT onto $HSB"
# The project's own sources, at their HSB-relative paths.
# __pycache__/*.pyc are skipped: running an example in place leaves bytecode next to it, and copying
# it lands in an HSB directory that may not exist yet -- under `set -e` that aborted the graft
# half-done, after the sources but before some of the wiring.
while IFS= read -r rel; do
    dst="$HSB/$rel"
    mkdir -p "$(dirname "$dst")"
    cp "$SRC/$rel" "$dst"
    echo "   + $rel"
done < <(cd "$SRC" && find src python examples -type f -not -path '*/__pycache__/*' -not -name '*.pyc' 2>/dev/null | sed 's#^[.]/##')

# The shared HSB files this project must replace wholesale to register the above.
while IFS= read -r rel; do
    mkdir -p "$(dirname "$HSB/$rel")"
    cp "$SRC/wiring/$rel" "$HSB/$rel"
    echo "   ~ $rel (wiring)"
done < <(cd "$SRC/wiring" && find . -type f -not -path '*/__pycache__/*' -not -name '*.pyc' | sed 's#^[.]/##')

say "done. Build with:  cmake -S '$HSB' -B <build> -DHOLOLINK_BUILD_SIPL=1 && cmake --build <build> -j"
