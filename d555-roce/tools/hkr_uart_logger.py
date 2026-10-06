#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Capture the D555e UART log to a file, flushing continuously (survives the port vanishing).

    python3 hkr_uart_logger.py OUT.log SECONDS [--setup "sysctl log -p uart" ...]

Setup commands are sent first (CRLF-terminated, 0.5 s apart), then the port is read until
the hard deadline or until the device disappears. Each line is prefixed with host time.
"""
import argparse
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("seconds", type=float)
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--setup", action="append", default=[])
    a = ap.parse_args()

    start = time.monotonic()
    deadline = start + a.seconds
    with open(a.out, "w", buffering=1) as out:
        try:
            s = serial.Serial(a.port, a.baud, timeout=0.2)
        except serial.SerialException as e:
            out.write(f"[host] open failed: {e}\n")
            return
        for c in a.setup:
            s.write((c + "\r\n").encode())
            s.flush()
            out.write(f"[host {time.monotonic() - start:8.3f}] >>> {c}\n")
            time.sleep(0.5)
        pending = ""
        while time.monotonic() < deadline:
            try:
                chunk = s.read(4096)
            except (serial.SerialException, OSError) as e:
                out.write(f"[host {time.monotonic() - start:8.3f}] port lost: {e}\n")
                break
            if not chunk:
                continue
            pending += chunk.decode(errors="replace").replace("\r", "")
            *lines, pending = pending.split("\n")
            for line in lines:
                out.write(f"[host {time.monotonic() - start:8.3f}] {line}\n")
        if pending:
            out.write(f"[host {time.monotonic() - start:8.3f}] {pending}\n")


if __name__ == "__main__":
    main()
