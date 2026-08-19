# D457 SIPL/UDDF — High-Level Architecture

How the **RealSense D457** streams over **GMSL** into an **NVIDIA Jetson Thor**
(Advantech MIC-742, L4T r39.2) through **NVIDIA SIPL/UDDF**, instead of the V4L2 `d4xx` path.

This is the *map*. For the "why" behind any hard-won value, see ``; for current
status and what's left, see ``.

---

## 1. The big picture — two planes

The system is one camera pipeline split into two opposite-flowing planes that meet at the chips:

- **Control plane — I2C, flows *down*.** The app asks SIPL for a camera; SIPL resolves the config
  from our query plugin, instantiates our UDDF module driver + the (patched) stock SerDes drivers,
  and they issue register writes over I2C bus 9 to bring up the link and start the DS5 streaming.
- **Data plane — MIPI CSI-2, flows *up*.** Once streaming, the DS5 pushes pixels through the
  serializer → GMSL2 → deserializer → Tegra CSI/VI, which DMAs them into capture buffers that SIPL
  hands to a consumer. **The ISP is bypassed** — frames are captured raw (RAW16) and reinterpreted
  by the consumer.

The single most important routing decision: **every stream reaches Tegra as RAW16 (MIPI DT 0x2E)**,
because SIPL's ICP capture path silently drops YUV422. Depth/IR set the DS5 output-DT override to
0x2E directly; RGB stays native YUV422 (0x1E) and the deserializer remaps 0x1E→0x2E.

---

## 2. Software-layer graph

```
                          ┌──────────────────────────────────────────────────┐
                          │  L7  CONSUMER / APPLICATION                        │
   data plane             │  nvsipl_camera (NVIDIA sample) — only consumer     │
   (frames, up)           │  today. "RealSense SIPL viewer" = NOT BUILT yet.   │   ⬅ frames out
        ▲                 └───────────────────────────┬──────────────────────┘
        │                                             │ INvSIPLClient / NotificationQueue
        │                 ┌───────────────────────────┴──────────────────────┐
        │                 │  L6  SIPL FRAMEWORK  (NVIDIA libnvsipl.so)         │
        │                 │  INvSIPLCamera · pipeline mgr · ICP raw capture    │
        │                 │  (ISP OFF) · NvSciBuf buffer pools                 │
        │                 │  ── 1 patch: BuildSensorProperty lanes 4→2 ──      │
        │                 └──────┬──────────────────────────┬─────────────────┘
        │                        │ dlopen qry_*.so          │ loads UDDF drivers
        │            ┌───────────┴──────────┐    ┌──────────┴────────────────────────┐
        │            │ L5  QUERY DB PLUGIN   │    │ L4  UDDF DRIVERS                   │
        │            │ libnvsipl_qry_d457.so │    │ (camera-module + SerDes)           │
   control plane     │ CNvMQuery_GetJsonData │    │  ┌──────────────────────────────┐  │
   (I2C, down)       │ → camera config JSON: │    │  │ D457 module (OURS)           │  │
        │            │  • D457_Camera        │    │  │  Library/Module/Sensor       │  │
        │            │  • 3 sensors→3 VCs    │    │  │  · DS5 mux control @ 0x1A    │  │
        ▼            │  • board transport    │    │  │  · per-stream mode tables    │  │
                     │  • PoC (MAX20087)     │    │  └──────────────────────────────┘  │
                     └───────────────────────┘    │  ┌──────────────┐ ┌─────────────┐  │
                                                   │  │ MAX9295 ser  │ │ MAX96712    │  │
                                                   │  │ (stock+patch)│ │ deser       │  │
                                                   │  │              │ │ (stock+patch)│  │
                                                   │  └──────────────┘ └─────────────┘  │
                                                   └──────────────┬────────────────────┘
                                                                  │ HSL dynamic I2C sequences
                          ┌───────────────────────────────────────┴──────────────────────┐
                          │  L3  HAL / HSL + cdi-mgr  (NVIDIA)                              │
                          │  Camera HAL (→ syslog) · HSL I2C encoder (16-bit BE on wire)    │
                          │  · cdi_mgr.ko → /dev/cdi-mgr.9.a  (enabled by our DT overlay)   │
                          └───────────────────────────────────────┬──────────────────────┘
                                                                  │ /dev/i2c-9
   ════════════════════════════════════ HARDWARE ══════════════════════════════════════════
        ▲ MIPI CSI-2                                              ▼ I2C bus 9
        │                ┌──────────────────────────────────────────────────────────────┐
        │                │  L2  GMSL LINK                                                 │
        │   ┌────────────┤  D457 (DS5 ASIC + OV9282)                                      │
        │   │  depth Z16 │     │ MIPI                                                     │
        │   │  rgb  YUYV │     ▼                                                          │
        │   │  ir   Y8I  │  MAX9295A serializer ── GMSL2 6Gbps ──> MAX96712 deserializer  │
        │   └────────────┤                                              │ 2-lane D-PHY    │
        │                └──────────────────────────────────────────────┼───────────────┘
        │                                                                ▼ 594 Mbps/lane
        │                ┌──────────────────────────────────────────────────────────────┐
        └────────────────┤  L1  TEGRA CAPTURE                                             │
                         │  CSI-A → NVCSI (CIL-A, 2 lanes) → VI → DMA into NvSciBuf pool   │
                         └──────────────────────────────────────────────────────────────┘
```

**Reading it:** control descends the right side (L7→L1) selecting config and writing registers;
data ascends the left side (L1→L7) as captured RAW16 frames. The DS5 chip (L2) is the pivot — it is
*told* what to do over I2C and *answers* with pixels over CSI.

---

## 3. Where the code lives, per layer

Everything we own lives under `d457-sipl/`. NVIDIA-owned layers are listed for context but are not
in this repo (except the patches we apply to them).

| Layer | Component | Owner | Code location |
|---|---|---|---|
| **L8** | RealSense SIPL viewer | **NOT BUILT** | *(planned; see HANDOVER §4.1)* |
| **L7** Consumer | `nvsipl_camera` sample | NVIDIA | (SDK sample; on-rig). Usage: `d457-sipl/docs/nvsipl_camera-guide.md`, `tools/d457_live.sh` |
| **L6** SIPL framework | `libnvsipl.so` | NVIDIA | system lib. Our **lanes 4→2** patch: `sdk-patches/patch_libnvsipl_lanes2.sh` (patches an `LD_LIBRARY_PATH` copy, system lib untouched) |
| **L5** Query DB | `libnvsipl_qry_d457.so` | **RealSense** | `query/d457_query.cpp` (the `D457_Camera` config JSON), `query/CMakeLists.txt`. → `/usr/lib/nvsipl_drv` |
| **L4** Camera module driver | `libuddf_d457cameramodule_library.so` | **RealSense** | `uddf_driver/D457Library.cpp` (discovery/registration), `D457Module.{hpp,cpp}` (module = sensor + serializer; DS5 I2C owner), `D457Sensor.{hpp,cpp}` (DS5 control: mode/stream/stop/status), `d457_ds5_registers.h` (DS5 register tables) |
| **L4** Serializer driver | MAX9295 | NVIDIA + **RealSense patch** | stock SDK driver; `sdk-patches/patch_max9295_d457.py` adds the D457 single-link path (stock is HAWK-only / `numSensors==2`) |
| **L4** Deserializer driver | MAX96712 | NVIDIA + **RealSense patch** | stock SDK driver; `sdk-patches/patch_max96712_d457.py` (2x4-on-PHY1 / 2-lane output + 0x1E→0x2E pixel remap for RGB) |
| **L3** HAL / HSL / cdi-mgr | Camera HAL, HSL, `cdi_mgr.ko` | NVIDIA | kernel + HAL. Our **DT overlay** enabling the cdi-mgr binding: `dt/d457-cdi-mgr-overlay.dts` (+ `dt/revert-to-d4xx.sh`, `dt/d4xx-camera.dts.ref`) |
| **L2** GMSL hardware | DS5/MAX9295A/MAX96712 | hardware | register sequences ported into `d457_ds5_registers.h`; SerDes values in the patch scripts |
| **L1** Tegra capture | NVCSI / VI | NVIDIA SoC | n/a (configured via L5 transport JSON + L4 deser patch) |

Supporting (not a runtime layer):

| Purpose | Location |
|---|---|
| Build + deploy on the rig | `tools/build_deploy.sh` |
| Stream test suite (each-stream, all-three, start/stop, permutations) | `tests/` (`run_all.sh`, `test_0*.sh`, `lib/common.sh`) |
| Frame viewing / conversion tools | `tools/d457_live.sh`, `tools/d457_mjpeg.py`, `tools/rgb_convert.py` |
| SerDes own-driver reference design | `reference/sipl-serdes/`, `docs/serdes-own-drivers-design.md` |
| Debug guide (build/deploy/diagnose) | `docs/debug-guide.md` |

---

## 4. Key design points

**The module driver is a thin control path, not a sensor driver.** The D457 is not a raw Bayer
sensor — the DS5 ASIC outputs already-processed depth/IR/color as a 16-bpp CSI stream. So
`D457Sensor` does no exposure/gain/AWB; it just selects a mode, plays/stops the stream
(`0x1000 = {streamId, cmd}`), and polls status. ISP is off; capture is raw passthrough.
(`uddf_driver/D457Sensor.cpp`)

**Two byte-swaps you must never forget.** The SIPL HSL I2C layer serializes 16-bit words
big-endian, but the DS5 register interface is little-endian — so the driver `swap16()`s **both** the
register offset and the value on every DS5 access. (`D457Sensor.cpp::RunRegTable`)

**One module, multiple streams = multiple sensorInfo entries on one mux.** SIPL maps one
`sensorInfo` → one virtual channel → one capture pipeline. Depth/RGB/IR are therefore three
`sensorInfo` entries (deviceIndex 0/1/2 → VC0/1/2) all at the *same* DS5 mux address 0x1A.
`D457Module` creates one `D457Sensor` per entry; **sensor 0 owns all DS5 I2C** (registers the shared
mux mapping and programs every stream in one sequence) — the others are capture-only, because
re-touching the shared mux mid-stream stalls the RGB ISP. (`query/d457_query.cpp`, `D457Module.cpp`,
`D457Sensor.cpp::StartStreaming`)

**Stream selection.** `D457_STREAMS=depth,rgb,ir` (test harness) or the legacy `D457_STREAM` env, or
the sensor count from the query, picks which streams run; each stream rides its canonical VC
(depth→VC0, RGB→VC1, IR→VC2). (`D457Sensor.cpp::BuildStreamList`)

**RAW16 end-to-end** is the unlock that makes capture work at all — see §1 and the header comment in
`d457_ds5_registers.h`.

---

## 5. Runtime flow in one trace

```
nvsipl_camera -c D457_Camera
  └─ libnvsipl: ParseDatabase → dlopen libnvsipl_qry_d457.so → CNvMQuery_GetJsonData()   [L6→L5]
       → matches board "NVIDIA Jetson AGX Thor", resolves D457_Camera + transport
  └─ libnvsipl: loads UDDF drivers by name "D457" / "MAX9295" / "Max96712GmslDeserializer" [L6→L4]
       → D457Module::doCreateUbbObjects: MAX9295 ser (single-link) + N × D457Sensor
  └─ Init: deser link-locks, MAX9295 detected, DS5 reachable at Physical 0x1A          [L4→L3→L2]
  └─ StartStreaming: sensor 0 writes stop+mode tables for every stream, in one HSL seq  [L4→L3→I2C]
       → DS5 begins emitting depth(0x2E)/rgb(0x1E→0x2E)/ir(0x2E) on VC0/1/2
  └─ frames flow DS5 → MAX9295 → GMSL2 → MAX96712 → CSI-A → NVCSI(2-lane,594) → VI       [L2→L1]
       → DMA into NvSciBuf → ICP raw-capture pool → INvSIPLClient queue → consumer       [L1→L7]
```

---

## 6. Status anchor

All three streams (depth Z16, RGB YUYV, IR Y8I) capture over SIPL with real content, each delivered
as RAW16 on its own VC. Remaining work is integration/hardening — the consumer/viewer app, making
the lanes patch permanent, run-to-run stability (DS5 wedge / PoC power-cycle), and soak. See
`` §4 for the live punch list.
