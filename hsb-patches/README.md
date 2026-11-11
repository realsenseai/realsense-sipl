# `hsb-patches/` — changes this repository needs in Holoscan Sensor Bridge

This repository builds on a tagged release of NVIDIA's
[Holoscan Sensor Bridge](https://github.com/nvidia-holoscan/holoscan-sensor-bridge) (HSB), currently
**2.7.0** (commit `1df133d4c78a6921249be4b147d65554aa8efa68`), instead of carrying a copy of it.
`tools/prepare_hsb.sh` clones that release, applies the patches below, and adds one project's
Holoscan code (`d457-sipl/hsb/` or `d555-sipl/hsb/`, each registered by its own
`hsb-registration.patch`).

| Patch (`2.7.0/`) | What it changes | Why |
| --- | --- | --- |
| `0001-raw16-pixel-format.patch` | `csi::PixelFormat` gains `RAW_16` (plus `YUYV_422_8` and `INVALID`) and its Python binding; `fusa_coe_capture` handles `RAW_16` | The D555 delivers depth (Z16) and RGB (YUYV) as 16-bit containers, which upstream does not support |
| `0002-demo-sipl-driver-mounts.patch` | `docker/demo.sh` mounts `/usr/lib/nvsipl_uddf` and `/usr/lib/nvsipl_drv` read-only when present | SIPL loads the camera drivers from those directories, which the demo container does not have |

Both are generic additions with no camera-specific code. The patches keep HSB's Apache-2.0 license.

## Tested

With D555 firmware 7.59.0.16345 on a Jetson AGX Thor (L4T R38.4), the D555 project built on HSB
2.7.0 with these patches streams over SIPL (depth and RGB), Linux CoE and FuSa CoE.

On a Jetson AGX Thor with L4T R39.2, the D457 project built on HSB 2.7.0 with these patches streams
depth, RGB and IR over SIPL from one D457.

## History

Until this change the repository vendored HSB 2.5.0 with RealSense edits (see git history before
the `hsb-2.7` branch, `holoscan-sensor-bridge/REALSENSE-FORK-NOTICE.md`). Of those edits, the
`RAW_16` support survives as `0001`; the SIPL mounts of `docker/demo.sh` survive as `0002`. The
others were dropped: the `udp_port` receiver option and the downgraded minimum-IP-version check
served D555 firmware older than 7.59.0.16345, and the Dockerfile and `cuda-python` adjustments are
not needed by the 2.7.0 container.

## Moving to a newer HSB release

Clone the new tag, apply each patch with `git apply --3way`, resolve what does not apply, and
save the results under `hsb-patches/<version>/` together with the project registration patches;
then update `HSB_TAG` and `HSB_COMMIT` in `tools/prepare_hsb.sh`.
