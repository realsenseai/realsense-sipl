# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
# Shared settings and helpers for the D555e RoCE scripts (sourced, POSIX sh).
# Run from the HSB 2.7 tree root after install.sh put apps/ and tools/ into ./rs/; the container
# runs with docker -it, so the calling shell needs a TTY (ssh -tt).
IF=${IF:-enP4p3s0f1np1}     # CX-7 netdev facing the camera
IBV=${IBV:-roceP4p3s0f1}    # its RDMA device
GPU=${GPU:-}                # CUDA_VISIBLE_DEVICES value; a dGPU UUID = GPUDirect into VRAM
EXTRA=${EXTRA:-}            # extra app arguments, e.g. "--stats-every 0"
OUT=${OUT:-rs/results}
mkdir -p "$OUT"
failed=0                    # callers: line=$(results "$log") || failed=1; exit "$failed"

rdma_packets() { ethtool -S "$IF" | awk '/rx_vport_rdma_unicast_packets/ {print $2}'; }

# run_app <log> <app.py> [app args...]: one container session; app output to <log>, docker's own
# output to <log>.docker, exit status in $app_status. The old log is removed first so a run that
# never started can't report the previous run's RESULT.
run_app() {
    log=$1
    shift
    rm -f "$log" "$log.docker"
    sudo sh docker/demo.sh sh -c \
        "python3 rs/$* --ibv-name $IBV ${GPU:+--gpu $GPU} $EXTRA > $log 2>&1" >"$log.docker" 2>&1
    app_status=$?
}

# results <log>: the app's RESULT line(s) on one line, or FAIL with the first error; returns 1 on
# a non-zero exit (a RESULT with status=FAIL, a failed stop, docker not starting) or no RESULT.
results() {
    r=$(grep -a -o 'RESULT.*' "$1" 2>/dev/null | tr '\n' ' ')
    if [ "$app_status" != 0 ] || [ -z "$r" ]; then
        err=$(grep -a -h -m1 -E 'rror|Traceback|not a TTY' "$1" "$1.docker" 2>/dev/null | head -1)
        echo "FAIL exit=$app_status ${r:-no RESULT} ${err}"
        return 1
    fi
    echo "$r"
}

# expected_packets <log>: RDMA packets for the frames the app received, all streams:
# frames x (ceil(frame_bytes / HSB payload size) + 1 metadata packet), per RESULT line.
expected_packets() {
    [ -f "$1" ] || { echo 0; return; }
    awk '/compute_payload_size/ && !payload { match($0, /payload_size=[0-9]+/); payload = substr($0, RSTART + 13, RLENGTH - 13) }
         /RESULT/ { match($0, / frames=[0-9]+/); f = substr($0, RSTART + 8, RLENGTH - 8)
                    match($0, /frame_bytes=[0-9]+/); b = substr($0, RSTART + 12, RLENGTH - 12)
                    if (payload) total += f * (int((b + payload - 1) / payload) + 1) }
         END { print total + 0 }' "$1"
}
