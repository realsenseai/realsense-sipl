#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
# D555e RoCE resolution / fps matrix: every depth and RGB profile, one stream at a
# time, with roce_latency.sh's capture. Profiles come from d555_profiles.py.
#
#   sh rs/roce_matrix.sh [depth|rgb|all] [seconds per profile]
#
# One block per profile: app RESULT + sniffer counts + latency medians (p99) in ms.
# Exits 1 if any profile failed.
WHICH=${1:-all}
SECONDS_PER_RUN=${2:-10}
# The camera refuses RGB 1280x800 (no implicit profile). A stream that sends nothing runs until
# the receiver's frame_limit ticks run out, 1 s each (fps x seconds, e.g. 300 s), then fails;
# SKIP="" runs them anyway.
SKIP=${SKIP-"rgb4 rgb5"}
export OUT=${OUT:-rs/matrix} LATENCY_ARGS=--oneline

python3 rs/d555_profiles.py | {
    rc=0
    while read -r stream profile width height fps; do
        [ "$WHICH" = all ] || [ "$WHICH" = "$stream" ] || continue
        case " $SKIP " in *" $stream$profile "*) echo "=== $stream p$profile skipped"; continue ;; esac
        echo "=== $stream p$profile ${width}x${height}@$fps" \
            "(payload $((width * height * 16 * fps / 1000000)) Mbps)"
        # docker -it needs the TTY; keep it off the loop's pipe
        sh rs/roce_latency.sh "$stream" "$profile" $((fps * SECONDS_PER_RUN)) < /dev/tty || rc=1
    done
    exit "$rc"
}
