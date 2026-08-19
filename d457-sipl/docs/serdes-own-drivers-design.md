# Design: `D457Max9295` / `D457Max96712` — owning the serdes drivers instead of patching

Status: **design, pre-implementation** · Target: fw-advantech-thor-1 (Thor, L4T r39.2) · 2026-06-25

## Goal

Replace the three runtime edits to NVIDIA's shipped serdes drivers
(`patch_max9295_d457.py`, `patch_max96712_d457.py`) with **our own UDDF drivers** that we
register and select, so the bring-up no longer mutates `/usr/src/jetson_sipl_api` and survives
SDK re-installs. Decide, per device, whether "own driver" is actually cleaner than the patch.

> The `libnvsipl.so` lane-count binary patch (`patch_libnvsipl_lanes2.sh`) is **out of scope** —
> the lane count is derived inside a precompiled, closed lib (and the deser's own private
> `SetMipiOutputMode` couples `csiPort → 2x4/4x2` the same way). No driver we write can reach it.
> It stays regardless of what we decide here.

## What the SDK interfaces actually allow (ground truth from the r39.2 headers)

The two devices live at **different layers**, and that asymmetry drives the whole design.

### Serializer — instantiated by *our* module driver (clean)

`SerializerUbb : public DeviceUbb, public IGmslSerializer` — a small, fully-virtual interface:

| Method | Role |
|---|---|
| `SerInit(ctx)` | the HAWK-vs-D457 fork lives here (`numSensors==2` vs our `==1`) |
| `SerPrepareForModuleInit` / `SerFinalizeInit` | GPIO + FSYNC sequencing |
| `SerGetInfo`, `SerEnableErrorPin`, `SerConfigureGPIOForwarding` | info / error / GPIO fwd |
| `GetName / Configure / GetDeviceTable / GetGpioPinTable` (DeviceUbb) | identity + I2C/GPIO tables |
| `ProbeHardware` / `Init` | **finalized no-ops** in `SerializerUbb` (nothing to override) |

Crucially, **`D457Module::doCreateUbbObjects()` `new`s the serializer itself**
(`std::make_unique<max9295::MAX9295>(config)` today). So swapping in our own class is a
**one-line change in code we already own** — no framework dispatch, no JSON, no NVIDIA edit.

Register programming has two paths, both available from `context.hwAccess`:
- `SubmitSequence(hsl::<name>)` — the compiled-`.py` HSL sequences (what stock uses), **or**
- direct `hwAccess->ReadI2C/WriteI2C(...)` — exactly what stock `CheckPresence()` already does.

→ **Our serializer can program every D457 register inline in C++** and skip the `.py`/HSL
compile step entirely. That removes a build dependency, not just a patch.

### Deserializer — a top-level framework driver, mostly *private* base logic (friction)

`MAX96712 final : public MAX967XX`, and `MAX967XX : public IDriver, public IGmslDeserializer`.
It is **not** created by our module — the framework instantiates it by **name** from JSON
(`deserializerInfo.name = "Max96712GmslDeserializer"`). The subclass override surface is narrow:

**Overridable (`protected virtual`) — the clean hooks:**
- `doCheckPresence`
- `doSubmitSetMipiDPhy(ctx, mipiConfig, is4x2Mode)` ← **the PHY / lane-rate values** (one of our patches)
- `doSetInternalFsync`
- `doSetDualSensorVideoPipelineMappingLinkC/D`
- `ConfigureDualSensorPipelineMappingForLink` (virtual)

**NOT overridable — `private` in `MAX967XX`, where our other two patches live:**
- `IsDualSensorConfig()` ← we force this `false` (private — cannot subclass around it)
- `SetMipiOutputMode()` ← the `csiPort → 2x4/4x2` coupling (private)
- `ConfigureVideoPipelineMappingForLink()` + the pixel-map / VC-demux sequences (private + shared HSL)
- the 4x2 **PLL-lock poll mask** (in `MAX967XXHsl.py`, shared with the base)

So a *pure subclass* gets us the PHY/datarate patch cleanly, but **cannot** express the
VC-demux pixel-map, the `IsDualSensorConfig→false` flip, or the relaxed PLL mask — those are
sealed in the base. Fully owning the deser means **forking the entire `MAX967XX` + `MAX96712`
implementation** into our tree.

## Mapping each current patch to an "own-driver" home

| Current patch hunk | Lands in | Clean? |
|---|---|---|
| Ser: `SerInit` `numSensors==1` branch | `D457Max9295::SerInit` | ✅ fully ours |
| Ser: RCLKOUT 0x03F1 + GPIO 0x02BE/BF | `D457Max9295::SerInit` (inline writes) | ✅ |
| Ser: pipe-per-VC routing (0x0309…0x031F) | `D457Max9295::SerInit` (inline writes) | ✅ |
| Deser: PHY/lane-rate (2x4-mode, 2-lane, 2500 Mbps) | `D457Max96712::doSubmitSetMipiDPhy(is4x2Mode)` | ✅ override hook exists |
| Deser: relaxed 4x2 PLL-lock mask (0xF0→0x20) | base `.py` / private | ⚠ needs base fork or stays a patch |
| Deser: VC-demux pixel-map (pipe0/1/2, 0x0100=0x23) | private `ConfigureVideoPipelineMappingForLink` | ⚠ needs base fork or stays a patch |
| Deser: `IsDualSensorConfig → false` | **private** | ⚠ needs base fork or stays a patch |
| `libnvsipl.so` lane count | closed precompiled lib | ❌ binary patch stays (out of scope) |

## Proposed design

### Part A — `D457Max9295` serializer (recommended: do it)

```
d457-sipl/uddf_driver/serdes/
  D457Max9295.hpp / .cpp        # final : public uddf::cdd::gmslubb::SerializerUbb
```

- Implements the 6 `IGmslSerializer` + 4 `DeviceUbb` methods.
- `SerInit`: `CheckPresence` (read DEVICE_ID 0x000D, expect 0x91/MAX9295A) → then **inline
  `hwAccess->WriteI2C`** for: I2C address-translation slot (host 0x1A→phys 0x10), RCLKOUT/GPIO
  (0x03F1/0x02BE/0x02BF), and the pipe-per-VC block (depth VC0/0x2E, RGB VC1/0x1E, IR VC2/0x2E).
  These are the exact registers in `patch_max9295_d457.py`'s `set_ser_video_phy_clock_max9295a`,
  just issued from C++ instead of a generated HSL sequence.
- `GetDeviceTable`: one entry, addr 0x40, offsetWidth 2, dataWidth 1 (same as stock).
- Wire-in: in `D457Module::doCreateUbbObjects`, replace `max9295::MAX9295` with
  `d457::D457Max9295`. **One line.** No JSON change, no new registered driver.
- Net: **`patch_max9295_d457.py` retired.** Low risk, self-contained, removes the HSL `.py`
  build step for the serializer.

### Part B — `D457Max96712` deserializer (two options — pick one)

**Option B1 — Thin subclass (smallest code, does NOT fully retire the patch).**
```
d457-sipl/uddf_driver/serdes/
  D457Max96712.hpp / .cpp       # final : public uddf::cdd::MAX96712
  D457DeserLibrary.cpp          # uddf_discover_drivers() → DriverInfo.name = "D457Max96712"
```
- Override `doSubmitSetMipiDPhy` → emit d4xx's exact 2x4-mode / 2-lane / 2500 Mbps PHY block.
  This **legitimately replaces** the `patch_4x2()` hunk via a sanctioned hook.
- Override `doCheckPresence` if needed (D457 link quirks).
- Select it by setting `deserializerInfo.name = "D457Max96712"` in `d457_gmsl.json`.
- **Still cannot** override `IsDualSensorConfig` (private), the pixel-map (private), or the PLL
  mask (shared `.py`). So those **remain** as the (smaller) `patch_max96712_d457.py` —
  i.e. B1 shrinks the deser patch to the pixel-map + PLL + force-single hunks, doesn't kill it.

**Option B2 — Full fork (fully retires the patch, larger maintenance surface).**
- Copy `MAX967XX.{hpp,cpp}` + `MAX96712.{hpp,cpp}` + `MAX967XXHsl.py`/`MAX96712Hsl.py` into our
  tree as `D457Max96712*`, change `IsDualSensorConfig`/`SetMipiOutputMode`/pixel-map/PLL directly,
  register under a new name, point JSON at it.
- Removes all runtime dependence on NVIDIA's deser source — but we now **own a large copy** that
  drifts from every SDK update. This is arguably *worse* than a 4-hunk patch we can re-apply.

### Registration (both parts)

Follow the existing `D457Library.cpp` pattern (`IDriverEnumerator` + `DriverInfo` +
`uddf_discover_drivers()`). Serializer needs **no** registration (module owns it). Deserializer
is a separate registered `IDriver`, selected by `DriverInfo.name` matching the JSON — same
mechanism, **no framework edit**.

## Recommendation

1. **Do Part A (`D457Max9295`) now** — it cleanly retires `patch_max9295_d457.py`, is fully
   inside code we own, and removes a build step. Clear win.
2. **Part B: prefer B1 (thin subclass)** for the PHY hunk via `doSubmitSetMipiDPhy`, and **keep
   the remaining 3 deser hunks as a (shrunken) patch.** Rationale: the items that block a clean
   subclass (`IsDualSensorConfig`, pixel-map, PLL mask) are sealed `private`/shared, and forking
   the whole base (B2) trades a small re-appliable patch for a large, drift-prone copy.
3. **`libnvsipl.so` patch stays** — no interface exists; unaffected by this work.

Net after A+B1: **one patch fully gone (serializer), the deser patch reduced to 3 hunks**, and
the only irreducible edit is the closed-lib lane binary patch. Full elimination is only possible
via B2, at a maintenance cost that likely isn't worth it.

## Open items to verify on-rig before coding

- Exact direct-write method name on `IHardwareAccess` for arbitrary regs (stock uses
  `ReadI2C(addr, reg, len, buf, I2CAddressMode::Physical)`; confirm the matching `WriteI2C`).
- Whether `doSubmitSetMipiDPhy`'s `mipiConfig` struct lets us set the 2500 Mbps datarate +
  2x4-mode without also tripping the private `SetMipiOutputMode` (which keys off `csiPort`).
- Confirm `DriverInfo.name` for the stock deserializer (the JSON uses
  `"Max96712GmslDeserializer"`) and where it is registered (under `uddf/libraries/`), to clone
  the registration for `D457Max96712`.
- FSYNC TX-ID for D457 (`GetModuleFsyncTxId` currently `0x02`, marked TODO).
