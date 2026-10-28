#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 RealSense AI. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# hsb.sh — build and use the HSB tree that tools/prepare_hsb.sh set up, inside HSB's container.
#
# The hololink-demo image ships the stock hololink Python package, without this project's
# operators and sensors. `build` builds the tree (SIPL and FuSa on) and then links
# build/pyroot/hololink: the tree's Python sources merged with the modules the build compiled.
# `shell` starts the container with PYTHONPATH on that, so the project's Python players import it.
#
# USAGE
#   tools/hsb.sh build                 # build in the container, then link build/pyroot
#   tools/hsb.sh shell [<command>...]  # the container in the HSB tree, PYTHONPATH set; bash by default
#
# HSB_TREE overrides the tree (default ./holoscan-sensor-bridge next to tools/), NAME the
# container name of `shell` (default hsb). Run from a terminal on the device's desktop for the
# players' windows.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HSB=${HSB_TREE:-$ROOT/holoscan-sensor-bridge}
PYROOT=$HSB/build/pyroot

die() { echo "hsb.sh: $*" >&2; exit 1; }

[ -f "$HSB/docker/demo.sh" ] || die "no HSB tree at $HSB; run tools/prepare_hsb.sh d457|d555 first"
cd "$HSB"

case "${1:-}" in
build)
    # GIT_CONFIG_GLOBAL: a global url.<ssh>.insteadOf rewrite would turn CMake's HTTPS dependency
    # downloads into SSH ones, which stop at a prompt inside the container.
    exec sh docker/demo.sh --name=hsb-build bash -c '
        set -e
        export GIT_CONFIG_GLOBAL=/dev/null
        cmake -S . -B build -DCCCL_DIR:PATH=/usr/local/cuda/targets/sbsa-linux/lib/cmake/cccl \
            -DHOLOLINK_BUILD_SIPL=1 -DHOLOLINK_BUILD_FUSA=1
        cmake --build build -j"$(nproc)"
        rm -rf build/pyroot && mkdir -p build/pyroot/hololink
        cp -rs "$PWD/python/hololink/." build/pyroot/hololink/
        cp -rsf "$PWD/build/python/lib/hololink/." build/pyroot/hololink/
        echo "hsb.sh: built; tools/hsb.sh shell to run the players"'
    ;;
shell)
    shift
    [ -d "$PYROOT/hololink" ] || die "no $PYROOT; run tools/hsb.sh build first"
    [ "$#" -gt 0 ] || set -- bash
    # The container runs as root: let it open windows on this desktop.
    if [ -n "${DISPLAY:-}" ] && command -v xhost > /dev/null; then
        xhost +local: > /dev/null
    fi
    exec sh docker/demo.sh --name="${NAME:-hsb}" env PYTHONPATH="$PYROOT" "$@"
    ;;
*)
    sed -n '/^# USAGE/,/^# players/p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
    ;;
esac
