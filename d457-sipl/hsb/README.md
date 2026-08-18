# `d457-sipl/hsb/` — the D457 project's Holoscan Sensor Bridge code

The D457 Holoscan consumer — the `D457SIPLCaptureOp` operator, its Python binding and the player
apps — belongs to the HSB tree at build time, but is owned by *this* project, not by the vendored
`../../holoscan-sensor-bridge/`.

## Why it lives here

`d457-sipl` (D457 over GMSL) and `d555-sipl` (D555 over CoE) are independent projects that share
this repository, and they target **incompatible SIPL APIs**:

| Project | Jetson release | SIPL config API |
| --- | --- | --- |
| `d555-sipl` | L4T **R38** | `CameraSystemConfig` / `GetCameraSystemConfig` — what upstream HSB 2.5.0 is written against |
| `d457-sipl` | L4T **R39.2** | `sensorconfig::SensorSystemConfig`, which replaced it |

Because of that, building D457 requires *disabling* upstream's own `sipl_capture` operator and the
C++ `sipl_player` — edits that would break the D555 project if they were made in a shared tree.
They previously stayed apart because each project had its own superproject branch pinning the HSB
submodule to its own HSB branch (`d457-holoscan-sipl` vs `main-2.5.0`), so the two wirings never
met. With both projects merged into one master that isolation is gone, so each project keeps its
Holoscan code — and its own copy of the shared files it must replace — here instead.

## Layout

| Path | Contents |
| --- | --- |
| `src/hololink/operators/d457_sipl_capture/` | The C++ capture operator: RAW16-only (no Tegra ISP), N-pipeline, one acquire thread per pipeline, multi-camera via `link_mask` + `apply_link_offsets()` |
| `python/hololink/operators/d457_sipl_capture/` | The pybind11 binding |
| `examples/` | `d457_sipl_player.py` (the grid-tiled multi-stream player) and `d457_tao_peoplenet.py` |
| `wiring/` | Full replacements for the four HSB files that must register the above — derived from upstream 2.5.0 plus this project's entries only, so they never reference D555 code |

## Use

```sh
tools/graft_hsb.sh d457 [<hsb-tree>]     # from the repository root; defaults to ./holoscan-sensor-bridge
cmake -S holoscan-sensor-bridge -B build/hsb -DHOLOLINK_BUILD_SIPL=1
cmake --build build/hsb -j"$(nproc)"
```

Graft one project at a time — the script refuses to stack the other on top. Verified on
`fw-advantech-thor-1` (L4T R39.2): `d457_sipl_capture` and its Python binding build, with no D555
target pulled in.
