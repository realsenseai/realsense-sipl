/*
 * Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
 *
 * d457_ds5_registers.h
 *
 * DS5 ASIC (D457) register definitions for the SIPL UDDF GMSL camera-module driver.
 *
 * Ported from the known-good d4xx Hololink path; the register tables live in this header now.
 * and reconciled register-for-register against the two ground-truth sources:
 *   - d4xx V4L2 GMSL driver:  kernel/realsense/d4xx.c
 *   - DS5 B0 firmware:        DS5_B0_DEV/AppServicesLib/{I2cSlaveHostIf,FlowDepth,FlowRgb}.c
 *
 * SCOPE: DS5-ASIC-side register writes only (mux @ 0x1A). The MAX9295 (serializer) and MAX96712
 * (deserializer) link bring-up is handled by NVIDIA's UDDF drivers (+ our sdk-patches/).
 *
 * ── Per-stream config-struct layout (firmware: I2cSlaveHostIf.c STStreamConfiguration, 0x20 B) ──
 * One config-struct array based at 0x4000, indexed by streamId (each entry 0x20 bytes):
 *   streamId 0 = DEPTH @ 0x4000   streamId 1 = RGB @ 0x4020   streamId 2 = IMU @ 0x4040
 *   streamId 4 = IR/Y  @ 0x4080
 * Field offsets within a struct (same for every stream):
 *   +0x00 dt (byte0) | +0x02 metaDataType (byte0) + vc (byte1)  ← write as one 16-bit word
 *   +0x04 width | +0x08 height | +0x0C fps | +0x1C dtOut (CSI output-DT override) | +0x1E control_status
 * Stream control register 0x1000 = {byte0 streamId, byte1 cmd}; cmd 2=play, 1=stop, 0=ignore.
 * Commanded-state status array 0x1004 + 4*streamId (depth 0x1004, RGB 0x1008, IR 0x1014).
 *
 * ── SIPL RAW16 routing (the depth solve, reused for all streams) ──
 * SIPL's ICP captures only "raw" MIPI datatypes; it silently drops YUV422 (DT 0x1E). So every
 * stream is delivered to Tegra as RAW16 (DT 0x2E), captured as width*height*2 bytes:
 *   - DEPTH: dtOut 0x401C = 0x2E  (DS5 emits 0x2E directly).
 *   - IR/Y : dtOut 0x409C = 0x2E  (DS5 emits 0x2E directly; internal pipe = Y8I/R8L8, 16 bpp).
 *   - RGB  : NO dtOut (firmware ignores RGB dtOut); DS5 emits native YUV422 (DT 0x1E, 16 bpp);
 *            the MAX96712 deserializer is patched with an extra pixel-map SRC 0x1E -> DST 0x2E
 *            so SIPL still captures it as RAW16. See sdk-patches/patch_max96712_d457.py.
 * The consumer reinterprets the RAW16 bytes per stream: Z16 (depth, byte-swapped LE), YUYV (RGB),
 * interleaved left/right Y8 (IR).
 *
 * ── Resolution is PARAMETRIC (no per-resolution tables) ──
 * The ONLY registers that change with resolution are the width/height/fps config-struct words
 * (+ the per-stream dt/dtOut/VC, which are fixed per stream type). So the mode-config sequence is
 * BUILT at runtime in D457Sensor.cpp (BuildModeTable) from m_width/m_height/m_fps — which flow from
 * the query (sensorInfoList[i].resolution). ANY DS5-supported mode works with no new table here, and
 * resolution can be changed without recompiling (D457_WIDTH/D457_HEIGHT/D457_FPS env -> query JSON ->
 * SIPL parse -> driver). Only DS5-supported (width,height,fps) combos stream; bandwidth-heavy modes
 * (1080p, 60fps) may need the rate set raised together — DS5 0x0402, deser 0x0418+DPLL, query dphyRate
 * (the 4x code-vs-actual mismatch is what truncated frames during the depth solve).
 *
 * ── VIRTUAL-CHANNEL ASSIGNMENT (single-stream vs simultaneous) ──
 * Each stream rides its CANONICAL VC (depth VC0, RGB VC1, IR VC2), fixed by the SerDes pipe DT
 * filters after the pipe-per-VC fix. The mode word at +0x02 = (vc<<8)|md_fmt, md_fmt=0 (no embedded
 * metadata line). The SerDes carry one pipe per VC; the query declares one virtualChannel per stream
 * (vcIdDst = canonical VC) -> one SIPL capture pipeline each. VC uniqueness is host-owned (the DS5
 * honors the per-stream VC word but does not police collisions).
 *
 * Register access convention (realsense_d457.py): 16-bit reg offset, 16-bit value, LITTLE-endian.
 * The SIPL HSL II2CBuilder serializes 16-bit words big-endian, so the driver byte-swaps BOTH the
 * register offset and the data word (swap16) — see D457Sensor.cpp RunRegTable / FINDINGS §5g,§5j.
 */
#ifndef D457_DS5_REGISTERS_H
#define D457_DS5_REGISTERS_H

#include <stdint.h>
#include <stddef.h>

/* DS5 mux I2C address (7-bit).
 *   D457_MUX_I2C_ADDR  = host-visible / virtual address (V4L2 "DS5 mux 9-001a", DT reg).
 *   D457_MUX_PHYS_ADDR = camera-local physical def-addr at power-on (DT def-addr).
 * Over GMSL the SIPL driver writes the PHYSICAL address directly (Physical I2C mode); the
 * serializer passes it through to the camera-local bus where the mux answers at 0x10. */
#define D457_MUX_I2C_ADDR            0x1A
#define D457_MUX_PHYS_ADDR           0x10

/* Sentinel used in the Python tables: a (WAIT,WAIT) entry => sleep ~0.5 s. */
#define D457_REG_WAIT_MS             0x0F

/* Stream control register 0x1000 = {streamId, cmd}. cmd 2=play, 1=stop. */
#define D457_REG_STREAM_CTRL         0x1000
#define D457_STREAM_START_DEPTH      0x0200   /* streamId 0, play  */
#define D457_STREAM_STOP_DEPTH       0x0100   /* streamId 0, stop  */
#define D457_STREAM_START_RGB        0x0201   /* streamId 1, play  */
#define D457_STREAM_STOP_RGB         0x0101   /* streamId 1, stop  */
#define D457_STREAM_START_IR         0x0204   /* streamId 4, play  */
#define D457_STREAM_STOP_IR          0x0104   /* streamId 4, stop  */

/* Per-stream control_status (config-struct +0x1E) and commanded-state (0x1004 + 4*streamId).
 * NB: the firmware also has a separate isStreaming bitfield block at 0x4800+2*streamId, but on
 * this rig's FW build that range reads the 0xCAFE "unimplemented" sentinel, so we use the +0x1E
 * control_status (proven valid for depth = 0x401E). These are read for logging only; the driver
 * does NOT gate StartStreaming on them (the DS5 only reports streaming once SIPL consumes CSI). */
#define D457_DEPTH_CONFIG_STATUS     0x401E   /* depth base 0x4000 + 0x1E */
#define D457_DEPTH_STREAM_STATUS     0x1004
#define D457_RGB_CONFIG_STATUS       0x403E   /* RGB   base 0x4020 + 0x1E */
#define D457_RGB_STREAM_STATUS       0x1008
#define D457_IR_CONFIG_STATUS        0x409E   /* IR/Y  base 0x4080 + 0x1E */
#define D457_IR_STREAM_STATUS        0x1014
#define D457_STATUS_STREAMING        0x1   /* control_status: 0 = OK/valid */
#define D457_STREAM_STREAMING        0x2   /* *_STREAM_STATUS commanded value when playing */

/* Start/stop polling budget (from realsense_d457.py). */
#define D457_START_MAX_COUNT         10
#define D457_START_POLL_TIME_MS      100

/* ── Parametric mode-config building blocks (consumed by D457Sensor.cpp BuildModeTable) ──
 * DS5 MIPI output (DS5 -> MAX9295): 2 lanes, 1000 Mbps/lane (d4xx MIPI_LANE_RATE). Programs the
 * DS5-side link only -- it stays 2-lane in the 4-lane config, exactly like d4xx's hybrid topology
 * (camera->serializer 2-lane, deserializer->Jetson 4-lane). The SoC CSI lane count / D-PHY rate
 * (4 lanes / 2500000) lives in the SIPL query's mipiSettings + the deser HSL's seq_set_mipi_d_phy;
 * the three have to agree or every frame is truncated. */
#define D457_REG_MIPI_LANES          0x0400
#define D457_VAL_MIPI_2LANE          0x0001
#define D457_REG_MIPI_RATE           0x0402
#define D457_VAL_MIPI_1000M          0x03E8

/* Per-stream config-struct bases (STStreamConfiguration array, 0x20 B stride). */
#define D457_CFG_BASE_DEPTH          0x4000
#define D457_CFG_BASE_RGB            0x4020
#define D457_CFG_BASE_IR             0x4080
/* Field offsets within a stream's config struct (add to the base above). */
#define D457_CFG_OFF_DT              0x00   /* dt (byte0) */
#define D457_CFG_OFF_MDVC            0x02   /* (vc<<8)|metaDataType; md_fmt 0 = no embedded line */
#define D457_CFG_OFF_WIDTH           0x04
#define D457_CFG_OFF_HEIGHT          0x08
#define D457_CFG_OFF_FPS             0x0C
#define D457_CFG_OFF_DTOUT           0x1C   /* CSI output-DT override (depth/IR -> RAW16; RGB ignores) */

/* Per-stream native data types and the RAW16 output override. */
#define D457_DT_DEPTH_Z16            0x0031  /* depth config-struct dt */
#define D457_DT_RGB_YUV422           0x001E  /* RGB native YUYV (deser remaps 0x1E->0x2E) */
#define D457_DT_IR_R8L8              0x0032  /* IR UserDefined3_R8L8 (sfY_R8L8, 16 bpp) */
#define D457_DT_OUT_RAW16            0x002E  /* dtOut: deliver to Tegra as RAW16 */

typedef struct {
    uint16_t reg;   /* register address (or D457_REG_WAIT_MS sentinel) */
    uint16_t val;   /* value (or D457_REG_WAIT_MS with reg==WAIT => delay) */
} d457_reg_t;

/* Worst-case entry count produced by BuildModeTable (depth/IR): lanes, rate, dt, mdvc, dtout,
 * fps, width, height, stream-ctrl, WAIT = 10. RGB omits dtout (9). Callers size buffers >= this. */
#define D457_MODE_TABLE_MAX_ENTRIES  10

/* ---- stop tables (resolution-independent) ---- */
static const d457_reg_t d457_stop_depth[] = {
    {0x1000,0x0100},{D457_REG_WAIT_MS,D457_REG_WAIT_MS},
};
static const d457_reg_t d457_stop_rgb[] = {
    {0x1000,0x0101},{D457_REG_WAIT_MS,D457_REG_WAIT_MS},
};
static const d457_reg_t d457_stop_ir[] = {
    {0x1000,0x0104},{D457_REG_WAIT_MS,D457_REG_WAIT_MS},
};

#endif /* D457_DS5_REGISTERS_H */
