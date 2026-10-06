#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""D555e RoCE latency breakdown from an RDMA-sniffer capture + the app's timing CSV.

The camera clock isn't synced to the host (no PTP). The firmware stamps metadata_s/ns just before
a frame's first packet, so min over frames of (host arrival of that packet - metadata time) maps
the camera clock onto the host's. That treats the fastest camera->host path as zero and applies
no drift correction, so camera->host figures are lower bounds, low by up to some tens of µs:
fine for ms-level medians. Metrics per frame (ms):

  isys->tx    camera ISYS time -> first packet sent            (camera clock)
  tx jitter   first-packet delay above the minimum
  wire        first packet -> end-of-frame packet on the host
  isys->host  frame complete in host/GPU memory
  cq          CQ completion - sniffer time of the end packet   (needs --csv)
  app         CQ completion -> Holoscan operator               (needs --csv)
  isys->app   end to end                                       (needs --csv)

Capture (the stock tcpdump's AppArmor profile blocks ibverbs, so use a root-owned copy; µs
timestamps; roce_latency.sh does all of this):
    sudo install -o root -g root -m 755 /usr/bin/tcpdump /usr/local/sbin/tcpdump-rdma
    sudo /usr/local/sbin/tcpdump-rdma -i roceP4p3s0f1 -s 256 -B 65536 -w lat.pcap udp port 4791
"""

import argparse
import csv
import statistics

import roce_pcap as rp


def frames(path):
    """Per completed frame: qp, packet count, first/end arrival (host ns) + camera metadata."""
    result = []
    in_progress = {}  # qp -> [t_first, packets]
    for p in rp.packets(path):
        if p.opcode == rp.OP_WRITE_ONLY:
            in_progress.setdefault(p.qp, [p.t_ns, 0])[1] += 1
        elif p.opcode == rp.OP_WRITE_ONLY_IMM:
            started = in_progress.pop(p.qp, None)
            md = rp.metadata(p)
            if started and md:
                result.append(dict(qp=p.qp, t_first=started[0], t_end=p.t_ns,
                                   packets=started[1] + 1, isys=md.isys_ns, tx=md.tx_ns,
                                   frame_number=md.frame_number & 0xFFFF))  # HSB: low 16 bits
    return result


def read_app_times(path):
    """frame_number -> (CQ completion ns, operator ns) from roce_d555_single.py --timing-csv."""
    times = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            try:
                received = int(row["received_s"]) * 1_000_000_000 + int(row["received_ns"])
                times[int(row["frame_number"])] = (received, int(row["app_ns"]))
            except (ValueError, KeyError):
                continue
    return times


def metrics(qp_frames, app_times):
    """name -> list of ns values; also returns the clock offset (ns)."""
    offset = min(f["t_first"] - f["tx"] for f in qp_frames)
    m = {
        "isys->tx": [f["tx"] - f["isys"] for f in qp_frames],
        "tx jitter": [f["t_first"] - f["tx"] - offset for f in qp_frames],
        "wire": [f["t_end"] - f["t_first"] for f in qp_frames],
        "isys->host": [f["t_end"] - f["isys"] - offset for f in qp_frames],
    }
    joined = [(f, app_times[f["frame_number"]]) for f in qp_frames if f["frame_number"] in app_times]
    if joined:
        m["cq"] = [received - f["t_end"] for f, (received, _) in joined]
        m["app"] = [app - received for _, (received, app) in joined]
        m["isys->app"] = [app - f["isys"] - offset for f, (_, app) in joined]
    return m, offset


def percentiles(values_ns):
    """(median, p99, min, max) in ms."""
    v = sorted(x / 1e6 for x in values_ns)
    return statistics.median(v), v[round(0.99 * (len(v) - 1))], v[0], v[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pcap")
    parser.add_argument("--csv", help="app timing CSV (roce_d555_single.py --timing-csv)")
    parser.add_argument("--skip", type=int, default=5, help="ignore the first N frames per QP")
    parser.add_argument("--oneline", action="store_true",
                        help="one line per QP, medians (p99) in ms, for result tables")
    args = parser.parse_args()

    app_times = read_app_times(args.csv) if args.csv else {}
    by_qp = {}
    measured = 0
    for f in frames(args.pcap):
        by_qp.setdefault(f["qp"], []).append(f)

    for qp, qp_frames in sorted(by_qp.items()):
        qp_frames = qp_frames[args.skip:]
        if not qp_frames:
            continue
        # A missed end packet merges two frames and skews the offset by a frame period,
        # so keep only frames with the usual packet count.
        usual = statistics.mode(f["packets"] for f in qp_frames)
        complete = [f for f in qp_frames if f["packets"] == usual]
        if len(complete) < len(qp_frames):
            print(f"QP 0x{qp:06x}: ignored {len(qp_frames) - len(complete)} incomplete frames")
        m, offset = metrics(complete, app_times)
        measured += 1

        if args.oneline:
            cells = [f"{name.replace(' ', '_')}={p[0]:.2f}({p[1]:.2f})"
                     for name, p in ((n, percentiles(v)) for n, v in m.items())
                     if name not in ("tx jitter", "cq")]
            print(f"frames={len(complete)} pkts/frame={usual} " + " ".join(cells))
            continue
        print(f"QP 0x{qp:06x}: {len(complete)} frames, {usual} packets/frame, "
              f"camera->host clock offset {offset / 1e9:.6f} s")
        for name, values in m.items():
            median, p99, low, high = percentiles(values)
            print(f"  {name:<11} median {median:8.3f}  p99 {p99:8.3f}  min {low:8.3f}  "
                  f"max {high:8.3f}  ms  (n={len(values)})")
        if args.csv and "app" not in m:
            print("  (no CSV rows matched this QP's frame numbers)")
    if not measured:
        print(f"no frames measured in {args.pcap} (empty, not RoCE, or no more than "
              f"--skip {args.skip} frames per QP)")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
