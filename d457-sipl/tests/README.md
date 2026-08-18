# D457 SIPL streaming tests

Hardware integration tests for the D457-over-GMSL/SIPL path on **fw-advantech-thor-1**. They drive the
real stack (`nvsipl_camera` → UDDF driver → MAX9295/MAX96712 → Tegra VI) and assert each stream
captures frames at ~30 fps with **0 drops** and **0 Tegra-VI faults**.

## Tests
| File | What it checks |
|------|----------------|
| `test_01_each_stream_alone.sh` | each stream starts alone — depth (VC0), rgb (VC1), ir (VC2) |
| `test_02_all_three.sh` | depth + rgb + ir simultaneously |
| `test_03_startstop_repeat.sh` | start all 3 / stream 10 s / stop — repeated 10× |
| `test_04_two_stream_permutations.sh` | every 2-stream pair: depth+rgb, depth+ir, rgb+ir |
| `run_all.sh` | runs the whole suite, reports an overall pass/fail |

## How it works
Each stream rides its **canonical CSI virtual channel** (depth→VC0, rgb→VC1, ir→VC2), fixed by the
SerDes pipe DT filters after the pipe-per-VC fix. For a given stream subset a test:
1. generates + compiles + installs a query plugin with one `sensorInfo` per stream at its canonical VC
   (`lib/common.sh` `gen_query`),
2. sets `D457_STREAMS=<comma-list>` so the driver plays exactly those streams,
3. power-cycles the DS5 (it wedges run-to-run),
4. runs `nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -r <secs> -s`,
5. parses the per-sensor `Frame captured / Frame drops` summary and checks `dmesg` for
   `ChanselFault/PIX_SHORT` — pass = every expected sensor ≥ `25*secs` frames, 0 drops, 0 faults.

## Running (on the rig)
Boot the `sipl-d457` extlinux label first (DS5 SIPL drivers in `/usr/lib/nvsipl_drv/`). Then:
```sh
cd ~/d457_tests          # where this folder is deployed on the rig
bash run_all.sh          # full suite
bash test_02_all_three.sh   # a single test
SECS=30 bash test_02_all_three.sh   # override duration
ITERS=3 bash test_03_startstop_repeat.sh
```
The scripts use `sudo` for the DS5 power-cycle, `nvsipl_camera`, and `dmesg`.

> ⚠ These tests power-cycle the camera and run the live pipeline. Don't run them while another
> `nvsipl_camera` is active. Frames/logs go to a size-capped tmpfs at `/tmp/live` (never the real disk).

## Notes
- `gen_query` overwrites `/usr/lib/nvsipl_drv/libnvsipl_qry_d457.so`. After the suite, the installed
  query reflects the LAST test's stream set — re-install the committed 3-sensor query (or just
  re-run `gen_query depth rgb ir`) to restore the default 3-stream config.
- The committed driver reads `D457_STREAMS`; with it unset the legacy behavior applies (3-sensor
  query → depth+rgb+ir, 2-sensor → depth+rgb, 1-sensor → `D457_STREAM`).
