# D457 GMSL UDDF camera-module driver — skeleton

A starting skeleton for the Intel RealSense **D457** as a **GMSL UDDF camera-module driver**
for NVIDIA SIPL-for-L4T on **Jetson Thor (L4T r39.2 / JetPack 7.2)**. Produces
`libnvuddf_d457_library.so`, paired at runtime with the stock `MAX9295` serializer and
`Max96712GmslDeserializer` drivers, and driven via `nvsipl_camera -t ../d457_gmsl.json`.

## Files
| File | Role | Models (SDK) |
|---|---|---|
| `D457Sensor.{hpp,cpp}` | DS5-ASIC sensor UBB: mode select, stream start/stop, status poll | `drivers/sensors/AR0234/`, `samples/drivers/gmsl/SampleSensorUbb` |
| `D457Module.{hpp,cpp}` | Module = D457 sensor + MAX9295 serializer (ModuleUbb) | `samples/drivers/gmsl/SampleModuleDriver`, dev-guide `ModuleUbb` |
| `D457Library.cpp` | `uddf_discover_drivers` registration, `DriverInfo.name="D457"` | `libraries/moduleR0SIM623/R0SIM623Library.cpp` |
| `CMakeLists.txt` | builds + installs the `.so` | `samples/drivers/gmsl/CMakeLists.txt` |
| `../d457_ds5_registers.h` | DS5 mode/stream/status register tables (ported from the Hololink path) | — |
| `../d457_gmsl.json` | the `CameraSystemConfig` SIPL loads (`-t`) | on-device `AR0234CS_HAWK` schema |

## How it fits the SIPL GMSL model
- D457 is **not** a raw Bayer sensor: the DS5 ASIC outputs processed depth(Z16)/IR(Y8)/color(YUYV)
  as a single 16-bpp (`UYVY8_1X16`) CSI stream, controlled via the DS5 mux at I2C `0x1A`.
- So the sensor UBB is a **thin control path** (mode table + stream `0x1000` + status poll
  `0x4800`/`0x1004`), not exposure/gain/AWB. ISP is bypassed; capture is ICP/raw passthrough.
- The MAX9295 serializer + MAX96712 deserializer are **stock SDK drivers** — reused unchanged.

## Build & install (on the Thor rig)
1. Get the full SDK (the rig's `/usr/src/jetson_sipl_api` was a stale CoE-only carve):
   ```bash
   cd /tmp && wget -q https://developer.nvidia.com/downloads/embedded/L4T/r39_Release_v2.0/release/Jetson_SIPL_API_R39.2.0_aarch64.tbz2
   sudo tar xjf Jetson_SIPL_API_R39.2.0_aarch64.tbz2 -C /
   ```
2. Drop this `driver/` dir into the SDK and register it:
   ```bash
   sudo cp -r driver /usr/src/jetson_sipl_api/sipl/uddf/cdd_d457
   sudo cp ../d457_ds5_registers.h /usr/src/jetson_sipl_api/sipl/uddf/cdd_d457/
   # add: add_subdirectory(uddf/cdd_d457)  to /usr/src/jetson_sipl_api/sipl/CMakeLists.txt
   ```
3. Build & install:
   ```bash
   cd /usr/src/jetson_sipl_api/sipl && mkdir -p build && cd build
   cmake .. && make -j all_drivers
   sudo make install            # → /usr/lib/nvsipl_drv/libnvuddf_d457_library.so
   ```
4. Place the config and run (Phase-1 control path, no streaming):
   ```bash
   sudo cp ../../d457_gmsl.json /var/nvidia/nvcam/settings/sipl/   # or pass an absolute path
   nvsipl_camera -t /path/to/d457_gmsl.json -v 4      # expect Init() OK, link-lock, D457 reachable
   nvsipl_camera -t /path/to/d457_gmsl.json -R -s -v 3  # Phase 2: raw output + FPS
   ```
   Unbind the V4L2 `d4xx` first (it contends for the CSI/I2C); load `cdi_mgr.ko`; ensure a
   SIPL `cdi-mgr` device-tree node exists for the D457 CSI port (see ../../.triage/FINDINGS.md §6).

## TODO / verify-points before first build (grep `TODO`)
1. **IHardwareAccess raw-I2C API** — `WriteReg16`/`ReadReg16` in `D457Sensor.cpp` are stubs;
   implement against the installed `uddf/cdi/IHardwareAccess.hpp` (see how `AR0234.cpp` issues
   register writes — direct I2C vs HSL dynamic sequence).
2. **MAX9295 UBB class/namespace/ctor** + **ModuleUbb API** (`doCreateUbbObjects`,
   `addSensorUbb`/`addSerializerUbb`) — confirm against `drivers/serializers/MAX9295/MAX9295.hpp`
   and `coSerDes/common/ModuleUbb.hpp`; fix the include + CMake link in `D457Module.cpp`/`CMakeLists.txt`.
3. **DS5 identity register** for `ProbeHardware` (read a stable version/ID).
4. **inputFormat / CSI port / lanes / deser I2C addr** in `../d457_gmsl.json` (see ../../.triage/FINDINGS.md §6).
5. **Multi-stream** (depth+RGB on separate virtual channels) — currently depth-only.
6. **FSYNC TX ID** for D457 (`GetModuleFsyncTxId` returns 0x02 like AR0234-HAWK/MAX9295; confirm).
