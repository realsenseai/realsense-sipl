# `multicam-patches/` — the D457 multi-camera SerDes diffs

See [`../multicam-sources/README.md`](../multicam-sources/README.md) for how to apply these.

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
