# `d457-sipl/hsb/` — the D457 project's Holoscan Sensor Bridge code

The D457 Holoscan consumer — the `D457SIPLCaptureOp` operator, its Python binding and the player
apps — is owned by this project. `tools/prepare_hsb.sh d457` copies it into a Holoscan Sensor Bridge
2.7.0 tree and registers it with `hsb-registration.patch`.

## Why one project per HSB tree

`d457-sipl` (D457 over GMSL) and `d555-sipl` (D555 over CoE) are independent projects that share
this repository, and their capture operators target different SIPL API generations:

| Project | Jetson release | SIPL config API |
| --- | --- | --- |
| `d555-sipl` | L4T **R38** | `CameraSystemConfig` / `GetCameraSystemConfig` |
| `d457-sipl` | L4T **R39.2** | `sensorconfig::SensorSystemConfig`, which replaced it |

HSB 2.7.0's own `sipl_capture` builds against both (it selects the API at compile time), so unlike
HSB 2.5.0 nothing upstream has to be disabled for D457. The two capture operators still do not build
against each other's API, so an HSB tree is prepared for one project at a time.

## Layout

| Path | Contents |
| --- | --- |
| `src/hololink/operators/d457_sipl_capture/` | The C++ capture operator: RAW16-only (no Tegra ISP), N-pipeline, one acquire thread per pipeline, multi-camera via `link_mask` + `apply_link_offsets()` |
| `python/hololink/operators/d457_sipl_capture/` | The pybind11 binding |
| `examples/` | `d457_sipl_player.py` (the grid-tiled multi-stream player) and `d457_tao_peoplenet.py` |
| `hsb-registration.patch` | Registers the above with HSB 2.7.0: the operator and binding in the SIPL block, `D457SIPLCaptureOp` in the operator registry, the two examples |

## Use

```sh
tools/prepare_hsb.sh d457                # from the repository root; prepares ./holoscan-sensor-bridge
```

then build inside the HSB container as described in the repository README.

**Not yet verified on HSB 2.7.0.** The operator built and streamed on HSB 2.5.0 on
`fw-advantech-thor-1` (L4T R39.2); on 2.7.0 it has only been compile-checked, so build and run it on
the D457 rig before relying on it. `tools/run_d457_cams.sh` prepares the rig's HSB tree with
`prepare_hsb.sh`.
