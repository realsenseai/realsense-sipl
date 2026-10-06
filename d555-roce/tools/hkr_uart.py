#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""Send HKR CLI commands over the D555e UART and print the replies.

Runs ON THE RIG. The UART (CP2105 port 0, 460800 8N1) is the CLI that survives
Ethernet mode and does not go through the camera's USB stack.

    python3 hkr_uart.py [--port /dev/ttyUSB0] [--quiet-logs] [--wait S] CMD [CMD ...]

Each CMD is sent followed by CRLF; output is read until a HARD deadline of --wait
seconds after the command (the log stream never goes idle, so no idle timeout).
"""
import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--wait", type=float, default=2.0, help="seconds to read after each command")
    ap.add_argument("--quiet-logs", action="store_true", help="send 'log err' first")
    ap.add_argument("cmds", nargs="*")
    a = ap.parse_args()

    s = serial.Serial(a.port, a.baud, timeout=0.1)
    cmds = (["log err"] if a.quiet_logs else []) + a.cmds
    s.reset_input_buffer()
    for c in cmds:
        s.write((c + "\r\n").encode())
        s.flush()
        deadline = time.monotonic() + a.wait
        buf = bytearray()
        while time.monotonic() < deadline:
            buf += s.read(4096)
        sys.stdout.write(f"===== {c}\n")
        sys.stdout.write(buf.decode(errors="replace"))
        sys.stdout.write("\n")
    s.close()


if __name__ == "__main__":
    main()
