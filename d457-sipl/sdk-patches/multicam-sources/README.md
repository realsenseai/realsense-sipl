# `multicam-sources/` — the D457 multi-camera SDK changes

The single-camera build is produced by `../../tools/build_deploy.sh`, which syncs
`../../uddf_driver/` into the SIPL SDK tree and applies `../patch_max9295_d457.py` /
`../patch_max96712_d457.py`. The multi-camera (Stage 2 / Stage 3) configuration additionally
changes four SerDes drivers that diverge too far from stock to express as targeted edits.

Those four are **NVIDIA's code** under `LicenseRef-NvidiaProprietary`, so only our diff is kept, in
[`../multicam-patches/`](../multicam-patches/) — never the files themselves. Apply them to the SDK
installed on the Jetson:

```sh
../multicam-patches/apply.sh                 # default /usr/src/jetson_sipl_api/sipl
../multicam-patches/apply.sh /path/to/sipl   # or an explicit tree
../multicam-patches/apply.sh -R              # revert
```

The script is idempotent and refuses to half-apply: an already-patched tree is reported and left
alone, and a tree the patches do not fit is reported as a failure rather than silently skipped.

| Patch | Applies to, under the SDK root |
| --- | --- |
| `MAX9295.cpp.patch`, `MAX9295.hpp.patch` | `uddf/drivers/serializers/MAX9295/` |
| `MAX967XX.cpp.patch`, `MAX967XXHsl.py.patch` | `uddf/drivers/deserializers/MAX967XX/` |

`D457Sensor.cpp` is **not** duplicated here — the multi-camera build uses the same
`../../uddf_driver/D457Sensor.cpp` as the single-camera build, which `build_deploy.sh` already
installs.

`repatch2_nvsipl_main.sh` re-applies the per-link VC/i2c offset to the SDK's
`samples/camera/main.cpp`. It restores from a rig-local backup (`~/multicam_bak/main.cpp`) that is
not tracked here, so take a copy of the stock `main.cpp` before first use.
