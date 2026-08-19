# Plan: `d457_sipl_capture` — a Holoscan SIPL capture operator for the D457 over GMSL

> Revised after two independent reviews. Material changes from v1 are flagged **[R]**.

## Goal
Give the D457-over-GMSL SIPL bring-up a **Holoscan consumer** that uses the *same* NvSIPL path the
reference `nvsipl_camera` client uses — same `D457_Camera` query config, same UDDF driver + query
plugin, same `D457_STREAM`/`D457_STREAMS` env mechanism — but delivers frames into a Holoscan pipeline
(GPU tensors → live visualization) instead of dumping `.raw` files. This is the viewer/consumer app the
effort currently lacks.

Deliverable: a new operator **`d457_sipl_capture`** + an example app **`d457_sipl_player.py`** in the
Holoscan Sensor Bridge (HSB) tree, modeled on **`sipl_capture`** (not `d555_sipl_capture` — see below),
adapted for the D457's **RAW16 (DT 0x2E) passthrough** of depth/RGB/IR.

## Why a new operator, modeled on `sipl_capture`
The stock `sipl_capture` and the verbatim-copy `d555_sipl_capture` are hard-wired for **Bayer
RAW10/RAW12 → demosaic → RGB**:
- `fill_camera_info()` accepts only `RAW10/RAW10TP/RAW12`, else throws `"Unsupported input format"`
  (`sipl_capture.cpp:279-291`). D457 reports `inputFormat:"raw16"` (`d457_query.cpp:94,117,140`).
- `compute()` wraps only NV12 or the four `NvSciColor_X2Rc10..._Bayer10*` (RAW10) buffers, else throws
  `"Buffer has unsupported color format"` (`sipl_capture.cpp:827-829`). The D457 RAW16 buffer arrives as
  `NvSciColor_Bayer16RGGB` (cfa=rggb) — rejected.
- The downstream chain (`PackedFormatConverter → ImageProcessor → BayerDemosaic`) assumes Bayer; the
  D457 RAW16 is Z16 depth / YUYV color / Y8I IR, not Bayer.

**[R] Base on `sipl_capture`, NOT `d555_sipl_capture`.** The d555 op (a) is a Bayer-only verbatim copy
with no RAW16 support, (b) **omits the `acquire_buffer_thread`** SIPL-drop-resilience design that
`sipl_capture` has (`sipl_capture.cpp:625-633,984-1047`), and (c) its **Python binding is orphaned** —
there is no `python/hololink/operators/d555_sipl_capture/` dir and no `add_subdirectory(d555_sipl_capture)`
in the python CMakeLists, so `operators.D555SIPLCaptureOp` would `ImportError`. Do not treat d555 as a
working precedent for any layer.

`PixelFormat::RAW_16 = 3` already exists (`csi_formats.hpp:43-44`, "Useful for Z16-like streams") and
`NVSIPL_CAP_INPUT_FORMAT_TYPE_RAW16` is a real enum (SDK `include/NvSIPLCapStructs.h:107`), so no core
change is needed.

---

## [R] Config story (resolves the #1 risk: 3-VC vs single-stream)
The deployed `D457_Camera` query plugin (`d457_query.cpp:78-148`) declares **three** `sensorInfo`
entries (id0=depth/VC0, id1=rgb/VC1, id2=ir/VC2). With `-c D457_Camera`, SIPL builds **3 pipelines**;
the driver's `BuildStreamList(numSensors>=3)` returns `{depth,rgb,ir}` and **ignores `D457_STREAM`**
(`D457Sensor.cpp:79-81`). Simultaneous 3-VC is the separate, still-unsolved bring-up item.

**Decision:**
- The operator is **N-pipeline** (it loops over `sipl_config_.cameras` like `sipl_capture` already
  does) — it works for 1 or 3 sensors, no link-mask param needed.
- **Primary validation path = single-sensor query.** The test harness `tests/lib/common.sh:68`
  (`gen_query <stream>`) compiles/installs a query plugin with **exactly one** sensor on the stream's
  canonical VC (`embeddedTopLines:0`). `gen_query depth` → 1 pipeline, depth/VC0; driver sees
  `numSensors==1` → honors `D457_STREAM`. This is the proven single-stream path and isolates the new
  operator from the unsolved multi-VC work.
- **Multi-stream (3-sensor stock `D457_Camera`) → phase 2**, gated on the separate simultaneous-VC
  bring-up.
- **[R] Always pass the config explicitly.** `init_nvsipl()` auto-selects a config only when
  `camera_config_==""`, and only a **CoE** camera (`sipl_capture.cpp:143`); `D457_Camera` is `GMSL`
  (`d457_query.cpp:72`), so the empty default never finds it. The example always passes
  `--camera-config D457_Camera`.

---

## Deliverables (files)

### New operator (C++) — `holoscan-sensor-bridge/src/hololink/operators/d457_sipl_capture/`
- `d457_sipl_capture.hpp` — operator class (mirror `sipl_capture.hpp`; drop ISP-stats/NITO members).
- `d457_sipl_capture.cpp` — implementation (changes in §A–§D).
- `d457_sipl_fmt.hpp` — SIPL config + `NvSciBufAttrValColorFmt` `fmt` formatters. **[R]** Copy
  `sipl_fmt.hpp` and add only the missing `NvSciColor_Bayer16RGGB/BGGR/GRBG/GBRG` cases (`Y16` is
  **already present** at `sipl_fmt.hpp:260`). The unknown-format throw is TRACE-gated
  (`sipl_fmt.hpp:273-274`) so this only matters at `-v TRACE`, but add the cases anyway.
- `CMakeLists.txt` — mirror `sipl_capture/CMakeLists.txt` (static lib; link `nvsipl`, `nvsipl_query`,
  `nvscibuf`, `nvscisync`, `nvbufsurface`, `nvfusacapinterface`, `nvrm_surface`; the
  `/usr/src/jetson_sipl_api/sipl/include{,/nvsci,/query/include}` includes; alias
  `hololink::operators::d457_sipl_capture`; install hpp + fmt hpp).

### Python binding — `holoscan-sensor-bridge/python/hololink/operators/d457_sipl_capture/`
- `d457_sipl_capture.cpp` — pybind11 module. **[R] Mirror `python/.../sipl_capture/sipl_capture.cpp`
  exactly** (it is the only fully-wired SIPL binding): expose `CameraInfo` incl. **`bytes_per_line`**
  (`python sipl_capture.cpp:88-95`), bind `list_available_configs` as a **static** method
  (`:115`), and add the new `stream`/`streams` kwarg (§B-4).
- `CMakeLists.txt` — mirror the python `sipl_capture` CMakeLists.

### Example app — `holoscan-sensor-bridge/examples/`
- `d457_sipl_player.py` — `d457_sipl_capture` → per-stream convert op → `HolovizOp`, one Holoviz view
  per `camera_info` entry (same loop shape as `sipl_player.py:69-94`).
- **[R]** The convert op reuses the established CuPy-operator pattern — `ImageShiftAndProcessingOperator`
  (`examples/ecam0m30tof_player.py:103-134`, depth+IR) and `ImageShiftToUint8Operator` — rather than a
  from-scratch op. It receives the named tensor via `cp.from_dlpack(in_message.get(info.output_name))`
  (the capture op keys each tensor by `info.output_name`, `sipl_capture.cpp:781,812`).
- C++ `d457_sipl_player.cpp` is **optional / phase 2** (the SIPL example block links bayer/demosaic which
  the D457 path doesn't use; a C++ player needs its own CUDA convert op).

### Build wiring
- `src/hololink/operators/CMakeLists.txt` — add `add_subdirectory(d457_sipl_capture)` in the
  `if(HOLOLINK_BUILD_SIPL)` block (`:67-70`).
- `python/hololink/operators/CMakeLists.txt` — add `add_subdirectory(d457_sipl_capture)` in its
  `if(HOLOLINK_BUILD_SIPL)` block (`:36-38`).
- `python/hololink/operators/__init__.py` — add `"D457SIPLCaptureOp": "d457_sipl_capture"` to `_MODULES`.
- `examples/CMakeLists.txt` — add `d457_sipl_player.py` to `EXAMPLE_INSTALL_FILES` under
  `HOLOLINK_BUILD_SIPL` (`:273-275`).

No changes to the D457 driver / query plugin are required.

---

## Detailed operator changes (vs `sipl_capture.cpp`)

### A. `init_nvsipl()` — RAW-only pipeline
- Pipeline config = capture (ICP) only: `captureOutputRequested=true, isp0/1/2OutputRequested=false,
  disableSubframe=true`.
- Drop the ISP-stats interface-provider block, `register_autocontrol()`, and NITO loading entirely
  (no Tegra ISP in the D457 path — `camera-controls-design.md:31`). These are all already
  `!raw_output_`-guarded in `sipl_capture.cpp:190-202,530-545`, so removal is clean; Start/Stop work
  without ISP (the existing raw path proves it).
- Keep config selection by `camera_config_` (`D457_Camera`) or `json_config_`.

### B. `fill_camera_info()` — accept RAW16
- Add `case NVSIPL_CAP_INPUT_FORMAT_TYPE_RAW16: info.pixel_format = PixelFormat::RAW_16;` (2 B/px).
- **[R] Embedded line = 0 for the query path** (`d457_query.cpp:92,115,138`) → `offset = 0`,
  height = 720. (The `embeddedTopLines:1` in `d457_gmsl.json:33` applies only to the `--json-config`
  path and is an inconsistency to fix there; the primary path uses the query → 0.)
- **Keep the cfa→bayer_format switch** (load-bearing: rggb→RGGB; dropping it re-introduces the default
  throw). Demosaic isn't used downstream, but the field must be set.
- `info.bytes_per_line = plane_pitch` from the reconciled `NvSciBufImageAttrKey_PlanePitch`
  (`sipl_capture.cpp:276`) — set before the format switch, so RAW16 inherits it. **This pitch is
  computed at init** via the op's own reconcile in `fill_camera_info` (`:240-268`), so it is available
  from `get_camera_info()` before the first frame. May exceed `width*2` (alignment) → it is the row
  stride the example must use.

### C. `compute()` — wrap RAW16 as a flat tensor
- Add `is_raw16 = plane_color_format[0]` ∈ {`NvSciColor_Bayer16RGGB` (expected), `…BGGR/GRBG/GBRG`,
  `NvSciColor_Y16` (defensive)}.
- **[R] Wrap as a flat `uint8` `[size]` GXF Tensor with trivial strides — the proven RAW10 pattern**
  (`sipl_capture.cpp:810-826`) — rather than a typed 2D uint16 tensor with a padded row stride
  (DLPack/`__cuda_array_interface__`/Holoviz consumers can shear on a non-contiguous trailing stride).
  The example reshapes to `[height, bytes_per_line]` and slices to `width*2` per row using the
  `bytes_per_line` from `CameraInfo`.
- Keep the NvSci→CUDA external-memory mapping, EOF-fence wait, and pending-buffer/release-callback
  bookkeeping unchanged. Keep the **`acquire_buffer_thread_func`** design from `sipl_capture`.
- HSB metadata block (`sipl_capture.cpp:832-910`) is gated by `is_metadata_enabled()` (off unless the
  app calls `enable_metadata(True)`). The GMSL path has no HSB device appending a trailer → leave
  metadata disabled in the example; keep the block but it won't run.

### B-4. Stream label / env passthrough
- Add an optional `stream` param (`""`|`depth`|`rgb`|`ir`) and/or `streams` (comma list). When set, the
  op `setenv("D457_STREAM", stream, 1)` (and/or `D457_STREAMS`) **before** `INvSIPLCamera::GetInstance()/
  Init()` — the driver reads them in `D457Sensor::Configure()` during `Init()` (verified
  `D457Sensor.cpp:47,58-81,157`), so the timing is correct.
- **For single-sensor validation the query and the env must agree** (e.g. `gen_query rgb` + `stream=rgb`).
  For multi-sensor, the per-pipeline stream is the deviceIndex convention 0=depth/1=rgb/2=ir.
- The example uses the stream label per pipeline to pick the convert path (single → the `--stream`
  value; multi → index convention).

### Reused unchanged
`init_nvsci()`, ICP `allocate_buffers()` (3-attr ICP path), `list_available_configs()`,
`get_camera_info()`, Start/Stop lifecycle (minus ISP buffers/sync/autocontrol). `allocate_sync` is
ISP-only → dropped.

---

## Example pipeline (`d457_sipl_player.py`)
```
d457_sipl_capture ──► D457StreamConvertOp (CuPy) ──► HolovizOp
   (uint8 [size] tensor,    (RGBA uint8 [H,W,4])         (COLOR)
    per pipeline)
```
`D457StreamConvertOp` reshapes the flat buffer to `[height, bytes_per_line]`, slices `:, :width*2`
(handles pitch padding), then per stream (logic from `d457_mjpeg.py:75-91`, `rgb_convert.py:13-24`):
- **depth**: view bytes as `<u2` Z16 LE (driver already `swap16`s → no extra swap), colorize
  `clip(z/4000*255)` (grayscale or turbo LUT) → RGBA.
- **rgb**: YUYV `[Y0,U,Y1,V]` → YUV→RGB (coeffs 1.402/0.344/0.714/1.772) → RGBA.
- **ir**: interleaved L/R Y8I, take left `[:, 0::2]` → replicate → RGBA.

CLI (mirror `sipl_player.py`): `--camera-config D457_Camera` (primary) **or** `--json-config <file>`;
`--stream depth|rgb|ir`; `--headless`; `--frame-limit N`; `--list-configs`. Always RAW (no toggle).

---

## Build & deploy
HSB builds on the Thor rig (or its container) with `HOLOLINK_BUILD_SIPL=ON`; the op links
`nvsipl`/`nvsipl_query` + `/usr/src/jetson_sipl_api/sipl/...` headers → Thor-only. Refs:
`holoscan-sensor-bridge/docs/user_guide/{build.md,thor-jp7-setup.md}`.
1. Apply the four wiring edits. 2. Build the C++ op + python binding on the rig. 3. Install the example.

---

## Rig validation (`fw-advantech-thor-1`)
Prereqs (`nvsipl_camera-guide.md` §1): `sipl-d457` extlinux label (`/dev/cdi-mgr.9.a`, `d4xx`
unloaded), driver+query in `/usr/lib/nvsipl_drv/`. No `LD_LIBRARY_PATH` override is needed — the
stock `libnvsipl.so` is used since the 4-lane switch. **Power-cycle the DS5 before each run**
(`i2cset -y 9 0x28 0x01 0x00 / …0x1f`). Never redirect output to disk; don't poll DS5 I2C while RGB
streams.

1. **Single-sensor query** — `gen_query depth` (via `tests/lib/common.sh`) so `D457_Camera` has exactly
   one pipeline.
2. **Config visibility** — `d457_sipl_player.py --list-configs` lists `D457_Camera` (query plugin loads
   in the Holoscan process).
3. **Depth smoke (headless, bounded)** — power-cycle, then
   `sudo env D457_STREAM=depth python3 d457_sipl_player.py
   --camera-config D457_Camera --stream depth --headless --frame-limit 100`. Expect: init OK, GMSL
   lock, **exactly one** pipeline up, ~30 fps, no format-rejection throw. Watch
   `journalctl -f | grep CameraHAL` and `dmesg -w`.
4. **Confirm geometry/format** — at TRACE, verify the `compute()` buffer dump: color ==
   `NvSciColor_Bayer16RGGB`, `plane_height==720`, `plane_pitch` (note if >2560), `size`. Also assert
   `get_camera_info()` returns 1 entry with width 1280/height 720/`bytes_per_line==plane_pitch` **before**
   declaring success.
5. **Live depth** — same with display; recognizable depth image (median ~1 m), matching the
   `nvsipl_camera` depth reference (374 frames / 29.998 fps / 83.8% non-zero).
6. **RGB** — `gen_query rgb`, run with `--stream rgb` / `D457_STREAM=rgb`; verify color. No DS5 I2C
   polling / `D457_CTRL_GET` while RGB streams.
7. **IR** — `gen_query ir`, `--stream ir`; verify grayscale left view.
8. **Stability** — longer bounded run per stream; power-cycle each time; note run-to-run issues.

**Acceptance:** each of depth/rgb/ir renders live in Holoscan from a single-sensor `D457_Camera`,
content matching the `nvsipl_camera` baseline, exactly one VC up, correct geometry/stride, no
format-rejection exceptions, stable ~30 fps for a bounded run. (3-sensor simultaneous = phase 2.)

---

## Residual risks / verify-on-rig
1. **Actual NvSci color format** — `NvSciColor_Bayer16RGGB` strongly favored by the rggb config; `Y16`
   possible. `is_raw16` covers both; confirm in step 4.
2. **Row-pitch padding** — `plane_pitch` may exceed `width*2`; the example reshape must use
   `bytes_per_line`. Get it wrong → sheared image.
3. **Depth byte order** — assume driver `swap16` yields correct LE Z16 (MJPEG tool reads `<u2`);
   confirm the rendered depth is sane.
4. **`d457_gmsl.json` embeddedTopLines:1 vs query 0** — inconsistency; fix the JSON to 0 if/when the
   `--json-config` path is used (the primary path uses the query → unaffected).
5. **Single stream / single VC for now** — 3-sensor simultaneous deferred (depends on separate multi-VC
   bring-up).
6. **HSB build/deploy onto the rig** — confirm the in-tree `holoscan-sensor-bridge/` build flow and how
   artifacts land on the rig (separate component from the `d457-sipl` driver).
```
