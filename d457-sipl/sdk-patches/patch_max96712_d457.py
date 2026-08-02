#!/usr/bin/env python3
"""
patch_max96712_d457.py — patch the stock NVIDIA SIPL MAX96712 deserializer driver so its
MIPI CSI-2 OUTPUT to the Thor matches the WORKING d4xx V4L2 path for the RealSense D457:
a 4-lane output on PHY0/1 (port A, PHY1 master) in 2x4 port mode @ 1100 Mbps/lane.

For simultaneous depth + RGB, the SAME 4-lane port A carries TWO CSI virtual channels (depth VC0,
RGB VC1) — the PHY/lane block below is unchanged. Following d4xx, each VC gets its OWN deserializer
pipe (depth pipe 0, RGB pipe 1) with its own pixel-map block and framing; both DSTs are RAW16
(DT 0x2E) so Tegra splits them into two SIPL capture pipelines (see NEW_MAP). An earlier single-pipe
map (both VCs through pipe 0) faulted at the Tegra VI after ~8 frames; pipe-per-VC is the fix.

WHY
  SIPL couples csiPort -> {PHY mode, Tegra lane count}:
    csi-ab -> 2x4 (4 Tegra lanes)   csi-a -> 4x2 (2 Tegra lanes)
  Since the 4-lane switch the D457 uses csi-ab and takes the 4 Tegra lanes as-is (the old
  patch_libnvsipl_lanes2.sh binary patch that forced 2 is retired). The deser output is the
  d4xx 2x4-port-mode config on PHY0/1 (the board is wired to the port-A 2x4 pads), now 4 lanes
  @ 1100 Mbps. The 4x2 PLL-lock check (which demanded all 4 PHYs locked, 0xF0) is still relaxed
  to the PHY1 bit since only PHY0/1 are enabled.

  ⚠ This file is the LEGACY single-camera generator. The live multi-camera source of truth is
  sdk-patches/multicam-sources/MAX967XXHsl.py (function seq_set_mipi_d_phy, the csi-ab/2x4 path);
  keep the two in sync.

GROUND TRUTH (d4xx max96712_init_settings() with lane_cnt=4 -- realsense_mipi_platform_driver
commit 758440a "RSDSO-21762 Align all 96712 overlays to 4 MIPI lanes"; the 2-lane values that
this config replaced are in parentheses):
  0x08A0=0x24  PHY enable + force_clk0 (port A clock)
  0x094A=0xD0  PHYA TX10: (lane_cnt-1)<<6 | ext-VC<<4 -> 4 MIPI lanes    (2-lane was 0x50)
  0x08A3=0xE4  lane->pad mapping (lower phys to lower lanes)
  0x0418=0x39  port-A: PHY1_SW_OVRR (1<<5) | rate 0x19 = 2500 Mbps       (d4xx 4-lane uses 0x2B/1100)
  0x08A2=0x34  2x4 port mode: enable PHY0/1, disable PHY2/3
  0x092D=0x55  pipe->PHY1 (set via desTxPort=1 / pipeline mapping; ours reads 0x15, bits[1:0]=PHY1)

  The matching Tegra side is the query's mipiSettings {dphyRate: 2500000, lanes: 4}. Deser rate and
  NVCSI rate MUST agree -- a mismatch truncates every frame (FINDINGS §5o). d4xx pairs 4 lanes with
  1100 Mbps (serdes_pix_clk_hz=275000000 x 16 bpp / 4 lanes); we run 2500 (the MAX96712 D-PHY max)
  after an on-rig sweep found 1100/1500/2000/2500 all clean -- see the 2026-08-02 FINDINGS entry.

USAGE: python3 patch_max96712_d457.py [SIPL_ROOT]
"""
import io, os, re, sys

SIPL_ROOT = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl"
HSL_4X2 = os.path.join(SIPL_ROOT, "uddf/drivers/deserializers/MAX96712/MAX96712Hsl.py")
HSL_PLL = os.path.join(SIPL_ROOT, "uddf/drivers/deserializers/MAX967XX/MAX967XXHsl.py")
CPP_967XX = os.path.join(SIPL_ROOT, "uddf/drivers/deserializers/MAX967XX/MAX967XX.cpp")

NEW_4X2_BODY = '''    def seq_set_mipi_d_phy_4x2(self):
        with Sequence('set_mipi_d_phy_4x2'):
            annotate('D457: d4xx-exact 2x4-port-mode, 4-lane on PHY0/1 (port A) @ 1100 Mbps')
            with self.max967xx:
                write(0x08A0, 0x24) # PHY enable + force_clk0 (port A clock)
                write(0x094A, 0xD0) # PHYA TX10: 4 MIPI lanes + master/ext-VC (2-lane was 0x50)
                write(0x08A3, 0xE4) # lane->pad mapping (lower phys to lower lanes)
                write(0x0973, 0x10) # ALT2_MEM_MAP8 (d4xx)
                write(0x1C00, 0xF4) # DPLL (PHY0 clock) reset
                write(0x1D00, 0xF4) # DPLL (PHY1) reset for rate change
                write(0x0418, 0x39) # port A lane rate: 0x20 (PHY1 ovrr) | 0x19 = 2500 Mbps (max)
                write(0x1C00, 0xF5) # DPLL (PHY0 clock) fix
                write(0x1D00, 0xF5) # DPLL (PHY1) fix frequency
                write(0x08A2, 0x34) # 2x4 port mode: PHY0/1 on, PHY2/3 off
'''

def patch_4x2():
    with io.open(HSL_4X2, encoding="utf-8") as f:
        s = f.read()
    if "d4xx-exact 2x4-port-mode" in s:
        return "4x2: already patched"
    # Replace the whole seq_set_mipi_d_phy_4x2 function body (up to the next 'def ')
    pat = re.compile(r"    def seq_set_mipi_d_phy_4x2\(self\):.*?(?=\n    def )", re.DOTALL)
    if not pat.search(s):
        return "4x2: FUNCTION NOT FOUND"
    s = pat.sub(NEW_4X2_BODY.rstrip("\n") + "\n", s, count=1)
    with io.open(HSL_4X2, "w", encoding="utf-8") as f:
        f.write(s)
    return "4x2: patched"

def patch_pll():
    with io.open(HSL_PLL, encoding="utf-8") as f:
        s = f.read()
    old = "        with Sequence('check_csi_pll_lock_4x2'):\n            annotate('Check CSI PLL Lock 4x2')\n            with self.max967xx:\n                poll(0x0400, 0xF0, 0xF0, 10000, 20)"
    new = "        with Sequence('check_csi_pll_lock_4x2'):\n            annotate('Check CSI PLL Lock 4x2 (D457: only PHY1 PLL locks once PHY2/3 disabled -> bit5)')\n            with self.max967xx:\n                poll(0x0400, 0x20, 0x20, 10000, 20)"
    if "PHY0/1 only" in s:
        return "PLL: already patched"
    if old not in s:
        return "PLL: PATTERN NOT FOUND"
    s = s.replace(old, new, 1)
    with io.open(HSL_PLL, "w", encoding="utf-8") as f:
        f.write(s)
    return "PLL: patched"

OLD_MAP = '''                write(0x090B, 0x07)
                writeFromMemory(0x092D, self.video_pipeline_mapping_link_a_block.reg_092D)
                write(0x090D, 0x1e)
                write(0x090E, 0x1e)
                write(0x090F, 0x00)
                write(0x0910, 0x00)
                write(0x0911, 0x01)
                write(0x0912, 0x01)'''

# d4xx __max96712_set_pipe(): the KEY unconditional fix is VIDEO_RX0 0x0100=0x23
# (DIS_PKT_DET + SEQ_MISS_EN disabled). SIPL stock left packet-detection ON (0x0100=0x22),
# which DROPS the DS5 stream -> deser forwards nothing -> 0 frames.
#
# PIPE-PER-VC pixel-map for SIMULTANEOUS depth (VC0) + RGB (VC1) + IR (VC2). d4xx routes EACH virtual
# channel through its OWN deserializer pipe (one per VC), each pipe with its own pixel-map block and its
# own framing (frame-start/frame-end) counters. The EARLIER single-pipe map crammed both VCs into pipe 0;
# two VCs sharing one pipe's framing faulted at the Tegra VI after ~8 frames (ChanselFault). This
# replicates d4xx: depth on deser pipe 0, RGB on deser pipe 1, IR on deser pipe 2 (each bound to the
# matching ser pipe X/Y/Z). VALIDATED on-rig 2026-06-25: all 3 sustain 30 fps, 0 drops, 0 faults.
#
# MAX96712 map regs are strided +0x40 per pipe (pipe0 base 0x090B, pipe1 base 0x094B). MAP_SRC/MAP_DST
# are 8-bit {VC[7:6], DT[5:0]}; the map matches incoming {VC,DT}, can rewrite the DT, and PRESERVES the
# VC so Tegra demuxes by VC. Both streams reach Tegra as RAW16 (DT 0x2E) — the SIPL ICP drops YUV422.
#
# PIPE 0 (base 0x090B) — VC0 traffic:
#   slot0 (0x090D/0x090E) = 0x2E -> 0x2E : depth data       (VC0, 0x2E pass-through)
#   slot1 (0x090F/0x0910) = 0x1E -> 0x2E : single-stream RGB-on-VC0 DEBUG (VC0, 0x1E -> 0x2E).
#         Harmless in dual mode (RGB rides VC1, so no VC0/0x1E arrives); lets the same config also
#         serve single-stream RGB-on-VC0 debug runs without a re-patch.
#   slot2 (0x0911/0x0912) = 0x00 -> 0x00 : VC0 frame-start
#   slot3 (0x0913/0x0914) = 0x01 -> 0x01 : VC0 frame-end
#   0x090B = MAP_EN (0x0F = slots 0-3); 0x092D = dest-PHY slots 0-3 = 0x55 (all PHY1); 0x0100 = VIDEO_RX0.
#
# PIPE 1 (base 0x094B = 0x090B + 0x40) — VC1 traffic (RGB):
#   slot0 (0x094D/0x094E) = 0x5E -> 0x6E : RGB data         (VC1, 0x1E -> 0x2E; 0x40|0x1E -> 0x40|0x2E)
#   slot1 (0x094F/0x0950) = 0x40 -> 0x40 : VC1 frame-start  (0x40|0x00)
#   slot2 (0x0951/0x0952) = 0x41 -> 0x41 : VC1 frame-end    (0x40|0x01)
#   0x094B = MAP_EN (0x07 = slots 0-2); 0x096D = dest-PHY = 0x55; 0x0112 = VIDEO_RX0 pipe1 (0x0100+0x12);
#   0x0810-0x0813 = TX_EXT0-3 pipe1 = 0x00 (vc_msb=0, since VC1 < 4).
#
# PIPE 2 (base 0x098B = 0x090B + 0x80) — VC2 traffic (IR, 0x2E pass-through):
#   slot0 (0x098D/0x098E) = 0xAE -> 0xAE : IR data  (VC2/0x2E; 0x80|0x2E pass-through, VC preserved)
#   slot1 (0x098F/0x0990) = 0x80 -> 0x80 : VC2 frame-start  (0x80|0x00)
#   slot2 (0x0991/0x0992) = 0x81 -> 0x81 : VC2 frame-end    (0x80|0x01)
#   0x098B = MAP_EN (0x07); 0x09AD = dest-PHY = 0x55; 0x0124 = VIDEO_RX0 pipe2 (0x0100 + 0x12*2);
#   0x0820-0x0823 = TX_EXT0-3 pipe2 = 0x00 (vc_msb=0, since VC2 < 4). IR rides RAW16 0x2E (DS5 dtOut),
#   so pipe2 is a pure pass-through (no DT rewrite), unlike RGB's 0x1E->0x2E remap on pipe1.
#
# PIPE ENABLE + BIND (framework-coupled — VERIFY baseline on first rig run):
#   0x00F4 |= (1<<pipe): VIDEO_PIPE_EN — enable pipe0+pipe1+pipe2 (0x07).
#   0x00F0/0x00F1 : VIDEO_PIPE_SEL — bind dser pipe -> (link<<2)|ser_pipe, 4 bits/pipe, two pipes/reg.
#             0x00F0 = pipe0 (low,(link0,ser0)=0x0) | pipe1 (high,(link0,ser1)=0x1) => 0x10.
#             0x00F1 = pipe2 (low,(link0,ser2)=0x2) | pipe3 (high, unused, preserved 0xE) => 0xE2.
#   The single-sensor framework path (patch_force_single) configures pipe 0; pipes 1 and 2's blocks,
#   enables, and binds are programmed explicitly here because that path does not allocate extra pipes.
NEW_MAP = '''                write(0x090B, 0x0F)
                write(0x092D, 0x55)
                write(0x090D, 0x2e)
                write(0x090E, 0x2e)
                write(0x090F, 0x1e)
                write(0x0910, 0x2e)
                write(0x0911, 0x00)
                write(0x0912, 0x00)
                write(0x0913, 0x01)
                write(0x0914, 0x01)
                write(0x0100, 0x23)
                write(0x094B, 0x07)
                write(0x096D, 0x55)
                write(0x094D, 0x5e)
                write(0x094E, 0x6e)
                write(0x094F, 0x40)
                write(0x0950, 0x40)
                write(0x0951, 0x41)
                write(0x0952, 0x41)
                write(0x0810, 0x00)
                write(0x0811, 0x00)
                write(0x0812, 0x00)
                write(0x0813, 0x00)
                write(0x0112, 0x23)
                write(0x098B, 0x07)
                write(0x09AD, 0x55)
                write(0x098D, 0xae)
                write(0x098E, 0xae)
                write(0x098F, 0x80)
                write(0x0990, 0x80)
                write(0x0991, 0x81)
                write(0x0992, 0x81)
                write(0x0820, 0x00)
                write(0x0821, 0x00)
                write(0x0822, 0x00)
                write(0x0823, 0x00)
                write(0x0124, 0x23)
                write(0x00F4, 0x07)
                write(0x00F0, 0x10)
                write(0x00F1, 0xE2)'''

def patch_mapping():
    with io.open(HSL_PLL, encoding="utf-8") as f:
        s = f.read()
    if "write(0x0100, 0x23)" in s:
        return "MAP: already patched"
    if OLD_MAP not in s:
        return "MAP: PATTERN NOT FOUND"
    s = s.replace(OLD_MAP, NEW_MAP, 1)
    with io.open(HSL_PLL, "w", encoding="utf-8") as f:
        f.write(s)
    return "MAP: patched (pipe-per-VC: pipe0=depth VC0, pipe1=RGB VC1 + VIDEO_RX0=0x23, d4xx-exact)"

# --- Force the SINGLE-sensor pipeline-mapping path even for the 2-sensor (depth+RGB) config ---
# When numSensors==2, the stock driver auto-selects ConfigureDualSensorPipelineMappingForLink, which
# assumes TWO GMSL streams (pipes X,Y) sharing ONE data type and only supports RAW10/12 -> it rejects
# our RAW16 ("unsupported data type 4") and cannot express depth(VC0/0x2E)+RGB(VC1/0x1E->0x2E). The
# D457 emits BOTH VCs from ONE DS5 ASIC on ONE GMSL stream, which the SINGLE-sensor path handles via
# the 7-slot VC-aware pixel-map above (set_ser_video_pipeline_mapping_A). Force it by making
# IsDualSensorConfig() return false (flips both call sites: VIDEO_PIPE_EN + mapping dispatch).
# VERIFIED on-rig 2026-06-24: with this, both pipelines init, both DS5 streams play, depth captures
# real content on VC0. (RGB VC1 pixel delivery is a separate open item — see FINDINGS.)
OLD_DUALCFG = '''    return linkIndex < MAX_DESERIALIZER_LINKS &&
           context.initConfig.numSensorsPerLink[linkIndex] == 2U;'''
NEW_DUALCFG = '''    // D457: one DS5 ASIC emits depth(VC0)+RGB(VC1) on ONE GMSL stream demuxed by VC, NOT two
    // GMSL streams like HAWK. Force the single-sensor pipe/mapping path (pipe N for link N)
    // everywhere; the single-sensor HSL map is patched to demux both VCs.
    (void)context; (void)linkIndex;
    return false;'''

def patch_force_single():
    with io.open(CPP_967XX, encoding="utf-8") as f:
        s = f.read()
    if "D457: one DS5 ASIC emits depth(VC0)+RGB(VC1) on ONE GMSL stream" in s:
        return "FORCE-SINGLE: already patched"
    if s.count(OLD_DUALCFG) != 1:
        return "FORCE-SINGLE: anchor count %d (expected 1) — NOT patched" % s.count(OLD_DUALCFG)
    s = s.replace(OLD_DUALCFG, NEW_DUALCFG, 1)
    with io.open(CPP_967XX, "w", encoding="utf-8") as f:
        f.write(s)
    return "FORCE-SINGLE: patched IsDualSensorConfig->false"

if __name__ == "__main__":
    print(patch_4x2())
    print(patch_pll())
    print(patch_mapping())
    print(patch_force_single())
    print("DONE")
