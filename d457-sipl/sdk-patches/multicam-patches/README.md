# `multicam-patches/` — the D457 multi-camera SerDes changes

The single-camera build is produced by `../../tools/build_deploy.sh`, which syncs
`../../uddf_driver/` into the SIPL SDK tree and applies `../patch_max9295_d457.py` /
`../patch_max96712_d457.py`. The multi-camera (Stage 2 / Stage 3) configuration additionally
changes four SerDes drivers that diverge too far from stock to express as targeted edits.

Those four are **NVIDIA's code** under `LicenseRef-NvidiaProprietary`, so only our diff is kept here
— never the files themselves. Apply them to the SDK installed on the Jetson:

```sh
./apply.sh                 # default /usr/src/jetson_sipl_api/sipl
./apply.sh /path/to/sipl   # or an explicit tree
./apply.sh -R              # revert
```

The script is idempotent and will not half-apply: every patch is dry-run first, and if any one of
them does not fit the tree, nothing is modified at all. An already-patched tree is reported and left
alone. The first apply keeps a one-time `<file>.orig` next to each patched file, matching what
`../patch_max9295_d457.py` and `../patch_max96712_d457.py` do.

| Patch | Applies to, under the SDK root |
| --- | --- |
| `MAX9295.cpp.patch`, `MAX9295.hpp.patch` | `uddf/drivers/serializers/MAX9295/` |
| `MAX967XX.cpp.patch`, `MAX967XXHsl.py.patch` | `uddf/drivers/deserializers/MAX967XX/` |

`D457Sensor.cpp` is **not** duplicated here — the multi-camera build uses the same
`../../uddf_driver/D457Sensor.cpp` as the single-camera build, which `build_deploy.sh` already
installs.

`repatch2_nvsipl_main.sh` re-applies the per-link VC/i2c offset to the SDK's
`samples/camera/main.cpp`. It restores from a rig-local backup (`~/multicam_bak/main.cpp`) that is
not tracked here, so take a copy of the stock `main.cpp` before first use; the script refuses to run
without it. `SIPL=` and `BAK=` override the two paths.

## Serializer bring-up toggles

`MAX9295.cpp.patch` adds six environment switches that alter serializer setup. They were A/B
switches during bring-up and are **not** part of the validated configuration: the Stage 2 / Stage 3
results were produced with all of them unset. They are kept because the reasoning behind each is
still useful if the link misbehaves on different hardware, but treat them as diagnostics.

| Variable | Effect when set |
| --- | --- |
| `D457_NOISO` | Skip the serializer I2C isolation step |
| `D457_ISO_RESET` | Reset the isolation state before configuring |
| `D457_SER_XLAT` | Apply the serializer address-translation path |
| `D457_SER_ISOL` | Force the isolation path rather than auto-selecting |
| `D457_ALONE` | Configure this serializer as if it were the only one on the bus |
| `D457_FINAL_RESET` | Issue an extra reset after configuration |

If you are reproducing the validated multi-camera configuration, leave all six unset.
