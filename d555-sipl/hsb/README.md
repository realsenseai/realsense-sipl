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

| Firmware | CoE channel | Single-stream players | Dual-stream CoE players |
| --- | --- | --- | --- |
| 7.58.46389.16966, 7.59.0.16345 | Always 4 | Work | Do not work: both streams arrive on channel 4 |
| 7.58.46389.17264 (channel fix, not released yet) | The one the host configures | Work | Work at 640x360 |

`linux_coe_d555_player.py` defaults to `--coe-channel 4`, which works with all three.

The dual-stream CoE players default to 1280x720 on both streams. That is over the camera's CoE limit
of about 30k packets/s per stream (1280x720 at 30 fps needs about 39k), on every firmware. Pass
`--camera-mode-depth 8 --camera-mode-rgb 10` for 640x360 at 30 fps.

A CoE run that fails to stop leaves the camera's stream marked as running, and the next start gets
no frames. Reboot the camera (`reboot` on its UART console, or a power cycle) before the next run.

The D555 player, dual-stream and PeopleNet examples for HSB's Linux (software RoCE) and RoCE
receivers live in `../../d555-hsb/`, the same project as `realsense/examples/d555` in the Holoscan
Sensor Bridge Hub; `../../d555-roce/` has the RoCE test tools.

## Use

From the repository root (the UDDF driver in `../` must be installed for SIPL):

```sh
tools/prepare_hsb.sh d555
tools/hsb.sh build
tools/hsb.sh shell
# inside the container, in the HSB tree:
python3 examples/linux_coe_d555_player.py --camera-mode 8 --coe-interface mgbe0_0
python3 examples/linux_coe_d555_dual_stream.py --camera-mode-depth 8 --camera-mode-rgb 10 --coe-interface mgbe0_0
D555_STREAM=depth build/examples/d555_sipl_player --json-config examples/sipl_config/d555_single.json \
    --camera-config depth_640x360_30fps          # D555_STREAM=rgb for RGB
python3 examples/fusa_coe_d555_player.py --camera-mode 8 --coe-interface mgbe0_0
python3 examples/fusa_coe_d555_dual_stream.py --camera-mode-depth 8 --camera-mode-rgb 10 --coe-interface mgbe0_0
```

`--camera-mode` is the depth profile index (0-16), or 17 plus the RGB profile index; 8 and 27 are
640x360 at 30 fps. Let each run end by itself (`--frame-limit <n>`): a SIPL run killed mid-stream
leaves the MGBE's CoE path unusable until the host reboots. SIPL and FuSa runs turn on the Jetson MGBE's
hardware CoE mode, which takes the AVTP frames the Linux CoE receiver reads from its socket; run the
Linux CoE player before them, or reboot the host in between.

Verified on `rs-hsb-thor` (Jetson AGX Thor, L4T R38.4, HSB 2.7.0 in `hololink-demo:2.7.0`,
firmware 7.59.0.16345, 7.58.46389.16966 and 7.58.46389.17264): the tree builds with no errors, and
`d555_sipl_player` (depth and RGB), `linux_coe_d555_player.py` and `fusa_coe_d555_player.py` stream
640x360 at 30 fps. On 7.58.46389.17264, `linux_coe_d555_dual_stream.py` and
`fusa_coe_d555_dual_stream.py` also stream both at 640x360 at 30 fps, each on its own channel.
Every player shows depth as a colour map and RGB as video, through the same `ImageDecoder`.
