# `d555-sipl/` — RealSense D555 over Ethernet (CoE) via NVIDIA SIPL/UDDF

Everything needed to run a **D555 / D555e** as a SIPL camera over **Camera-over-Ethernet**, where
the camera reaches the Jetson through a Holoscan Sensor Bridge rather than a GMSL SerDes link.

| | |
|---|---|
| Camera | RealSense D555 (HKR SoC) |
| Transport | CoE — camera → Holoscan Sensor Bridge → Jetson Ethernet |
| Output | `libnvuddf_realsense_library.so`, a UDDF module driver loaded by SIPL |
| Validated on | `rs-hsb-thor` — Jetson Thor, L4T R38.4 |

## Layout

| Path | Contents |
| --- | --- |
| `uddf_driver/` | The driver itself (Apache-2.0, RealSense) |
| `hololink/core/` | The Hololink networking core the transport uses — NVIDIA, **Apache-2.0**, from the Holoscan Sensor Bridge |
| `fmt/` | Vendored [fmt](https://github.com/fmtlib/fmt) (MIT) |

Inside `uddf_driver/`:

- `D555ModuleDriver.{cpp,hpp}` / `D555SensorDriver.{cpp,hpp}` — the UDDF module and sensor driver
- `HsbTransportDriver.{cpp,hpp}` — **the Ethernet path**: the CoE bridge transport, implementing
  `ICoEBridgeControl` on top of the Hololink data channel
- `D555ExtendedInterface.hpp` / `HsbExtendedInterface.hpp` — the extra UDDF interfaces exposed
- `RealsenseLibrary.cpp` — driver-library entry point and registration
- `d555_hsl.py` / `d555_data.py` — PyHSL register sequences, compiled into the driver at build time

## Building

The **NVIDIA Jetson Camera SIPL SDK is not vendored here** — it is NVIDIA-licensed and cannot be
redistributed. Build against the copy installed on the Jetson:

```sh
cmake -S d555-sipl -B build/d555            # SIPL_ROOT defaults to /usr/src/jetson_sipl_api/sipl
cmake --build build/d555 -j"$(nproc)"
```

Pass `-DSIPL_ROOT=/path/to/sipl` for an SDK tree elsewhere. The build needs, from that tree:
`uddf/include`, `hsl/{include,pyhsl,integrations/cmake}`, `hsl/lib-target/libnvhslencoder.so`, and
`HsbTransportDriverBase.hpp` — which lives in `uddf/samples/drivers/coe` on L4T R38 and in
`uddf/common/helpers` on R39.2; CMake probes both.

Install the result where SIPL looks for UDDF drivers:

```sh
sudo cp build/d555/libnvuddf_realsense_library.so /usr/lib/nvsipl_uddf/
```

## Running

The Holoscan-side consumers live in the vendored HSB tree:

- `holoscan-sensor-bridge/examples/d555_sipl_player.cpp` — the SIPL capture player
- `holoscan-sensor-bridge/examples/linux_coe_d555_player.py`, `fusa_coe_d555_player.py` and their
  `_dual_stream` variants — the Python CoE players
- `holoscan-sensor-bridge/src/hololink/operators/d555_sipl_capture/` — the Holoscan capture operator
- `holoscan-sensor-bridge/examples/sipl_config/d555_single.json` — the SIPL camera config

Stream selection is by `D555_STREAM`. The SDK's own `nvsipl_coe_camera` / `nvsipl_coe_query_test`
are useful for bring-up; they ship with the SDK and are installed at `/usr/bin` on the Jetson.

## Note on `hololink/core/`

This is a **fork** of the Holoscan Sensor Bridge C++ core, not a copy of the tree vendored at
`../holoscan-sensor-bridge/`: 18 files are identical, 13 differ, and `tools.{cpp,hpp}` exist only
here. It is kept separate because the driver builds against this pinned variant. Unifying the two
is worthwhile but needs testing on the D555 rig, so it has deliberately not been done here.

Tracked in [issue #3](https://github.com/realsenseai/realsense-sipl/issues/3), which records the
concrete divergence. The `alignment == 0` half is fixed here now -- it used to return 0 instead of
throwing. What still differs is the accepted domain: the HSB copy rounds any alignment correctly by
division, while this fork requires a power of two and throws otherwise. Unifying the two is the real fix.

## Licensing

`uddf_driver/` is Apache-2.0 (RealSense). These files previously carried NVIDIA's
`LicenseRef-NvidiaProprietary` header, inherited from the SDK sample they were first created
alongside; the code is RealSense-authored (it shares 17 lines with NVIDIA's `SampleHsbDriver.cpp`)
and the header was corrected to match. `hololink/core/` keeps its NVIDIA Apache-2.0 headers, which
Apache-2.0 requires be retained; `fmt/` keeps its MIT notice.

`hsb/src/hololink/operators/d555_sipl_capture/d555_sipl_fmt.hpp` keeps **NVIDIA's** notice and
nothing else: it is a byte-for-byte copy of upstream's `sipl_capture/sipl_fmt.hpp`, so replacing the
notice would have dropped the attribution Apache-2.0 §4(c) requires be retained. The D457 side's
`d457_sipl_fmt.hpp` is the same formatter with four extra `FMT()` entries, so it carries both
notices — NVIDIA's for the copied block, RealSense's for the addition.

⚠ **This is not the whole set, and an earlier version of this section wrongly said it was.** That
claim came from an audit that only compared each file against a byte-identical or same-named
upstream twin, which by construction can only find verbatim copies — Apache-2.0 §4(c) covers
derivative works too. Re-run against all 363 upstream files with no name assumption, **15 files
under `*/hsb/` carry a RealSense-only notice while sharing 45–95% of their substantive lines with an
upstream file**, `d555_sipl_player.cpp` (95%), `d555_sipl_capture.hpp` (94%) and
`d555_sipl_capture.cpp` (87%) among them. They need the same treatment as `d457_sipl_fmt.hpp`:
NVIDIA's notice retained, RealSense's added. Tracked in #6.
