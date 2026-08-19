#!/usr/bin/env bash
# apply.sh — apply the D457 multi-camera SerDes changes to a stock Jetson SIPL SDK tree.
#
# These four SDK drivers diverge too far from stock to express as the targeted edits in
# ../patch_max9295_d457.py / ../patch_max96712_d457.py, but they are still NVIDIA's code: only our
# diff is kept here, never the files themselves, so nothing NVIDIA-licensed is redistributed.
#
# Idempotent: a tree that is already patched is detected and left alone. All-or-nothing: every patch
# is dry-run first, and if any one does not fit, nothing is modified. The first apply leaves a
# one-time <file>.orig next to each patched file.
#
# USAGE
#   ./apply.sh [SIPL_ROOT]        # default /usr/src/jetson_sipl_api/sipl
#   ./apply.sh -R [SIPL_ROOT]     # revert
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REVERT=0
[ "${1:-}" = "-R" ] && { REVERT=1; shift; }
SIPL="${1:-/usr/src/jetson_sipl_api/sipl}"
[ -d "$SIPL/uddf" ] || { echo "FATAL: not a SIPL SDK tree: $SIPL" >&2; exit 1; }

# patch file : path under $SIPL
MAP=(
  "MAX9295.cpp:uddf/drivers/serializers/MAX9295/MAX9295.cpp"
  "MAX9295.hpp:uddf/drivers/serializers/MAX9295/MAX9295.hpp"
  "MAX967XX.cpp:uddf/drivers/deserializers/MAX967XX/MAX967XX.cpp"
  "MAX967XXHsl.py:uddf/drivers/deserializers/MAX967XX/MAX967XXHsl.py"
)

# Classify every patch before touching anything: "refuses to half-apply" has to mean that a tree the
# patches do not fit is left exactly as it was, not left half-modified with rc=1.
declare -a TO_APPLY=()
fatal=0
for entry in "${MAP[@]}"; do
    name="${entry%%:*}"; rel="${entry#*:}"
    patch_file="$HERE/$name.patch"
    target="$SIPL/$rel"
    [ -f "$patch_file" ] || { echo "FATAL: missing $patch_file" >&2; exit 1; }
    [ -f "$target" ]     || { echo "FATAL: missing $target" >&2; exit 1; }

    if [ "$REVERT" = "1" ]; then
        if patch -R --dry-run -p1 -f "$target" < "$patch_file" >/dev/null 2>&1; then
            TO_APPLY+=("$entry")
        else
            echo "  not applied, skipping  $rel"
        fi
        continue
    fi

    if patch -R --dry-run -p1 -f "$target" < "$patch_file" >/dev/null 2>&1; then
        echo "  already patched  $rel"
    elif patch --dry-run -p1 -f "$target" < "$patch_file" >/dev/null 2>&1; then
        TO_APPLY+=("$entry")
    else
        echo "  WILL NOT APPLY  $rel (SDK version mismatch?)" >&2
        fatal=1
    fi
done

if [ "$fatal" = "1" ]; then
    echo "FATAL: at least one patch does not fit $SIPL; nothing was modified" >&2
    exit 1
fi

for entry in "${TO_APPLY[@]+"${TO_APPLY[@]}"}"; do
    name="${entry%%:*}"; rel="${entry#*:}"
    patch_file="$HERE/$name.patch"
    target="$SIPL/$rel"
    if [ "$REVERT" = "1" ]; then
        patch -R -p1 -s "$target" < "$patch_file"
        echo "  reverted  $rel"
        continue
    fi
    # Keep a one-time .orig, so a later SDK upgrade that breaks the patch still has something to
    # restore from. -b would overwrite an existing .orig, so only pass it the first time.
    if [ -f "$target.orig" ]; then
        patch -p1 -s "$target" < "$patch_file"
    else
        patch -p1 -s -b --suffix=.orig "$target" < "$patch_file"
    fi
    echo "  patched  $rel"
done
