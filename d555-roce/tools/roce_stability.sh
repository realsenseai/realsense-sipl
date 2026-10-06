#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
# D555e RoCE stability runs. Each run is a full session: container start,
# enumeration + configure_roce, stream start, N frames, stream stop. Settings: roce_common.sh.
#
#   sh rs/roce_stability.sh soak <depth|rgb|dual> <frames>   # e.g. soak rgb 900 (30 s @ 30 fps)
#   sh rs/roce_stability.sh cycle <iterations> <frames>       # alternates rgb / depth
#   sh rs/roce_stability.sh dualcycle <iterations> <frames>   # depth + rgb together
#
# Per run: the app's RESULT line(s), the RDMA packets the CX-7 accepted, and the packets the
# received frames account for (the camera may send a few more around start/stop).
# Exits 1 if any run failed.
. rs/roce_common.sh

run() {  # run <tag> <depth|rgb|dual> <frames>
    if [ "$2" = dual ]; then app=roce_d555_dual.py; else app="roce_d555_single.py --stream $2"; fi
    before=$(rdma_packets)
    run_app "$OUT/$1.log" "$app --frame-limit $3"
    packets=$(($(rdma_packets) - before))
    line=$(results "$OUT/$1.log") || failed=1
    echo "$1 $line rdma_pkts=$packets expected_pkts=$(expected_packets "$OUT/$1.log")"
}

case "$1" in
soak)
    run "soak_$2" "$2" "$3" ;;
cycle | dualcycle)
    i=1
    while [ "$i" -le "$2" ]; do
        if [ "$1" = dualcycle ]; then s=dual; elif [ $((i % 2)) -eq 1 ]; then s=rgb; else s=depth; fi
        run "$1_$(printf %02d "$i")_$s" "$s" "$3"
        i=$((i + 1))
    done ;;
*)
    echo "usage: $0 soak <depth|rgb|dual> <frames> | cycle|dualcycle <iterations> <frames>" >&2
    exit 2 ;;
esac
exit "$failed"
