#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 RealSense AI. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# prepare_hsb.sh — set up a Holoscan Sensor Bridge tree for ONE project of this repository.
#
# This repository does not vendor HSB. It builds on the tagged HSB release below, plus:
#   1. the shared patches in hsb-patches/<version>/ (RAW_16 pixel format, SIPL driver mounts), and
#   2. one project's Holoscan code: <project>/hsb/{src,python,examples} copied in, then
#      <project>/hsb/hsb-registration.patch applied to register it with HSB's CMake and Python
#      package.
#
# d457-sipl (GMSL, L4T R39.2) and d555-sipl (CoE, L4T R38) are prepared one at a time: their capture
# operators target different SIPL API generations, so a tree holds one of them.
#
# Safe to re-run: the clone and the patches are skipped when already in place, and the project
# sources are copied again, so re-running picks up edits. It refuses a tree prepared for the
# other project, or one at a different HSB commit.
#
# USAGE
#   tools/prepare_hsb.sh d457|d555 [<hsb-tree>]
#   tools/prepare_hsb.sh --check [<hsb-tree>]     # report the HSB version and the project prepared
#
# <hsb-tree> defaults to ./holoscan-sensor-bridge (ignored by git).
set -euo pipefail

HSB_REPOSITORY=https://github.com/nvidia-holoscan/holoscan-sensor-bridge.git
HSB_TAG=2.7.0
HSB_COMMIT=1df133d4c78a6921249be4b147d65554aa8efa68

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
say() { echo ">> $*"; }
die() { echo "FATAL: $*" >&2; exit 1; }

PROJECT="${1:-}"
[ -n "$PROJECT" ] || die "usage: $0 {d457|d555|--check} [hsb-tree]"
HSB="${2:-$REPO/holoscan-sensor-bridge}"

prepared_project() {
    if [ -d "$HSB/src/hololink/operators/d457_sipl_capture" ]; then echo d457
    elif [ -d "$HSB/src/hololink/operators/d555_sipl_capture" ]; then echo d555
    else echo none; fi
}

if [ "$PROJECT" = "--check" ]; then
    [ -d "$HSB/.git" ] || die "not an HSB checkout: $HSB"
    echo "HSB $(git -C "$HSB" describe --tags --always 2>/dev/null) ($(git -C "$HSB" rev-parse --short HEAD)), project: $(prepared_project)"
    exit 0
fi

case "$PROJECT" in
    d457|d555) ;;
    *) die "unknown project '$PROJECT' (expected d457 or d555)" ;;
esac
SRC="$REPO/$PROJECT-sipl/hsb"
[ -d "$SRC" ] || die "no such project: $SRC"

# 1. The pinned HSB release.
if [ ! -e "$HSB" ]; then
    say "cloning HSB $HSB_TAG into $HSB"
    # A global url.<ssh>.insteadOf rewrite would turn this public HTTPS clone into an SSH one.
    GIT_CONFIG_GLOBAL=/dev/null git -c advice.detachedHead=false clone --quiet --depth 1 \
        --branch "$HSB_TAG" "$HSB_REPOSITORY" "$HSB"
fi
[ -d "$HSB/.git" ] || die "$HSB exists and is not an HSB git checkout"
have=$(git -C "$HSB" rev-parse HEAD)
[ "$have" = "$HSB_COMMIT" ] || die "$HSB is at $have, expected HSB $HSB_TAG ($HSB_COMMIT)"

other=$(prepared_project)
if [ "$other" != none ] && [ "$other" != "$PROJECT" ]; then
    die "$HSB is prepared for $other. Use another tree, or reset this one: git -C '$HSB' checkout -- . && git -C '$HSB' clean -fd"
fi

# Apply a patch unless it is already applied; a patch that fits neither way is fatal.
apply_patch() {
    local patch=$1
    if git -C "$HSB" apply --reverse --check "$patch" 2>/dev/null; then
        echo "   = $(basename "$patch") (already applied)"
    elif git -C "$HSB" apply --check "$patch" 2>/dev/null; then
        git -C "$HSB" apply "$patch"
        echo "   + $(basename "$patch")"
    else
        die "$(basename "$patch") does not apply to $HSB"
    fi
}

# 2. Shared patches.
say "shared patches for HSB $HSB_TAG"
for patch in "$REPO/hsb-patches/$HSB_TAG"/*.patch; do
    apply_patch "$patch"
done

# 3. The project's own sources, at their HSB-relative paths. __pycache__ is skipped: running an
#    example in place leaves bytecode behind, and copying it into the tree serves no purpose.
say "$PROJECT sources"
while IFS= read -r rel; do
    mkdir -p "$(dirname "$HSB/$rel")"
    cp "$SRC/$rel" "$HSB/$rel"
done < <(cd "$SRC" && find src python examples -type f -not -path '*/__pycache__/*' -not -name '*.pyc' | sort)
echo "   $(cd "$SRC" && find src python examples -type f -not -path '*/__pycache__/*' -not -name '*.pyc' | wc -l) files"

# 4. Register them with HSB.
say "$PROJECT registration"
apply_patch "$SRC/hsb-registration.patch"

say "done. Build it in the HSB container, then open the container to run the players:"
echo "   tools/hsb.sh build"
echo "   tools/hsb.sh shell"
