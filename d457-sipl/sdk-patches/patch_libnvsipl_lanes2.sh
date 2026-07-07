#!/bin/bash
# patch_libnvsipl_lanes2.sh — force the SIPL Tegra CSI lane count to 2 for the D457.
#
# WHY: SIPL couples csiPort -> {2x4-vs-4x2 capture descriptor, Tegra lane count}. The D457 needs the
# 2x4 capture descriptor (csi-ab) but only 2 lanes (the board wires 2; d4xx uses 2x4-mode+2-lane).
# csi-ab forces 4 Tegra lanes; the lane count is hard-derived in PRECOMPILED libnvsipl.so
# BuildSensorProperty from csiInterfaceType (NOT settable via query mipiSettings.lanes or our driver).
#
# WHAT: patch a COPY of libnvsipl.so (loaded via LD_LIBRARY_PATH; the SYSTEM lib is left untouched) so
# BuildSensorProperty reads lanes=2 unconditionally. Two `ldrb w,[x19,#256]` (the lane field at struct
# offset 256, set by the query parser) -> `mov w,#2`:
#   file off 0x4c8d4 : 66 02 44 39 -> 46 00 80 52   (functional read -> sensor property -> RCE/CIL)
#   file off 0x4c82c : 63 02 44 39 -> 43 00 80 52   (the log read; makes syslog print "lanes 2")
# This is UNCONDITIONAL (every camera via BuildSensorProperty gets 2 lanes) — fine for this
# D457-dedicated rig; would break a genuine 4-lane camera (HAWK). Verified: Tegra "Partition: CIL A,
# Lanes: 2", clean PHY, depth streams.
#
# USAGE: ./patch_libnvsipl_lanes2.sh   (run on the rig)
#   then: LD_LIBRARY_PATH=/home/mic-742/sipl_libs nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera
set -e
SYS=/usr/lib/aarch64-linux-gnu/nvidia/libnvsipl.so
DIR=/home/mic-742/sipl_libs
mkdir -p "$DIR"
[ -f "$DIR/libnvsipl.so.orig" ] || cp "$SYS" "$DIR/libnvsipl.so.orig"
cp "$SYS" "$DIR/libnvsipl.so"
C="$DIR/libnvsipl.so"
printf '\x46\x00\x80\x52' | dd of="$C" bs=1 seek=$((0x4c8d4)) conv=notrunc 2>/dev/null
printf '\x43\x00\x80\x52' | dd of="$C" bs=1 seek=$((0x4c82c)) conv=notrunc 2>/dev/null
echo "patched $C ; verify (expect 46008052 / 43008052):"
dd if="$C" bs=1 skip=$((0x4c8d4)) count=4 2>/dev/null | xxd
dd if="$C" bs=1 skip=$((0x4c82c)) count=4 2>/dev/null | xxd
echo "system lib untouched: $(md5sum "$SYS")"
