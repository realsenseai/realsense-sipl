#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
# D555e RoCE latency run: capture with the CX-7 RDMA sniffer while the app logs
# per-frame times, then print the breakdown. Settings: roce_common.sh. Exits 1 if the run failed.
#
#   sh rs/roce_latency.sh <depth|rgb> <profile> <frames>     # e.g. roce_latency.sh depth 4 300
#
# LATENCY_ARGS is passed to roce_latency.py (e.g. --oneline); KEEP_PCAP=1 keeps the capture.
. rs/roce_common.sh
# The stock tcpdump's AppArmor profile (tied to /usr/bin/tcpdump) blocks ibverbs, so use a copy;
# root-owned and outside /tmp, since it runs as root.
TCPDUMP=/usr/local/sbin/tcpdump-rdma
[ -x "$TCPDUMP" ] || sudo install -o root -g root -m 755 /usr/bin/tcpdump "$TCPDUMP" || exit 1

tag="$1_p$2${GPU:+_dgpu}"
pcap="$OUT/$tag.pcap"
csv="$OUT/$tag.csv"
rm -f "$pcap" "$csv"
sudo "$TCPDUMP" -i "$IBV" -s 256 -B 65536 -w "$pcap" udp port 4791 2>"$OUT/$tag.tcpdump.txt" &
capture=$!  # sudo's PID; sudo relays the SIGINT below to tcpdump
sleep 2
run_app "$OUT/$tag.log" "roce_d555_single.py --stream $1 --profile $2 --stats-every 0 --frame-limit $3 --timing-csv $csv"
sleep 1
sudo kill -INT "$capture"
wait "$capture"
line=$(results "$OUT/$tag.log") || failed=1
echo "$tag $line | $(grep -E 'captured|dropped by kernel' "$OUT/$tag.tcpdump.txt" | tr '\n' ' ')"
if [ "$failed" = 0 ]; then
    python3 rs/roce_latency.py "$pcap" --csv "$csv" $LATENCY_ARGS || failed=1
fi
[ -n "$KEEP_PCAP" ] || rm -f "$pcap"  # tens of MB each
exit "$failed"
