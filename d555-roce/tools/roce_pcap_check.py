#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Validate the D555e's HSB RoCEv2 packets in a capture.

Per QP (stream): ICRC (zlib crc32 over 8 x 0xFF + masked IPv4/UDP/BTH + rest), PSN continuity
(20-bit), RETH length vs payload, contiguous VAs within a frame, and on each end-of-frame packet
ImmDt == page | psn << 12 and metadata psn == BTH psn. ICRC needs whole packets (no -s snaplen).

    python3 roce_pcap_check.py capture.pcap [--verbose]
"""

import argparse
import sys
import zlib
from collections import Counter

import roce_pcap as rp

ERRORS = ("icrc_bad", "psn_gap", "length_bad", "va_bad", "imm_bad", "metadata_psn_bad",
          "other_opcode")


def icrc(ip):
    """RoCEv2 ICRC over an IPv4 packet without its trailing ICRC."""
    udp = (ip[0] & 0x0F) * 4  # IPv4 header length, including any options
    masked = bytearray(ip[:udp + 20])  # IPv4 + UDP (8) + BTH (12)
    masked[1] = 0xFF                       # TOS
    masked[8] = 0xFF                       # TTL
    masked[10:12] = b"\xff\xff"            # IPv4 checksum
    masked[udp + 6:udp + 8] = b"\xff\xff"  # UDP checksum
    masked[udp + 12] = 0xFF                # BTH resv8a
    return zlib.crc32(ip[udp + 20:], zlib.crc32(masked, zlib.crc32(b"\xff" * 8)))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pcap")
    parser.add_argument("--verbose", action="store_true", help="print every end-of-frame packet")
    args = parser.parse_args()

    counts = Counter()
    examples = []
    last_psn = {}   # qp -> PSN of its previous packet
    next_va = {}    # qp -> expected VA of the next data packet in the current frame

    def error(kind, text):
        counts[kind] += 1
        if len(examples) < 20:
            examples.append(text)

    for n, p in enumerate(rp.packets(args.pcap), 1):
        counts["packets"] += 1
        wire_icrc = int.from_bytes(p.ip[-4:], "little")
        if icrc(p.ip[:-4]) != wire_icrc:
            error("icrc_bad", f"pkt {n} qp 0x{p.qp:x} psn {p.psn}: ICRC 0x{wire_icrc:08x} wrong")
        if p.qp in last_psn and p.psn != (last_psn[p.qp] + 1) & rp.PSN_MASK:
            error("psn_gap", f"pkt {n} qp 0x{p.qp:x}: PSN {p.psn} after {last_psn[p.qp]}")
        last_psn[p.qp] = p.psn

        if p.opcode == rp.OP_WRITE_ONLY:
            counts["data"] += 1
            if p.dma_len != len(p.payload):
                error("length_bad", f"pkt {n}: RETH length {p.dma_len}, payload {len(p.payload)}")
            if p.qp in next_va and p.va != next_va[p.qp]:
                error("va_bad", f"pkt {n}: VA 0x{p.va:x}, expected 0x{next_va[p.qp]:x}")
            next_va[p.qp] = p.va + len(p.payload)
        elif p.opcode == rp.OP_WRITE_ONLY_IMM:
            counts["frames"] += 1
            next_va.pop(p.qp, None)  # the next frame starts at a new buffer
            if p.imm >> rp.IMM_PSN_SHIFT != p.psn & rp.PSN_MASK:
                error("imm_bad", f"pkt {n}: ImmDt 0x{p.imm:08x} vs PSN {p.psn}")
            if p.dma_len != len(p.payload):
                error("length_bad", f"pkt {n}: end RETH length {p.dma_len}, "
                                    f"metadata {len(p.payload)}")
            md = rp.metadata(p)
            if md is None or md.psn != p.psn:
                error("metadata_psn_bad", f"pkt {n}: metadata PSN {md and md.psn} vs {p.psn}")
            if args.verbose and md:
                print(f"frame end: qp 0x{p.qp:06x} psn {p.psn} page {p.imm & rp.IMM_PAGE_MASK} "
                      f"fn {md.frame_number} bytes {md.bytes_written}")
        else:
            error("other_opcode", f"pkt {n}: opcode 0x{p.opcode:02x}")

    for key in ("packets", "data", "frames") + ERRORS:
        print(f"{key:18s} {counts[key]}")
    for text in examples:
        print("  " + text)
    ok = counts["packets"] > 0 and not any(counts[k] for k in ERRORS)
    print("RESULT", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
