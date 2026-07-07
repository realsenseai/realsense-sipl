# Reference: NVIDIA SIPL/UDDF serdes drivers (as installed on the rig)

A local copy of the stock NVIDIA serializer/deserializer drivers + their UBB base classes and
registration, pulled from **fw-advantech-thor-1** so we can develop the own-driver work
(`D457Max9295` / `D457Max96712`, see `../../docs/serdes-own-drivers-design.md`) against the real
source instead of guessing.

## Provenance
- Source: `…/jetson_sipl_api/sipl/uddf/` on fw-advantech-thor-1 (Thor, L4T r39.2, Jetson SIPL API R39.2).
- Pulled 2026-06-25. Layout below mirrors the SDK's `uddf/` tree.

## ⚠ Licensing — DO NOT commit/redistribute
These files carry `SPDX-License-Identifier: LicenseRef-NvidiaProprietary` ("distribution … is
strictly prohibited"). They are here for **local reference only** and are **gitignored**
(`.gitignore` in this dir). Do not check them into a shared/pushed branch or copy them verbatim
into our own drivers — reimplement against the interfaces, don't paste proprietary source.

## Layout
```
drivers/common/            UBB base classes (DeviceUbb, SerializerUbb, SensorUbb, EepromUbb)
drivers/serializers/MAX9295/      stock MAX9295 serializer driver
drivers/deserializers/MAX96712/   MAX96712 device subclass (thin)
drivers/deserializers/MAX967XX/   MAX967XX base (the heavy deser implementation)
libraries/deserMAX96712/   deserializer driver registration (DriverInfo "Max96712GmslDeserializer")
coSerDes/common/ModuleUbb.hpp   the ModuleUbb convenience layer our D457Module derives from
```

## Patched vs pristine (important)
The rig's SDK is the one we've been patching in place. So the **non-suffixed** source files
already contain our edits; the **`.bak` / `.pre_*`** files are earlier (closer-to-pristine)
snapshots. Map of what each current file's patch state is:

| File | State | Pristine snapshot here? |
|---|---|---|
| `serializers/MAX9295/MAX9295.cpp` | **PATCHED** (`numSensors==1` branch) | no |
| `serializers/MAX9295/MAX9295Hsl.py` | **PATCHED** (`set_ser_video_phy_clock_max9295a`) | partial: `.pre_rclk.bak` (pre-RCLK), `.prepipe` (pre pipe-per-VC) — neither is fully pristine NVIDIA |
| `deserializers/MAX967XX/MAX967XX.cpp` | **PATCHED** (`IsDualSensorConfig→false`) | yes: `.pre_forcesingle.bak` |
| `deserializers/MAX967XX/MAX967XXHsl.py` | **PATCHED** (PHY/PLL/pixel-map) | intermediate: `.pre_dualvc.bak`, `.pre_rgb.bak`, `.preir`, `.prepipe` |
| `deserializers/MAX96712/MAX96712.{cpp,hpp}` | **pristine** (we never patched the device subclass) | n/a |
| `drivers/common/*Ubb.hpp`, `coSerDes/common/ModuleUbb.hpp` | **pristine** | n/a |
| `libraries/deserMAX96712/MAX96712Library.cpp` | **pristine** | n/a |

The exact patch hunks (and the rationale) are in `../../sdk-patches/patch_max9295_d457.py` and
`patch_max96712_d457.py`.

## Notes
- There is **no `MAX9295Hsl.hpp`** in the tree — the `hsl::…` symbols the `.cpp` references are
  generated from the `.py` by the pyhsl tooling at build time. Our own drivers can sidestep HSL
  entirely by issuing `hwAccess->WriteI2C(...)` directly (see the design doc).
- `__pycache__` was excluded.
