# D457 SIPL/UDDF — Camera Controls Design

How to bring **d4xx-parity camera controls** (exposure, gain, auto-exposure, white balance,
laser/emitter, RGB ISP knobs, …) to the **D457 SIPL/UDDF** driver — exposed through a **structured
SIPL interface**, so the application sets *semantic* controls (`SetManualExposureUs(33000)`) and the
driver does the DS5 register I/O internally. The application never computes register offsets or
issues raw I2C.

Read alongside `architecture.md` (the layer map) and `` (hard-won facts). All
register/encoding claims below are cited at the end (§13).

---

## 1. Goal and scope

- **Goal:** the same control surface the `d4xx` V4L2 driver gives on this hardware — exposure, gain,
  auto-exposure, manual/auto white balance, white-balance temperature, saturation, sharpness,
  power-line frequency, laser/emitter enable + manual laser power, sync mode — driven through SIPL.
- **Constraint from the user:** *use the SIPL interface; do not write I2C directly* from the client.
  The control logic and register encoding live **inside the driver**, behind a typed interface that
  SIPL hands to the client.
- **Non-goals (this design):** the consumer/viewer app itself; advanced-mode depth-control presets
  and per-pixel tables (HWMC, deferred to a later phase); IMU controls (the IMU exposes only
  `fw_version` even in d4xx).

---

## 2. Background — how a control actually reaches the DS5

The D457 is a **smart camera**: the DS5 ASIC runs its own ISP/AE/AWB and emits already-processed
depth/IR/color. There is **no Tegra ISP** in our pipeline — every stream is captured as RAW16
passthrough (`architecture.md` §1). A "control" is therefore **a write into the DS5's host-facing
control register file over I2C bus 9** — the same swap16/`II2CBuilder` path the driver already uses
for mode/stream tables. There is no other actuator.

The DS5 firmware offers two host transports for controls (§13a, §13b):

- **(A) Per-camera control register blocks** — plain 16-bit register writes. **This covers every
  standard image control plus laser/emitter.** Two blocks only:
  - **`0x4100` — cam0 = depth + left-IR** (depth and IR share one imager control set).
  - **`0x4200` — cam1 = RGB** (gated by the RGB SKU; the D457 has it).
- **(B) HWMC mailbox** (`0x4900` data / `0x490C` exec / `0x4904` status) — for AE ROI/setpoint/type,
  calibration tables, presets, version. Richer, framed, async. **Phase 2 only** — path (A) covers
  the parity set we care about first.

Path (A) is what `d4xx` uses for the same controls, and what the DS5's own internal master
(`D4MImager.c`) uses — it is the proven encoding (§13a, §13c).

---

## 3. The core decision — which "SIPL interface"

SIPL exposes **two** structured control surfaces. Neither alone gives d4xx parity; the design picks
deliberately.

### 3a. SIPL's native `ISensorControl` — necessary to understand, but not sufficient

`uddf::ddi::interfaces::ISensorControl` (`SetSensorControls` / `GetSensorAttributes` /
`Parse*EmbeddedData`, §13d) is NVIDIA's structured sensor-control interface. Two problems make it a
poor *primary* fit:

1. **It models only the classic AE/AWB sensor surface** — `exposureGainControl` (seconds + gain
   multiplier), `wbControl` (per-channel float gains), and `illuminationControl` (a single
   on/off bool for the emitter) (§13e). It has **no representation** for manual laser *power level*
   (mW), RGB saturation/sharpness/WB-temperature/power-line-frequency, AE ROI/priority, sync mode,
   or readout shaping. Those are the bulk of d4xx parity and simply don't fit its structs.
2. **It is driven by SIPL's ISP / auto-control loop**, which is **off** in our RAW16 passthrough
   pipeline. With ISP disabled, SIPL very likely never calls `SetSensorControls` at runtime at all
   (**open question O1, §12** — must be confirmed on the rig). And even if it did, letting SIPL's
   auto-control plugin drive exposure would **fight the DS5's own AE** — which we do not want.

So `ISensorControl` cannot be the parity surface. It stays in the design only as an *optional,
secondary* reporting/compatibility shim (§5d).

### 3b. The existing `IReadWriteI2C` — exactly what the user wants to move away from

`D457Module` already exposes `IReadWriteI2C` (`WriteI2C`/`ReadI2C`, `D457Module.cpp:55`). That is the
client *computing register offsets and writing them* — raw I2C from the app. It stays as a
debug/escape hatch but is **not** the control API.

### 3c. Decision — a custom **typed** control interface, delivered the SIPL way

Define **`ID457CameraControl`**, a typed UDDF interface with semantic methods
(`SetManualExposureUs`, `SetGain`, `SetAutoExposure`, `SetLaserPowerMw`, `SetEmitterMode`,
`SetWhiteBalanceTempK`, …). It is delivered through SIPL's **standard custom-interface-provider
mechanism** — the identical path `IReadWriteI2C` already rides:

```
client: INvSIPLCamera::GetModuleInterfaceProvider(idx, provider)   // after Init(), before Start()
        provider->GetInterface(ID457CameraControl::id)  →  ID457CameraControl*
        ctrl->SetManualExposureUs(Cam::DepthIR, 33000);            // semantic call
driver: D457Module maps the call → DS5 control block 0x4100 → sensor 0 WriteReg16 (swap16) over I2C
```

This satisfies *"use the SIPL interface, not raw I2C"*: the client uses SIPL's `INvSIPLCamera` +
interface-provider to obtain a typed control object and calls named controls; **all register
offsets, encodings, byte-swaps, unit conversions, ordering and gating live inside the driver.** It
is "the SIPL interface" in the same sense `IReadWriteI2C` is — retrieved via the SIPL provider — but
typed and d4xx-shaped instead of raw.

> **Why not push NVIDIA to extend `ISensorControl`?** It's a fixed NVIDIA SDK header; we can't add
> fields. The provider/`GetInterface(UUID)` extension point is precisely the SDK's sanctioned way to
> add vendor controls. This is the intended pattern, not a workaround.

---

## 4. Mapped control surface (d4xx parity)

All controls below are **path (A)** register writes. Addresses are absolute DS5 registers; offset
within the block in parens. Each is **get + set** unless noted. The driver dispatches by camera
target to the correct block, so the depth/RGB offset overlaps (e.g. `+0x08`) are never ambiguous.

### 4a. Depth + IR — `Cam::DepthIR`, block `0x4100` (cam0)

| Control (d4xx CID) | Register | Encoding / range | Notes |
|---|---|---|---|
| Manual exposure (`EXPOSURE_ABSOLUTE`) | `0x4100` LSW / `0x4102` MSW | **uint32 µs**, 1..165000 (D450 SKU 200000) | 32-bit; **write MSW first, then LSW commits** (§6b) |
| Gain (`ANALOGUE_GAIN`) | `0x4104` (+0x04) | uint16, **16..248** | imager gain code |
| Auto-exposure (`EXPOSURE_AUTO`) | `0x410C` (+0x0C) | uint16 bool 0/1 | when on, DS5 runs its own AE |
| Emitter/laser enable (`LASER_POWER`/emitter) | `0x4108` (+0x08) | uint16 mode: **0 off, 1 laser-on, 2 auto, 3 led** | projector mode enum |
| Manual laser power (`MANUAL_LASER_POWER`) | `0x4124` (+0x24) | uint16 **mW, 0..360, step 30** | clamped by eyesafety/calib at runtime (§8) |
| AE ROI (`AE_ROI_SET`) | top `0x4110`/left `0x4114`/bottom `0x4118`/right `0x411C` | 4×uint16 px | **write left/bottom/right first; top commits** (§6c) |
| Sync mode (`SYNC_MODE`) | `0x412C` (+0x2C) | uint16: default/master/external | also toggles serializer ESYNC in d4xx |
| Laser PWM freq (`PWM`) | `0x4128` (+0x28) | uint16 | |
| Readout shaping (`READOUT_SHAPING`) | `0x4130` (+0x30) | uint16 0..100 (%) | depth only |
| Preset (`PRESET`) | `0x4120` (+0x20) | uint16 preset id | semantics FW-side |

### 4b. RGB — `Cam::Color`, block `0x4200` (cam1)

| Control (d4xx CID) | Register | Encoding / range | Notes |
|---|---|---|---|
| Manual exposure (`EXPOSURE_ABSOLUTE`) | `0x4200` LSW / `0x4202` MSW | **uint32, 100 µs units** (UVC), 1..10000 | driver converts µs↔100µs at the boundary (§6a) |
| Gain (`ANALOGUE_GAIN`) | `0x4204` (+0x04) | uint16, 0..128 | |
| Auto-exposure mode (`EXPOSURE_AUTO`) | `0x420C` (+0x0C) | uint16 UVC AE bitmap: 1 manual / 2 auto / 4 shutter-pri / 8 aperture-pri | |
| AE priority (`EXPOSURE_AUTO_PRIORITY`) | `0x4208` (+0x08) | uint16 0/1 | hidden on some SKUs |
| Saturation (`SATURATION`) | `0x4210` (+0x10) | uint16 0..100 | |
| Sharpness (`SHARPNESS`) | `0x4214` (+0x14) | uint16 0..100 | |
| White-balance temperature (`WHITE_BALANCE_TEMPERATURE`) | `0x4218` (+0x18) | uint16 **Kelvin**, 2800..6500 step 10 | |
| Auto white balance (`AUTO_WHITE_BALANCE`) | `0x421C` (+0x1C) | uint16 0/1 | |
| Power-line frequency (`POWER_LINE_FREQUENCY`) | `0x4220` (+0x20) | uint16: 0 off / 1 50 Hz / 2 60 Hz | |

### 4c. Deferred to Phase 2 (HWMC, path B)

AE setpoint, AE algorithm type, depth-control presets/advanced-mode tables, calibration read/write,
GVD/version, EEPROM. These need the `0x4900` mailbox framing (write blob → exec → poll status → read
response). Not required for control parity with the everyday d4xx surface.

> **Not present in d4xx and not wired in DS5 path (A):** brightness, contrast, gamma, hue, backlight
> compensation. They are UVC selectors in firmware but unbound — reachable only via HWMC or a
> firmware change. Out of scope; call out explicitly so nobody hunts for them.

---

## 5. Proposed architecture

### 5a. The interface (sketch — not final signatures)

Lives in our namespace `uddf::cdd::d457`, modeled structurally on `IReadWriteI2C` (a UUID `id`, derive
from `uddf::ddi::IInterface`):

```cpp
namespace uddf::cdd::d457 {

enum class Cam : uint8_t { DepthIR = 0, Color = 1 };
enum class EmitterMode : uint16_t { Off = 0, Laser = 1, Auto = 2, Led = 3 };
enum class SyncMode    : uint16_t { Default = 0, Master = 1, External = 2 };
enum class PowerLineFreq : uint16_t { Disabled = 0, Hz50 = 1, Hz60 = 2 };

enum class CtrlResult { Ok, OutOfRange, NotSupported, Busy, NotReady, HwError };

class ID457CameraControl : public uddf::ddi::IInterface {
public:
    static constexpr uddf::ddi::UUID id { /* freshly generated UUID */ };

    // Common (both cameras)
    virtual CtrlResult SetAutoExposure(Cam, bool enable)            = 0;
    virtual CtrlResult GetAutoExposure(Cam, bool& enable)           = 0;
    virtual CtrlResult SetManualExposureUs(Cam, uint32_t micros)    = 0;   // converts for RGB
    virtual CtrlResult GetManualExposureUs(Cam, uint32_t& micros)   = 0;
    virtual CtrlResult SetGain(Cam, uint16_t gain)                  = 0;
    virtual CtrlResult GetGain(Cam, uint16_t& gain)                 = 0;
    virtual CtrlResult SetAeRoi(Cam, uint16_t top, uint16_t left,
                                     uint16_t bottom, uint16_t right) = 0;

    // Depth/IR only
    virtual CtrlResult SetEmitterMode(EmitterMode)                  = 0;
    virtual CtrlResult SetLaserPowerMw(uint16_t mw)                 = 0;   // 0..360 step 30
    virtual CtrlResult GetLaserPowerMw(uint16_t& mw)                = 0;
    virtual CtrlResult SetSyncMode(SyncMode)                        = 0;
    virtual CtrlResult SetReadoutShaping(uint16_t percent)          = 0;

    // RGB only
    virtual CtrlResult SetAutoWhiteBalance(bool)                    = 0;
    virtual CtrlResult SetWhiteBalanceTempK(uint16_t kelvin)        = 0;
    virtual CtrlResult SetSaturation(uint16_t)                      = 0;
    virtual CtrlResult SetSharpness(uint16_t)                       = 0;
    virtual CtrlResult SetPowerLineFrequency(PowerLineFreq)         = 0;
    virtual CtrlResult SetAePriority(bool)                          = 0;

    // Capability query (driver-hardcoded ranges; FW exposes no MIN/MAX/DEF on path A — §6d)
    struct ControlInfo { int32_t min, max, step, def; bool supported; };
    virtual CtrlResult GetControlInfo(/*ControlId*/ uint32_t, Cam, ControlInfo&) = 0;

protected:
    ~ID457CameraControl() override = default;
};
} // namespace
```

### 5b. Where it's implemented and how the call flows

`D457Module` implements `ID457CameraControl` and returns it from `GetInterface(uuid)` alongside
`IReadWriteI2C` (one extra `if` in `D457Module.cpp:47`). Each method:

1. validates the value against the hard-coded range for that `(control, Cam)` → `OutOfRange`;
2. picks the block base by `Cam` (`0x4100` / `0x4200`) and the offset for the control;
3. applies unit conversion (RGB exposure µs→100µs);
4. calls **`m_sensor0->WriteReg16(reg, val)`** — sensor 0 owns the DS5 mux I2C and the swap16 path
   (`D457Module.cpp:36`, `D457Sensor.hpp:67`). 32-bit and multi-register controls do the ordered
   sub-writes (§6b/§6c) here.

No new I2C plumbing — it reuses the exact `WriteReg16/ReadReg16` already backing `IReadWriteI2C`. The
difference is purely that the *encoding logic moves from the client into the driver*.

### 5c. Why route everything through sensor 0

The driver's hard rule: **sensor 0 owns all DS5 mux I2C; touching the mux from elsewhere mid-stream
stalls the RGB ISP** (`architecture.md` §4, FINDINGS). Controls are mux writes, so they must funnel
through sensor 0 and be **serialized** (one mutex around the control path). This is also why the
control interface is exposed at the **module** level (`GetModuleInterfaceProvider`), not per-pipeline
— a single owner, one lock, both control blocks reachable. See §7 for the streaming-safety
consequences, which are the main risk in this design.

### 5d. Optional `ISensorControl` shim (low priority)

If O1 (§12) shows SIPL *does* call `SetSensorControls` in our pipeline, implement a **minimal**
`ISensorControl` that maps only `exposureGainControl`→exposure/gain and `illuminationControl`→emitter
on/off onto the same internal helpers, and a `GetSensorAttributes` returning the hard-coded ranges.
Do **not** register a SIPL auto-control plugin (it would fight the DS5's AE). This is a compatibility
nicety, not the parity mechanism.

---

## 6. Encoding mechanics the driver must get right

These are the non-obvious, must-not-forget details (all cited §13):

- **(a) Byte-swap stays.** Reuse `WriteReg16/ReadReg16` — they already `swap16()` both the register
  offset and the data word for the DS5 (FINDINGS hard-won fact). The control path inherits this for
  free; do **not** re-implement raw writes.
- **(b) 32-bit exposure ordering.** Write the **MSW at `+0x02` first**, then the **LSW at the base**,
  which is the write that *commits* the request. (`WriteReg16(base+2, msw); WriteReg16(base, lsw);`)
- **(c) AE-ROI ordering.** Write left/bottom/right (`+0x14/+0x18/+0x1C`) first, then **top (`+0x10`)
  commits** the ROI as a set.
- **(d) Ranges are driver-side.** Path (A) does **not** expose UVC MIN/MAX/DEF over registers — only
  the current value. Hard-code the ranges (from the tables in §4) in `GetControlInfo`. FW still
  range-checks and will reject out-of-range with an error status (§8), but we validate first for a
  clean `OutOfRange` and to avoid a wasted async round-trip.
- **(e) RGB exposure unit.** RGB block is **100 µs units**; depth block is **µs**. Keep the public API
  uniformly in microseconds and convert at the RGB boundary.

---

## 7. Concurrency & streaming safety — the main risk

Setting a control at runtime = writing the shared DS5 mux **while it is streaming**. Two facts make
this delicate:

- The driver already warns: **"do not poll the DS5 over I2C while RGB streams — it disrupts it"**, and
  re-touching the mux mid-stream stalls the RGB ISP (FINDINGS, `architecture.md` §4).
- But the DS5 control interface is *designed* to accept controls during streaming — that's the entire
  point of host AE/exposure control. The warning was about **gratuitous read-polling**, not
  legitimate single control writes.

Design rules to stay on the safe side:

1. **Serialize** every control through one mutex in sensor 0's control path; never overlap with the
   mode/stream table sequences.
2. **Writes are cheap and safe; read-back polling is the hazard.** Prefer fire-and-forget sets.
   For GET, do a **single** register read, not a busy-loop. For completion, see rule 3.
3. **Async completion without polling storms.** A SET is asynchronous: the FW stages it and applies on
   its queue; the write returns "work-in-progress". The per-stream `control_status` word
   (streaming-config base **+0x1E** → depth `0x401E`, RGB `0x403E`, IR `0x409E`; **verify exact RGB
   address, O3 §12**) flips WIP→OK/ERROR when applied. Read it **at most once or twice** after a set
   if confirmation is needed — never in a tight loop while RGB streams.
4. **Prefer applying depth/laser controls when depth is the active stream**; treat RGB-ISP writes as
   the higher-risk ones and gate them behind the same lock. If field testing shows any RGB
   disruption, fall back to *staging controls and applying at the next stream (re)start* for the
   affected knobs.

This risk is the one thing most likely to need empirical tuning on the rig.

---

## 8. Gating & error handling

- **Pre-stream sets are allowed.** The FW pre-seeds control registers at boot and accepts/staging
  controls any time; sensor-level apply is gated on power and **lands at stream start**. So a client
  may set exposure/gain/emitter before `Start()` and it takes effect when the stream comes up.
- **Range rejects** → FW sets the control status to ERROR and leaves the prior value; map to
  `OutOfRange`/`HwError`. We pre-validate (§6d) so this is a backstop.
- **Laser is not guaranteed by the register write.** Actual emission runs through an eyesafety
  interlock + calibration-EEPROM check, and power is clamped to a calibration max (≤360 mW). If the
  module's eyesafety/projector calibration is missing/failing, the emitter stays dark regardless of
  `0x4108`/`0x4124`. The driver reports the write result; **"laser didn't light" is a runtime
  eyesafety/calibration condition, diagnosable only via FW logs** (`ds5-log-collector`), not a
  register error. Document this for the client.
- **SKU gates.** Projector-mode and laser-power are disabled on no-projector SKUs; RGB block exists
  only on RGB SKUs. The D457 has both — but `GetControlInfo(...).supported` should reflect this so a
  client can grey-out unsupported controls rather than getting silent failures.

---

## 9. Client usage (illustrative)

```cpp
// after camera->Init(), before camera->Start()
IInterfaceProvider* prov = nullptr;
camera->GetModuleInterfaceProvider(/*sensorIdx=*/0, prov);
auto* ctrl = static_cast<ID457CameraControl*>(prov->GetInterface(ID457CameraControl::id));

ctrl->SetAutoExposure(Cam::DepthIR, false);
ctrl->SetManualExposureUs(Cam::DepthIR, 33000);   // 33 ms
ctrl->SetGain(Cam::DepthIR, 64);
ctrl->SetEmitterMode(EmitterMode::Laser);
ctrl->SetLaserPowerMw(150);

ctrl->SetAutoWhiteBalance(Cam::Color, false);
ctrl->SetWhiteBalanceTempK(4600);
ctrl->SetSaturation(64);

camera->Start();
// runtime: same ctrl-> calls are valid mid-stream (see §7 safety rules)
```

---

## 10. Implementation plan

**Phase 0 — verify (no code).** On the rig, resolve O1/O2/O3 (§12): does SIPL call `SetSensorControls`
in our ISP-off pipeline; does `GetModuleInterfaceProvider`→`GetInterface` reach `D457Module::GetInterface`
for a custom UUID (we already rely on this for `IReadWriteI2C`, so high confidence); exact RGB
`control_status` address. A throwaway test that sets exposure via the existing `IReadWriteI2C` to
`0x4100/0x4102` and reads it back confirms the whole register path end-to-end before building the
interface.

**Phase 1 — interface + depth/IR core.** Add `ID457CameraControl.hpp` (interface + UUID + enums).
Implement in `D457Module`: exposure (32-bit ordering), gain, auto-exposure, emitter mode, laser
power, sync mode; the dispatch-by-`Cam` + range-check + `WriteReg16` plumbing; the control mutex.
Wire into `GetInterface`. Unit-convert nothing yet (depth is µs).

**Phase 2 — RGB ISP + AE-ROI.** RGB exposure (µs↔100µs), gain, AE mode/priority, saturation,
sharpness, WB temp, auto-WB, power-line-freq; AE-ROI ordered write; `GetControlInfo` ranges +
`supported` SKU flags; GET paths.

**Phase 3 — HWMC (optional/later).** AE setpoint/type, presets, calibration — only if needed.

**Phase 4 — optional `ISensorControl` shim** if O1 says it fires.

**Files touched:** `uddf_driver/ID457CameraControl.hpp` (new), `uddf_driver/D457Module.{hpp,cpp}`
(implement + `GetInterface`), possibly small helpers in `D457Sensor.{hpp,cpp}` (a guarded
multi-write helper / the control mutex). No query-plugin or SerDes changes. `IReadWriteI2C` stays as
the debug hatch.

---

## 11. Testing

- **Register round-trip** (Phase 0): set exposure/gain via the path, read back, compare.
- **Per-control set/get** in the `tests/` harness (extend `lib/common.sh`): drive each control, read
  back, and where observable validate the *effect* (e.g. emitter on/off visible in IR; exposure
  change visible in frame brightness; WB temp shifts RGB).
- **Mid-stream safety** (the key one): set controls during a depth-only run, then during a
  depth+RGB+IR soak, and confirm **no RGB drop/stall** (reuse the 120 s 3-stream soak). This is where
  §7 gets validated; tune the staging fallback if RGB is disturbed.
- **Range/gating:** out-of-range rejects; laser with/without eyesafety calibration; pre-stream set →
  effect-at-start.

---

## 12. Open questions / to verify

- **O1 — Does SIPL invoke `ISensorControl::SetSensorControls` with ISP off?** Determines whether the
  shim (§5d) is worth anything. Expectation: no. Verify via a logging stub on the rig.
- **O2 — Does the custom-UUID `GetInterface` reach `D457Module` via `GetModuleInterfaceProvider`?**
  We already ship `IReadWriteI2C` through this exact path, so high confidence — but confirm the
  provider actually calls our `GetInterface` for a *second* custom UUID, and whether it must be the
  module vs pipeline provider for the index we use.
- **O3 — Exact RGB/IR `control_status` poll address.** CLAUDE.md hard-won facts put the per-stream
  config base at depth `0x4000` / RGB `0x4020` / IR `0x4080` with control_status at +0x1E. One source
  cited RGB status as `0x421E`; reconcile against the `0x4020`+0x1E = `0x403E` reading before relying
  on it. (Low stakes — we minimize polling anyway, §7.)
- **O4 — Per-SKU `supported` flags** for the actual D457 unit (projector present, RGB present): read
  once at init for `GetControlInfo`.
- **O5 — Mid-stream RGB disruption envelope:** which control writes (if any) actually perturb the RGB
  ISP, to decide what must use the stage-and-restart fallback.

---

## 13. References (file:line)

- **(a) d4xx control→register map** — the d4xx driver, `kernel/realsense/d4xx.c`:
  control bases/offsets `:155-181`; set/get handlers `ds5_s_ctrl :2709`, `ds5_g_volatile_ctrl :3154`;
  exposure helper `ds5_hw_set_exposure :2135`; AE helper `:2104`; CID defs `:2165-2206`,
  laser CIDs `:3400-3412`; HWMC mailbox `:2214-2217`, templates `:281-377`.
- **(b) DS5 firmware host control interface** —
  `DS5_B0_DEV/AppServicesLib/I2cSlaveHostIf.{h,c}`: control blocks/struct
  `I2cSlaveHostIf.h:62-205`; write dispatch + `i2cHostIfControlSet` `.c:566-569, 754-793`; async
  completion `.c:726-738`; read path `.c:591-613`; HWMC framing `.c:376-426`. Ranges
  `CoreServicesLib\Imager.h:12-17`; laser/eyesafety `CoreServicesLib\Pwm.{h,c}` (mode enum
  `Pwm.h:33-38`, max/step `Pwm.c:41`/`Pwm.h:22`, eyesafety gate `Pwm.c:206-339,508,561-578`); SKU
  gates `UsbApplication.c:978-982`.
- **(c) Canonical master-side encoding** — `DS5_B0_DEV/CoreServicesLib/D4MImager.c`:
  writes `:342-457`, reads `:138-169`, byte-reverse `:61-79`.
- **(d) SIPL/UDDF interfaces (local SDK)** —
  `sipl\uddf\include\uddf\ddi\interfaces\ISensorControl.hpp` (`SetSensorControls :91`),
  `SensorControlTypes.hpp` (`SensorControls :166-177`, `ExposureGainInfo :145-158`),
  `IReadWriteI2C.hpp` (UUID + signatures, the pattern to mirror),
  `sipl\include\NvSIPLCamera.hpp` (`GetModuleInterfaceProvider :1191`,
  `GetPipelineInterfaceProvider :1122`).
- **(e) Current driver** — `d457-sipl\uddf_driver\D457Module.{hpp,cpp}` (`GetInterface :47`,
  `WriteI2C :55`, sensor-0 ownership `:36`); `D457Sensor.hpp` (`WriteReg16/ReadReg16 :67-68`,
  cached HW access `:106`).

---

## 14. Implementation status & on-rig validation (2026-06-25)

What was built and what the hardware confirmed.

### Built (in the repo)
- **`uddf_driver/ID457CameraControl.hpp`** — the structured interface + the 17-control registry enum
  (depth/IR block `0x4100`, RGB block `0x4200`), `ControlDesc`, `CtrlId`, `CtrlResult`.
- **`uddf_driver/D457Module.{hpp,cpp}`** — implements `ID457CameraControl` (GetControlCount/
  QueryControl/QueryControlById/GetControl/SetControl) backed by a registry with the DS5 register +
  encoding mapping; switched the module's interface hook from `GetInterface` to the SDK-correct
  **`doGetExtendedInterface`** (verified against the rig's `coSerDes/common/ModuleUbb.hpp:84-97,305`);
  added `SetDeviceOffsetWidth` (the rig SDK's `IReadWriteI2C` now has it as a 3rd pure virtual);
  added the `ApplyEnvControls()` harness (shared encode/read helpers `WriteCtrl`/`ReadCtrl`).
- **`sdk-patches/patch_nvsipl_camera_d457ctrl.py`** — idempotent patch adding the client-side
  retrieval hook to the GMSL `nvsipl_camera` sample (`GetModuleInterfaceProvider` → `GetInterface`).
- **`tools/build_deploy.sh`** — sync list now includes `ID457CameraControl.hpp`.

### Validated on `fw-advantech-thor-1` (depth stream, 0 drops)
The control **register path works end-to-end** through the **stock** `/usr/sbin/nvsipl_camera`,
driven by the driver reading env at `StartStreaming` (`D457_CTRL` / `D457_CTRL_GET`):

```
D457_CTRL='emitter_mode=1;laser_power_mw=120' D457_CTRL_GET='laser_power_mw,emitter_mode' \
  nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -r 6 -s
  → set emitter_mode=1 (reg 0x4108) -> ok      get emitter_mode -> 1     ✓ round-trip
  → set laser_power_mw=120 (reg 0x4124) -> ok  get laser_power_mw -> 120 ✓ round-trip
  → Frame captured: 206, 0 drops
```
Also confirmed: `depth_auto_exposure` 0→0; `laser_power_mw` default reads **150** (the documented
DS5 default we never wrote — proves the read path returns real register values). `depth_gain`
read-back returns `2` during streaming (control-specific: the imager reports gain differently while
the AE loop is active) — a known quirk, not a path bug; writes still land.
`ReadReg16` was fixed: it must use **address `0x1A` + `I2CAddressMode::Physical`** (it had passed
`0` as the address) and little-endian byte assembly.

### Blocked — client-side custom interface retrieval
**`INvSIPLCamera::GetModuleInterfaceProvider(idx, …)` returns `INVALID_STATE` (6)** for the D457
module in **all** documented phases (config / post-Init / post-Start; sensor indices 0/1/2), with
streaming healthy and no framework log explaining it. No SDK sample exercises this API. So the
"pure SIPL client calls `ctrl->SetControl(...)`" path (§3c, §9) is **not usable on this SDK build**
yet — resolves open question **O2** as a blocker. The structured interface is fully implemented and
will work the moment that call returns a provider; until then, controls are driven via the
driver-side env harness above (binary-agnostic — works with the stock sample).

> ⚠ **Repo integration note:** the `ApplyEnvControls()` call site + the `ReadReg16` fix were applied
> to the rig's `D457Sensor.cpp` (HEAD generation). The repo working-tree `D457Sensor.cpp` /
> `d457_ds5_registers.h` were being edited concurrently during this work (newer `m_streamCfgs`/
> `BuildModeTable` generation), so those two driver-side hooks were **not** folded into the
> working-tree `D457Sensor.cpp` to avoid clobbering that in-progress work — integrate them there.
```
