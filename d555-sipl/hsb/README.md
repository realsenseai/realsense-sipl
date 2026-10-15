# `d555-sipl/hsb/` — the D555 project's Holoscan Sensor Bridge code

The D555 Holoscan consumers — the `D555SIPLCaptureOp` operator, the image decoder, the CoE players
and the D555 sensor model — are owned by this project. `tools/prepare_hsb.sh d555` copies them into
a Holoscan Sensor Bridge 2.7.0 tree and registers them with `hsb-registration.patch`. The D555
capture operator targets the L4T R38 SIPL API (`CameraSystemConfig`); see
`../../d457-sipl/hsb/README.md` for the D457 side, which targets R39.2.

## Layout

| Path | Contents |
| --- | --- |
| `src/hololink/operators/d555_sipl_capture/` | The C++ SIPL capture operator for the D555 |
| `src/hololink/operators/image_decoder/` + `python/.../image_decoder/` | The image decoder used by the CoE players |
| `python/hololink/sensors/d555/` | The D555 sensor model and modes |
| `examples/` | `d555_sipl_player.cpp`, the `linux_coe_*` / `fusa_coe_*` players, and `sipl_config/d555_single.json` |
| `hsb-registration.patch` | Registers the above with HSB 2.7.0: CMake (operators, Python bindings, the player), the operator and sensor registries |

## Camera firmware

The players expect D555 firmware 7.59.0.16345 or later. It sends every CoE stream on channel 4, so
`linux_coe_d555_player.py` defaults to `--coe-channel 4`, and the dual-stream CoE players do not work
with it (both streams arrive on the same channel).

The D555 player, dual-stream and PeopleNet examples for HSB's Linux (software RoCE) and RoCE
receivers live in `../../d555-hsb/`, the same project as `realsense/examples/d555` in the Holoscan
Sensor Bridge Hub; `../../d555-roce/` has the RoCE test tools.

## Use

```sh
tools/prepare_hsb.sh d555                # from the repository root; prepares ./holoscan-sensor-bridge
```

then build inside the HSB container as described in the repository README (with
`-DHOLOLINK_BUILD_SIPL=1 -DHOLOLINK_BUILD_FUSA=1`). SIPL and FuSa runs turn on the Jetson MGBE's
hardware CoE mode, which takes the AVTP frames the Linux CoE receiver reads from its socket; run the
Linux CoE player before them, or reboot the host in between.

Verified on `rs-hsb-thor` (Jetson AGX Thor, L4T R38.4, HSB 2.7.0 in `hololink-demo:2.7.0`,
firmware 7.59.0.16345): the tree builds with no errors, and `d555_sipl_player` (depth and RGB),
`linux_coe_d555_player.py` and `fusa_coe_d555_player.py` stream 640x360 at 30 fps.
