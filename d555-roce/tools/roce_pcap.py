# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Streaming pcap/pcapng reader and HSB RoCEv2 packet parser (standard library only).

HSB >= 2.6 data plane (what the D555e firmware sends):
  data  opcode 0x2A  UC RDMA WRITE Only:           IPv4 | UDP 4791 | BTH | RETH | payload | ICRC
  end   opcode 0x2B  ... with Immediate:           ... | RETH | ImmDt | 128 B metadata | ICRC
  ImmDt = page[11:0] | psn[19:0] << 12; the PSN wraps at 20 bits.
"""

import struct
from collections import namedtuple

OP_WRITE_ONLY = 0x2A
OP_WRITE_ONLY_IMM = 0x2B
ROCE_UDP_PORT = 4791
PSN_MASK = 0xFFFFF
IMM_PSN_SHIFT = 12
IMM_PAGE_MASK = 0xFFF

_BTH_RETH = struct.Struct(">B3xIIQII")  # opcode, (resv8a|QP), PSN word, VA, rkey, DMA length
# HSB metadata: flags, psn, crc, timestamp s/ns (ISYS), bytes_written, frame_number, metadata s/ns
_METADATA = struct.Struct(">IIIQIQIQI")

# ip: the IPv4 packet (header .. ICRC), for ICRC checks; payload: after RETH (and ImmDt)
Packet = namedtuple("Packet", "t_ns opcode qp psn va rkey dma_len imm payload ip")
Metadata = namedtuple("Metadata", "psn frame_number isys_ns tx_ns bytes_written")


def read_pcap(path):
    """Yield (t_ns, frame) for each Ethernet frame of a pcap or pcapng file."""
    with open(path, "rb") as f:
        magic = f.read(4)
        if magic == b"\x0a\x0d\x0d\x0a":
            yield from _read_pcapng(f, magic)
            return
        formats = {b"\xd4\xc3\xb2\xa1": ("<", 1000), b"\x4d\x3c\xb2\xa1": ("<", 1),
                   b"\xa1\xb2\xc3\xd4": (">", 1000), b"\xa1\xb2\x3c\x4d": (">", 1)}
        if magic not in formats:
            raise SystemExit(f"{path}: not a pcap/pcapng file")
        endian, ns_per_tick = formats[magic]
        header = f.read(20)
        if struct.unpack(endian + "I", header[16:20])[0] != 1:
            raise SystemExit(f"{path}: not an Ethernet capture")
        record = struct.Struct(endian + "IIII")
        while len(raw := f.read(16)) == 16:
            sec, frac, caplen, _ = record.unpack(raw)
            yield sec * 1_000_000_000 + frac * ns_per_tick, f.read(caplen)


def _read_pcapng(f, magic):
    endian = "<"
    ns_per_tick = 1000  # default if_tsresol is microseconds
    block = magic + f.read(8)
    while len(block) == 12:
        block_type = struct.unpack(endian + "I", block[:4])[0]
        if block_type == 0x0A0D0D0A:  # section header: byte order
            endian = "<" if block[8:12] == b"\x4d\x3c\x2b\x1a" else ">"
        length = struct.unpack(endian + "I", block[4:8])[0]
        body = block[8:] + f.read(length - 12)
        if block_type == 6:  # enhanced packet block
            ts_high, ts_low, caplen = struct.unpack(endian + "III", body[4:16])
            yield ((ts_high << 32) | ts_low) * ns_per_tick, body[20:20 + caplen]
        elif block_type == 3:  # simple packet block
            yield 0, body[4:-4]
        block = f.read(12)


def parse(t_ns, frame):
    """Packet for an IPv4 RoCEv2 frame, else None."""
    if len(frame) < 14 + 20 + 8 + 28 or frame[12:14] != b"\x08\x00" or frame[23] != 17:
        return None
    ip_start = 14
    udp = ip_start + (frame[ip_start] & 0x0F) * 4
    if frame[udp + 2:udp + 4] != b"\x12\xb7":  # dst port 4791
        return None
    ip_end = ip_start + struct.unpack(">H", frame[ip_start + 2:ip_start + 4])[0]
    opcode, qp, psn, va, rkey, dma_len = _BTH_RETH.unpack_from(frame, udp + 8)
    payload = udp + 8 + _BTH_RETH.size
    imm = None
    if opcode == OP_WRITE_ONLY_IMM:
        imm = struct.unpack_from(">I", frame, payload)[0]
        payload += 4
    return Packet(t_ns, opcode, qp & 0xFFFFFF, psn & 0xFFFFFF, va, rkey, dma_len, imm,
                  frame[payload:ip_end - 4], frame[ip_start:ip_end])


def packets(path):
    """Yield every RoCEv2 Packet in a capture."""
    for t_ns, frame in read_pcap(path):
        packet = parse(t_ns, frame)
        if packet is not None:
            yield packet


def metadata(packet):
    """Metadata of an end-of-frame (0x2B) packet, or None if the capture cut it short."""
    if len(packet.payload) < _METADATA.size:
        return None
    _, psn, _, ts_s, ts_ns, size, frame_number, md_s, md_ns = _METADATA.unpack_from(packet.payload)
    return Metadata(psn, frame_number, ts_s * 1_000_000_000 + ts_ns,
                    md_s * 1_000_000_000 + md_ns, size)
