#!/usr/bin/env bash
# sync_hsb.sh — sync the D457 Holoscan Sensor Bridge (HSB) player sources between this repo
# (d457-sipl/hsb/) and the rig's HSB tree, and (re)build them there.
#
# The D457 Holoscan viewer (D457SIPLCaptureOp operator + d457_sipl_player.py) lives in the HSB repo
# tree on the rig (~/holoscan-sensor-bridge, not itself a git checkout there). d457-sipl/hsb/ is the
# git-tracked copy (see hsb/README.md for why: HSB here is a submodule pinned to upstream main-2.5.0,
# so we vendor the D457-specific files instead of carrying a submodule fork).
#
# USAGE (run from your dev box; needs SSH to the rig — see `reference-lab-rig-ssh` conventions):
#   ./sync_hsb.sh push          # repo -> rig, then rebuild (op + python binding)
#   ./sync_hsb.sh push --no-build
#   ./sync_hsb.sh pull           # rig -> repo (capture on-rig edits back into git before they're lost)
#   ./sync_hsb.sh build          # rebuild only (no file sync)
#
# ENV OVERRIDES: RIG_HOST, RIG_USER, RIG_KEY, RIG_HSB (rig HSB tree root).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
HSB_LOCAL="$REPO/d457-sipl/hsb"

RIG_HOST=${RIG_HOST:-fw-advantech-thor-1}
RIG_USER=${RIG_USER:-mic-742}
RIG_KEY=${RIG_KEY:-$HOME/.ssh/claude_fw-advantech-thor-1_ed25519}
RIG_HSB=${RIG_HSB:-/home/$RIG_USER/holoscan-sensor-bridge}

SSH=(ssh -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST")
SCP=(scp -o BatchMode=yes -i "$RIG_KEY")

say() { echo ">> $*"; }
die() { echo "FATAL: $*" >&2; exit 1; }

CMD="${1:-}"
NO_BUILD=0
[ "${2:-}" = "--no-build" ] && NO_BUILD=1

[ -d "$HSB_LOCAL" ] || die "missing $HSB_LOCAL"

# Files that live in the HSB submodule tree and get overwritten wholesale from hsb/wiring/
# (repo-relative source : rig-relative dest).
WIRING=(
  "wiring/operators_CMakeLists.txt:src/hololink/operators/CMakeLists.txt"
  "wiring/python_operators_CMakeLists.txt:python/hololink/operators/CMakeLists.txt"
  "wiring/operators___init__.py:python/hololink/operators/__init__.py"
  "wiring/examples_CMakeLists.txt:examples/CMakeLists.txt"
)

# New D457-only files/dirs (repo-relative : rig-relative), copied whole.
NEWFILES=(
  "src/hololink/operators/d457_sipl_capture:src/hololink/operators/d457_sipl_capture"
  "python/hololink/operators/d457_sipl_capture:python/hololink/operators/d457_sipl_capture"
  "examples/d457_sipl_player.py:examples/d457_sipl_player.py"
)

do_push() {
    say "pushing HSB D457 sources: repo -> $RIG_HOST:$RIG_HSB"
    for pair in "${NEWFILES[@]}"; do
        src="${pair%%:*}"; dst="${pair##*:}"
        say "  $src -> $dst"
        if [ -d "$HSB_LOCAL/$src" ]; then
            "${SSH[@]}" "mkdir -p '$RIG_HSB/$dst'"
            # Trailing "/." copies the DIRECTORY'S CONTENTS into an existing remote dir; without it
            # `scp -r dir remote:existing_dir` nests a whole extra `existing_dir/dir/...` copy and
            # silently leaves the real destination files untouched (bit us once already).
            "${SCP[@]}" -r "$HSB_LOCAL/$src/." "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst/"
        else
            "${SSH[@]}" "mkdir -p '$RIG_HSB/$(dirname "$dst")'"
            "${SCP[@]}" "$HSB_LOCAL/$src" "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst"
        fi
    done
    for pair in "${WIRING[@]}"; do
        src="${pair%%:*}"; dst="${pair##*:}"
        say "  $src -> $dst"
        "${SCP[@]}" "$HSB_LOCAL/$src" "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst"
    done
}

do_pull() {
    say "pulling HSB D457 sources: $RIG_HOST:$RIG_HSB -> repo (capture on-rig edits)"
    for pair in "${NEWFILES[@]}"; do
        src="${pair%%:*}"; dst="${pair##*:}"
        say "  $dst -> $src"
        if "${SSH[@]}" "[ -d '$RIG_HSB/$dst' ]"; then
            mkdir -p "$HSB_LOCAL/$src"
            "${SCP[@]}" -r "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst/." "$HSB_LOCAL/$src/"
        else
            "${SCP[@]}" "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst" "$HSB_LOCAL/$src"
        fi
    done
    for pair in "${WIRING[@]}"; do
        src="${pair%%:*}"; dst="${pair##*:}"
        say "  $dst -> $src"
        "${SCP[@]}" "$RIG_USER@$RIG_HOST:$RIG_HSB/$dst" "$HSB_LOCAL/$src"
    done
    say "review with: git -C '$REPO' diff -- d457-sipl/hsb"
}

do_build() {
    # The build dir was originally populated as root inside the HSB demo container (CUDA/nvcc is
    # only on the container's PATH there); rebuild the same way via docker/demo.sh, else cmake
    # fails to reconfigure ("Permission denied" on CMakeCache.txt, "Failed to find nvcc").
    #
    # NOTE: this only refreshes $RIG_HSB/build (bind-mounted, persists). The demo container's
    # `import hololink` resolves a SEPARATE copy baked into the image at
    # /usr/local/lib/python3.12/dist-packages/hololink/ -- since every `docker run` here uses
    # --rm, any in-container fix-up to THAT path is discarded the moment the container exits, so
    # it CANNOT be refreshed here (a later, separate container invocation would just see the stale
    # image copy again). The actual player launch (run_d457_cams.sh) re-copies the fresh .so over
    # that path as its own first step, in the SAME container invocation that then runs the player.
    say "rebuilding on rig inside the HSB demo container (targets: d457_sipl_capture, _python)"
    # demo.sh forwards args via an UNQUOTED `$*`, which word-splits any compound command (killing
    # `&&`/quoting) -- stage a real script file and pass just its path (one plain word) instead.
    # Must live under $RIG_HSB (bind-mounted into the container); /tmp is NOT mounted into it.
    "${SSH[@]}" "cat > '$RIG_HSB/_build_d457_hsb.sh'" <<'EOF'
#!/bin/bash
set -e
cd build
cmake --build . --target d457_sipl_capture -j4
cmake --build . --target d457_sipl_capture_python -j4
EOF
    # demo.sh's `docker run` uses -it; force a pty over SSH (-tt) so it doesn't fail with
    # "the input device is not a TTY" in this non-interactive invocation.
    ssh -tt -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST" \
        "cd '$RIG_HSB' && sh docker/demo.sh bash _build_d457_hsb.sh; rm -f _build_d457_hsb.sh"
    say "build done. Installed .so: $RIG_HSB/build/python/lib/operators/d457_sipl_capture/*.so"
    say "example app: $RIG_HSB/examples/d457_sipl_player.py (no build step, plain python)"
}

case "$CMD" in
    push)  do_push;  [ "$NO_BUILD" = "1" ] || do_build ;;
    pull)  do_pull ;;
    build) do_build ;;
    *) echo "usage: $0 {push [--no-build]|pull|build}" >&2; exit 1 ;;
esac
