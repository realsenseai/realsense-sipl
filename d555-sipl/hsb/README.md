# `d555-sipl/hsb/` — the D555 project's Holoscan Sensor Bridge code

The D555 Holoscan consumers — the `D555SIPLCaptureOp` operator, the image decoder, the CoE players
and the D555 sensor model — are owned by *this* project rather than by the vendored
`../../holoscan-sensor-bridge/`. See `../../d457-sipl/hsb/README.md` for why the two projects are
kept apart; in short, D555 targets **L4T R38** and D457 targets **R39.2**, whose SIPL config APIs
are mutually exclusive.

D555 is the project that sits naturally on upstream HSB 2.5.0: upstream's own `sipl_capture` is
written against the same R38 API, so this graft *adds* to upstream rather than disabling parts of it.

## Layout

| Path | Contents |
| --- | --- |
| `src/hololink/operators/d555_sipl_capture/` | The C++ SIPL capture operator for the D555 |
| `src/hololink/operators/image_decoder/` + `python/.../image_decoder/` | The image decoder used by every D555 player |
| `python/hololink/sensors/d555/` | The D555 sensor model and modes |
| `examples/` | `d555_sipl_player.cpp`, the `linux_coe_*` / `fusa_coe_*` / `linux_d555_*` players, the PeopleNet demos, and `sipl_config/d555_single.json` |
| `wiring/` | Full replacements for the six HSB files that register the above (operator and sensor registries, CMake, packaging) |

## Use

```sh
tools/graft_hsb.sh d555 [<hsb-tree>]     # from the repository root
cmake -S holoscan-sensor-bridge -B build/hsb -DHOLOLINK_BUILD_SIPL=1
cmake --build build/hsb -j"$(nproc)"
```

Verified on `rs-hsb-thor` (L4T R38.4): `sipl_capture`, `d555_sipl_capture`, `image_decoder`,
`sipl_capture_python`, `image_decoder_python`, `sipl_player` and `d555_sipl_player` all build, with
no D457 target pulled in.
