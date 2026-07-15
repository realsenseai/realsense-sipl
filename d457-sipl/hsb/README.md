# `d457-sipl/hsb/` — vendored Holoscan Sensor Bridge (HSB) D457 player sources

The D457 SIPL **Holoscan consumer** (the viewer app, as opposed to the `nvsipl_camera` reference
capture-to-file tool) is a custom operator + example app that live in the **Holoscan Sensor Bridge**
tree, built on the rig. Unlike the driver/query/SerDes patches (which build against the vendored
Camera SIPL SDK and are staged under `../sdk-patches/`), these HSB sources were, until 2026-07-15,
**only present on the rig** (`~/holoscan-sensor-bridge`, itself not a git checkout there) — at risk
of being lost exactly like the multi-link driver work called out in
`.triage/HANDOVER-d457-sipl-multilink-impl.md` §"WHERE THE IMPLEMENTED CODE LIVES". This directory
is the fix: the authoritative, version-controlled copy.

## Layout (mirrors the HSB repo tree under `holoscan-sensor-bridge/`)
- `src/hololink/operators/d457_sipl_capture/` — the C++ capture operator (`D457SIPLCaptureOp`):
  `d457_sipl_capture.{cpp,hpp}`, `d457_sipl_fmt.hpp` (NvSci color-format `fmt` formatter), its
  `CMakeLists.txt`. RAW16-only (no Tegra ISP), N-pipeline (loops `sipl_config_.modules` →
  `sensorConfigs`), one acquire thread per pipeline.
- `python/hololink/operators/d457_sipl_capture/` — the pybind11 binding (`d457_sipl_capture.cpp`)
  + its `CMakeLists.txt`.
- `examples/d457_sipl_player.py` — the Holoscan app: `D457SIPLCaptureOp` → per-stream `CuPy` convert
  (depth Z16→jet, RGB YUYV→RGB, IR left-view) → `HolovizOp`, grid-tiled.
- `wiring/` — **full copies** of the 4 small HSB-tree files this feature touches (they live inside
  the `holoscan-sensor-bridge` git submodule, which tracks upstream `main-2.5.0` — we do not want to
  carry a submodule diff, so `sync_hsb.sh` copies these wholesale onto the rig checkout instead of
  patching in place):
  - `operators_CMakeLists.txt` → `src/hololink/operators/CMakeLists.txt`
    (`add_subdirectory(d457_sipl_capture)`; the stock `sipl_capture`/`d555_sipl_capture` are
    commented out — they target the pre-R39.2 SIPL query API and don't build on this rig).
  - `python_operators_CMakeLists.txt` → `python/hololink/operators/CMakeLists.txt` (same idea).
  - `operators___init__.py` → `python/hololink/operators/__init__.py`
    (`"D457SIPLCaptureOp": "d457_sipl_capture"` module-lazy-load entry).
  - `examples_CMakeLists.txt` → `examples/CMakeLists.txt`
    (adds `d457_sipl_player.py` to the installed example files; C++ SIPL examples gated `if(FALSE ...)`
    since they also need the un-ported stock SIPL ops).

## Sync + rebuild
Use `../tools/sync_hsb.sh` (push sources to the rig + rebuild, or pull rig-side edits back into git):
```bash
./sync_hsb.sh push   # repo -> rig tree, then cmake --build (targets: d457_sipl_capture + its python ext)
./sync_hsb.sh pull    # rig tree -> repo (capture any ad hoc on-rig edits back into git before they're lost)
```
See that script's header for the rig paths + exact cmake targets.

## Why not a submodule branch
`holoscan-sensor-bridge` is a git submodule pinned to upstream `main-2.5.0`
(`git submodule status` shows `+<sha> holoscan-sensor-bridge (heads/main-2.5.0)`). Carrying local
commits directly in the submodule works until the next `git submodule update`, and there is no
current process for maintaining a long-lived fork/branch of it. Vendoring the D457-specific files
here (mirroring the existing `sdk-patches/` pattern for the Camera SIPL SDK) keeps this repo
self-contained and avoids submodule-pointer churn.
