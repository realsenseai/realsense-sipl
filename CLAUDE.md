# CLAUDE.md — realsense-holoscan

Workspace for bringing up the **Intel RealSense D457 over GMSL on an NVIDIA Jetson Thor
(Advantech MIC-742, L4T r39.2)** through **NVIDIA SIPL/UDDF**, as an alternative to the V4L2 `d4xx`
path. Also hosts RealSense + Holoscan Sensor Bridge integration material.

## Repository layout
- **`d457-sipl/`** — ⭐ the active effort: the D457 SIPL/UDDF camera-module driver, query plugin,
  cdi-mgr device-tree overlay, DS5 register tables, and `sdk-patches/`.
- **`.triage/`** — handoff docs + the running log. **`.triage/FINDINGS.md`** is the running log
  (SOLVED recipe at the top, journey in §5l–§5o). **`.triage/HANDOVER-d457-sipl-3.md`** is the current
  cold-start handover (status, recipe, rig access, what's left); #1/#2 are earlier snapshots.
- `sipl/` — local copy of the public Jetson "Camera SIPL" SDK headers/sample (CoE-only carve).
- `holoscan-sensor-bridge/` — Holoscan Sensor Bridge.
- `realsense-holoscan/` — Holoscan app integration; `realsense-holoscan/unused/d457/` holds the
  **Hololink reference** (`d457_registers.py`, `realsense_d457.py`) — the simplified DS5 register
  sequences ported into the SIPL driver.
- `resources/` — misc assets.

Default branch: `main`. Active branch: `d457-sipl-gmsl`.

## Companion repos (separate checkouts, ground-truth references)
- **`C:\work\rs-fw-ds5`** — DS5 ASIC **firmware**. Authoritative for the host I2C register/HWMC
  interface and the streaming control/flow (`DS5_B0_DEV/AppServicesLib/{I2cSlaveHostIf,FlowDepth}.c`).
- **`C:\work\realsense_mipi_platform_driver`** — the **d4xx** Linux/V4L2 GMSL driver
  (`kernel/realsense/d4xx.c`). The working host stream-start sequence on this same hardware.

## Target rig
- **`fw-advantech-thor-1`** — Advantech MIC-742, Jetson Thor, L4T R39.2, kernel 6.8.12-tegra.
- SSH: PuTTY plink, `mic-742`/`mic-742` (scoped allow rule in `.claude/settings.local.json`; no
  standing key). The Camera HAL logs to **syslog** — diagnose with `journalctl … | grep CameraHAL`,
  kernel CSI/VI with `dmesg`.
- GMSL chain: D457 (DS5 ASIC + OV9282) → MAX9295A ser → MAX96712 deser → Thor CSI-A, I2C bus 9.
- SIPL boot uses the non-default `sipl-d457` extlinux label (`cdi-mgr` overlay, `/dev/cdi-mgr.9.a`);
  DEFAULT `d4xx` is the V4L2 fallback.

## Current state (2026-06-24)
- ✅ **All three streams capture over SIPL** with real content: **depth (Z16), RGB (YUYV color), IR
  (Y8I)**. Each delivered to Tegra as **RAW16 (DT 0x2E) on VC0**, selected at runtime by the
  **`D457_STREAM=depth|rgb|ir`** env var (default depth). See FINDINGS top: two "SOLVED" blocks
  (depth 2026-06-23, RGB+IR 2026-06-24).
- Remaining: simultaneous multi-stream (multi-VC), the consumer/viewer app, run-to-run stability
  (DS5 wedges between runs — PoC power-cycle needed), soak, multi-link.

### Hard-won facts (don't relearn these)
- **DS5 register offset AND data word are byte-swapped** by the SIPL HSL I2C layer — `swap16()` on
  both the register offset and the 16-bit value on every DS5 access.
- DS5 mux answers at **Physical 0x1A** (not the `def-addr 0x10`); use `I2CAddressMode::Physical`.
- Per-stream config base: **depth 0x4000, RGB 0x4020, IR/Y 0x4080** (each +0x1C = dtOut override,
  +0x1E = control_status). Stream start `0x1000={streamId,cmd2}`: depth 0x0200, RGB 0x0201, IR 0x0204.
- **SIPL's ICP silently drops YUV422 (DT 0x1E)** — route every stream as RAW16 (0x2E). Depth/IR set
  the DS5 dtOut override (0x401C/0x409C = 0x2E); **RGB ignores dtOut**, so the DS5 emits native 0x1E
  and the **MAX96712 deser remaps 0x1E→0x2E** (extra pixel-map slot).
- **RGB needs MAX9295 RCLKOUT (0x03F1) + GPIO (0x02BE/0x02BF)** — the RealTek RTS5845 RGB ISP needs
  the serializer-forwarded reference clock to output pixels (depth/IR OV9282 are self-clocked).
- **MAX9295 pipe DT-filters** — pipe-0 needs dt2=0x1E (`0x0315=0xDE`) to forward RGB alongside
  depth/IR's 0x2E; else deser RX never locks (`0x01DC` bit0=0).
- **Never redirect `nvsipl_camera` output to a file** (50 MB/s error spew on 0 frames filled the disk);
  use `pkill -x nvsipl_camera` (not `-f`). **Do not poll the DS5 over i2c while RGB streams** (disrupts it).
- The **stock MAX9295 SDK driver is HAWK-only** (`numSensors==2`); `sdk-patches/patch_max9295_d457.py`
  adds the D457 single-link path.

## Skills & agents in this repo (`.claude/`)
Ported from the companion repos + bring-up-specific additions:
- **`ds5-log-collector`** (skill) — capture DS5 FW logs from a rig via `rs-fw-logger`.
- **`build-ds5`** (skill) — build the DS5 firmware (operates in `C:\work\rs-fw-ds5`).
- **`mipi-build`** / **`mipi-deploy`** (skills) — build/deploy the d4xx driver (operates in
  `C:\work\realsense_mipi_platform_driver`).
- **`mipi-driver-investigator`** (agent) — read-only investigator of the d4xx driver repo.
- **`ds5-firmware-investigator`** (agent) — read-only investigator of the DS5 firmware repo.

Build/deploy skills run against their **source repo**, not this one — each skill's "Repo location"
note has the absolute path. Investigator agents are read-only (never mutate repo/tree/device).

## Conventions
- Don't commit to `main`; work on `d457-sipl-gmsl` (or a branch).
- Keep `.triage/FINDINGS.md` and the `.triage/HANDOVER-*` docs current as the effort progresses.
- Rig changes are reversible: the `d4xx` boot label + `dt/revert-to-d4xx.sh` are the fallback.
