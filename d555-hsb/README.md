# RealSense D555 over Holoscan Sensor Bridge

> This folder is the same project as
> [`realsense/examples/d555`](https://github.com/nvidia-holoscan/holoscan-sensor-bridge-hub/tree/main/realsense/examples/d555)
> in the Holoscan Sensor Bridge Hub; change both together. It
> runs on stock HSB 2.7.0 and needs neither `hsb-patches/` nor `tools/prepare_hsb.sh`.

Python examples that stream the [RealSense D555](https://realsenseai.com/) depth camera into Holoscan applications over Ethernet. The D555 implements the Holoscan Sensor Bridge (HSB) protocol in its own firmware, so it is enumerated, configured and received with the stock HSB 2.7.0 classes, with no FPGA board in between.

| Example | What it shows |
| --- | --- |
| `d555_player.py` | One stream, depth (colorized) or RGB, in a Holoviz window |
| `d555_dual_stream.py` | Depth and RGB side by side, each on its own data channel |
| `d555_peoplenet.py` | TAO PeopleNet person and face detection on the RGB stream |

The camera sends every frame as RoCEv2 RDMA writes. Each example takes either receiver with `--receiver`:

- `linux` (default): HSB's `LinuxReceiverOperator`, which handles the packets in software on any network interface, such as the Jetson AGX Thor's MGBE ports.
- `roce`: HSB's `RoceReceiverOp`, which needs an RDMA-capable NIC such as the ConnectX-7 on IGX Thor.

## Overview

The `d555` package holds everything camera-specific:

- `d555/camera.py` finds the camera and starts and stops its streams through the firmware's stream command register.
- `d555/profiles.py` lists the firmware's stream profiles (resolution and frame rate per index).
- `d555/receivers.py` creates the selected HSB receiver for a stream.
- `d555/operators.py` has `D555DecoderOp`, which turns a raw frame into an RGB image on the GPU (colorized Z16 depth, or YUYV converted to RGB), and `FrameStatsOp`, which checks frame counters and reports the frame rate.

Nothing is compiled: the GPU kernels run through CuPy, and no HSB source changes are needed.

## Tested Configuration

| Component | Jetson AGX Thor | IGX Thor |
| --- | --- | --- |
| Holoscan Sensor Bridge | 2.7.0 (`1df133d`), `hololink-demo:2.7.0` container | 2.7.0 (`1df133d`), `hololink-demo:2.7.0` container |
| Holoscan SDK | 4.4.0 (in the HSB container) | 4.4.0 (in the HSB container) |
| Developer kit | NVIDIA Jetson AGX Thor Developer Kit | NVIDIA IGX Thor Developer Kit, with an RTX PRO 6000 Blackwell |
| Operating system | JetPack 7 (L4T R38.4.0) | IGX OS 2.0 (L4T R38.5.0) |
| FPGA bitstream | None: the HSB IP runs in the camera firmware | None: the HSB IP runs in the camera firmware |
| Sensor | RealSense D555 | RealSense D555 |
| Sensor firmware | 7.58.46389.16966 and 7.59.0.16345 (HSB, early access) | 7.58.46389.16966 (HSB, early access) |
| Receiver | `linux`, on the Thor's `mgbe0_0` (10 GbE) | `roce` on the ConnectX-7, also with `--gpu` into the RTX PRO 6000; `linux` on the same port |

On IGX Thor, `--receiver roce` uses the first RDMA device unless `--ibv-name` names one. If the camera is not on that port, pass its device, for example `--ibv-name roceP4p3s0f1` (`ls /sys/class/infiniband`).

## Hardware

- A RealSense D555 with HSB-capable firmware (see [Setup](#setup)).
- A host that runs the HSB 2.7.0 container:
  - Jetson AGX Thor Developer Kit, with the camera on an MGBE port (`--receiver linux`), or
  - IGX Thor with a ConnectX-7, with the camera on the ConnectX-7 (`--receiver roce` or `linux`).
- A display for Holoviz, or `--headless`.

## Setup

1. **Camera firmware.** Flash the D555 HSB firmware, 7.58.46389.16966 (recommended) or 7.59.0.16345. Boot the camera in USB mode, run `dfu usb` on its UART console, flash with `dfu-util`, and check the version with `gvd`. After a reboot with an Ethernet link, the camera boots in Ethernet mode at `192.168.0.2`.

2. **Host network.** Give the host interface facing the camera an address on the camera's subnet, for example on a Jetson AGX Thor:

   ```sh
   sudo nmcli con add con-name hololink-mgbe0 ifname mgbe0_0 type ethernet ip4 192.168.0.101/24
   sudo nmcli con up hololink-mgbe0
   ```

   The camera announces itself once a second. Inside the HSB container, `hololink enumerate` should list it:

   ```text
   INFO ... mac_id=98:4F:EE:19:E1:5B hsb_ip_version=0X2602 ... ip_address=192.168.0.2 ... interface=mgbe0_0
   ```

3. **HSB container.** Build HSB 2.7.0 as described in the [HSB build guide](https://docs.nvidia.com/holoscan/sensor-bridge/getting-started/build):

   ```sh
   git clone --branch 2.7.0 https://github.com/nvidia-holoscan/holoscan-sensor-bridge.git
   cd holoscan-sensor-bridge
   sh docker/build.sh --igpu    # Jetson AGX Thor; use --dgpu on IGX Thor with a discrete GPU
   ```

4. **PeopleNet model** (for `d555_peoplenet.py` only), into HSB's `examples` directory:

   ```sh
   wget --content-disposition 'https://api.ngc.nvidia.com/v2/models/org/nvidia/team/tao/peoplenet/pruned_quantized_decrypted_v2.3.3/files?redirect=true&path=resnet34_peoplenet_int8.onnx' -O examples/resnet34_peoplenet_int8.onnx
   ```

## Build

There is nothing to build. The examples run inside the HSB 2.7.0 container, which provides Holoscan, `hololink` and the Python packages in `requirements.txt`.

## Run

Start the container from the HSB checkout, then run an example from this directory. `docker/demo.sh` mounts the current directory and `$HOME`; adjust the path below to wherever this project is checked out.

```sh
cd holoscan-sensor-bridge
sh docker/demo.sh
# inside the container:
cd <realsense-sipl checkout>/d555-hsb
python3 d555_player.py --stream depth
python3 d555_player.py --stream rgb
python3 d555_dual_stream.py
HSB_ROOT=/path/to/holoscan-sensor-bridge python3 d555_peoplenet.py
```

Common options:

| Option | Meaning |
| --- | --- |
| `--receiver linux\|roce` | Software receiver on any NIC, or `RoceReceiverOp` on an RDMA NIC |
| `--hololink <ip>` | Camera address (default `192.168.0.2`) |
| `--profile <n>`, `--depth-profile`, `--rgb-profile` | Firmware profile index; `python3 -m d555.profiles` lists them |
| `--ibv-name`, `--ibv-port` | RDMA device and port for `--receiver roce` |
| `--gpu <uuid>` | On IGX, a discrete GPU's UUID from `nvidia-smi -L`, so frames land in its memory |
| `--headless`, `--frame-limit <n>` | Run without a window, and stop after `n` frames per stream |

## Test

Each example runs headless for a fixed number of frames and exits with status 0 only if every stream passes: all frames received, no gaps in the camera's frame counter, no short frames, nothing dropped by the receiver, and the stream stop acknowledged.

```sh
python3 d555_player.py --stream depth --headless --frame-limit 300 --stats-every 100
python3 d555_player.py --stream rgb --headless --frame-limit 300 --stats-every 100
python3 d555_dual_stream.py --headless --frame-limit 600 --stats-every 300
HSB_ROOT=/path/to/holoscan-sensor-bridge python3 d555_peoplenet.py --headless --frame-limit 300
```

The receivers hold only the newest frame. While the pipeline handles its first frame, the next one can be replaced before it is passed on; a packet capture shows the camera sent it. Gaps within the first 10 frames are therefore counted as `startup_gaps`, and frames the receiver dropped in that time as `startup_dropped`; neither fails a run.

The first `d555_peoplenet.py` run builds a TensorRT engine next to the ONNX model, which takes a few minutes. The camera is already streaming during the build, so that first run reports dropped frames; later runs load the cached engine.

## Expected Output

Each run logs content statistics at `--stats-every` intervals (valid-pixel share and median distance for depth, mean and spread of luma for RGB) and ends with a `RESULT` line per stream. From the tested configuration, with the camera facing an office:

```text
INFO depth: 300 frames, 30.07 fps, valid_depth=64.3% median_depth_mm=2907
INFO RESULT stream=depth frames=300 fps=30.07 seconds=9.9 frame_number_gaps=0 startup_gaps=0 startup_dropped=0 short_frames=0 receiver_dropped=0 max_interval_ms=36.4 status=PASS

INFO rgb: 300 frames, 30.08 fps, mean_luma=117.0 luma_std=51.4
INFO RESULT stream=rgb frames=300 fps=30.08 seconds=9.9 frame_number_gaps=0 startup_gaps=0 startup_dropped=0 short_frames=0 receiver_dropped=0 max_interval_ms=37.1 status=PASS

INFO RESULT stream=rgb frames=600 fps=29.85 seconds=20.1 frame_number_gaps=0 startup_gaps=1 startup_dropped=0 short_frames=0 receiver_dropped=0 max_interval_ms=37.3 status=PASS
INFO RESULT stream=depth frames=600 fps=30.01 seconds=20.0 frame_number_gaps=0 startup_gaps=0 startup_dropped=0 short_frames=0 receiver_dropped=0 max_interval_ms=35.2 status=PASS
```

With a display, `d555_player.py` shows the colorized depth (blue near, red far, black where depth is invalid) or the RGB image, `d555_dual_stream.py` shows RGB on the left and depth on the right, and `d555_peoplenet.py` draws green boxes around people and red boxes around faces.

## Limitations

- **Bandwidth.** The camera sends at most about 500 Mbps of payload. At 30 fps that is depth and RGB together up to 896x504, or one stream at 1280x720. The examples refuse stream sets above the limit unless `--allow-overload` is given.
- **RGB 1280x800** (firmware profiles 4 and 5) is refused by the camera, so the examples do not offer it.
- **Dual stream** runs both streams at the same frame rate: the viewer draws a frame from each together.
- **Stopping.** A stream stop is sent once and never retried, because a repeated stop leaves the camera refusing new profiles until it is power-cycled. If an example reports `stream stop was not acknowledged`, power-cycle the camera before the next run.
- **Frame limit.** `--frame-limit` counts receiver ticks, which also pass once a second while no frame arrives, so a stream that never starts ends after that many seconds and fails.
- **Not covered here:** the camera's IMU stream, depth-to-RGB alignment, and the Camera-over-Ethernet (IEEE 1722) and SIPL capture paths.
- The firmware is an early-access preview, not validated for production.

## License

Apache License 2.0. See [LICENSE](../LICENSE). `d555_peoplenet.py` and `d555_peoplenet.yaml` are derived from Holoscan Sensor Bridge's `linux_tao_peoplenet.py` and `tao_peoplenet.yaml` and keep NVIDIA's copyright notice.

## Support

Maintained by Ashraf Kattoura (RealSense AI). Report issues on the [Holoscan Sensor Bridge Hub issue tracker](https://github.com/nvidia-holoscan/holoscan-sensor-bridge-hub/issues) and mention `realsense/examples/d555`.
