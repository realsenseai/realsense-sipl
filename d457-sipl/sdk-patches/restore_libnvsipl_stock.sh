#!/bin/bash
# restore_libnvsipl_stock.sh — undo the retired lanes=2 binary patch of libnvsipl.so.
#
# HISTORY: until the 4-lane switch, the D457 needed the 2x4 capture descriptor (csiPort "csi-ab")
# but only 2 Tegra CSI lanes, to match the d4xx V4L2 path. libnvsipl.so's BuildSensorProperty
# hard-derives the lane count from csiInterfaceType (csi-ab -> 4) and ignores the query's
# mipiSettings.lanes, so a COPY of the library was binary-patched (two `ldrb w,[x19,#256]` ->
# `mov w,#2` at file offsets 0x4c8d4 / 0x4c82c) and loaded via LD_LIBRARY_PATH=~/sipl_libs.
#
# That patch is now GONE: the deserializer outputs 4 lanes @ 1100 Mbps (d4xx lane_cnt=4 config,
# realsense_mipi_platform_driver 758440a), so csi-ab's native 4 lanes are exactly what we want.
# The 2-lane path was the 8x720p30 bandwidth ceiling (2 x 594 Mbps = 1.19 Gbps vs 4 x 1100 = 4.4).
#
# WHAT THIS DOES: puts a pristine copy of the system libnvsipl.so back into ~/sipl_libs so any
# leftover LD_LIBRARY_PATH in an old script is harmless. The system library was never modified.
# Preferred: drop LD_LIBRARY_PATH=~/sipl_libs from the launch line entirely.
#
# USAGE: ./restore_libnvsipl_stock.sh   (run on the rig)
set -e
SYS=/usr/lib/aarch64-linux-gnu/nvidia/libnvsipl.so
DIR=${1:-$HOME/sipl_libs}
if [ ! -d "$DIR" ]; then
    echo "no $DIR — nothing to restore (already clean)"
    exit 0
fi
cp "$SYS" "$DIR/libnvsipl.so"
rm -f "$DIR/libnvsipl.so.orig"
echo "restored stock libnvsipl.so into $DIR"
md5sum "$SYS" "$DIR/libnvsipl.so"
echo "^ the two md5s must match (no patch in effect)"
