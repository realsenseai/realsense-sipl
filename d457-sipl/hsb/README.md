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

From the repository root (the D457 drivers and query plugin must be installed for SIPL):

```sh
tools/prepare_hsb.sh d457
tools/hsb.sh build
sudo i2cset -y 9 0x28 0x01 0x00; sleep 3; sudo i2cset -y 9 0x28 0x01 0x1f; sleep 2   # power-cycle the DS5
tools/hsb.sh shell
# inside the container, in the HSB tree:
python3 examples/d457_sipl_player.py --camera-config D457_Camera --frame-limit 900
```

The player shows depth (as a colour map), RGB and IR side by side; `--stream depth|rgb|ir` shows one.
It takes a few seconds before the first frames. Power-cycle the DS5 before each run.
`tools/run_d457_cams.sh` prepares the rig's HSB tree with `prepare_hsb.sh` for the multi-camera runs.

Verified on `fw-advantech-thor-1` (Jetson AGX Thor, L4T R39.2, HSB 2.7.0 in `hololink-demo:2.7.0`):
the tree builds with no errors, and `d457_sipl_player.py` streams depth, RGB and IR from one D457.
