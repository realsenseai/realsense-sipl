# D457 SIPL debug guide — where the logs are and what to look for

When the D457 SIPL path misbehaves (0 frames, truncated frames, garbage content, stalls), the
evidence is spread across **seven layers**. This guide walks them top-to-bottom, with the exact
commands to pull each log — including the **RTCPU / Camera-RCE firmware trace**, which is where the
NVCSI/VI capture faults actually decode — and a symptom→layer matrix at the end.

Rig: **`fw-advantech-thor-1`** (MIC-742, Jetson Thor, L4T r39.2, kernel 6.8.12-tegra),
`mic-742`/`mic-742`. GMSL chain: D457 (DS5 + OV9282) → MAX9295A → MAX96712 → Thor CSI-A, I2C bus 9.

> ⚠️ The tracefs/debugfs **paths and event names below are the canonical Jetson set and vary by L4T
> version.** Where a path is uncertain it's marked `(verify on rig)`. Use the discovery commands in
> §4d to list what actually exists on this kernel.

---

## A. Build (get the D457 SIPL components compiled)

> **TL;DR — one shot:** `tools/build_deploy.sh` on the rig does steps 1–3 (build) + Deploy below in
> one go: `./build_deploy.sh` (build+install all 3), `-c query` (subset), `-r <repo>` (sync sources +
> apply patches first). The manual steps follow for when you need to do part of it by hand.

The repo `d457-sipl/` is the **source of truth**; the rig has the SIPL SDK build tree at
`$SIPL = /home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl` (CMake, build dir `$SIPL/build`).
Four artifacts make up the D457 path:

| Artifact | Built from | Output |
|----------|-----------|--------|
| Camera-module driver (D457 sensor/module + **MAX9295 serializer HSL**) | `uddf/cdd_d457/` + `uddf/drivers/serializers/MAX9295/` | `libuddf_d457cameramodule_library.so` |
| **MAX96712 deserializer** (HSL pixel-map / PHY) | `uddf/drivers/deserializers/MAX96712,MAX967XX/` | `libnvuddf_max96712_library.so` |
| Query plugin (camera-config JSON) | `query/d457_query.cpp` | `libnvsipl_qry_d457.so` |

(There is no longer a `libnvsipl` binary patch — the old lanes=2 patch was retired with the move to
4 MIPI lanes; the stock system `libnvsipl.so` is used as-is.)

**1. Sync the repo sources into the SDK tree** (the rig builds from `$SIPL`, not the repo):
```bash
SIPL=/home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl
# driver sources + DS5 register tables
cp d457-sipl/uddf_driver/D457Sensor.{cpp,hpp} d457-sipl/uddf_driver/D457Module.{cpp,hpp} \
   d457-sipl/d457_ds5_registers.h  $SIPL/uddf/cdd_d457/
```

**2. Apply the SerDes SDK patches** (they edit the MAX9295/MAX96712 HSL `.py` + `MAX967XX.cpp`
*in the SDK tree*; idempotent — they no-op if already applied, and need a **stock** HSL to match):
```bash
python3 d457-sipl/sdk-patches/patch_max9295_d457.py  $SIPL   # serializer: single-link + pipe-per-VC
python3 d457-sipl/sdk-patches/patch_max96712_d457.py $SIPL   # deser: 2x4/2-lane PHY + pipe-per-VC map
```
> ⚠️ The rig's HSL tree is already patched/hand-edited (pipe-per-VC etc.). Re-running the scripts
> there is a no-op; to reproduce from scratch, extract a **stock** `Jetson_SIPL_API_R39.2.0` first.
> Editing any `*Hsl.py` regenerates its `.hslc`/`.hpp` at build time (the make target cascades).

**3. Compile** (driver + deser libs, then the query plugin):
```bash
cd $SIPL/build
make -j4 uddf_d457cameramodule_library uddf_max96712_library
g++ -shared -fPIC -O2 -o /tmp/libnvsipl_qry_d457.so /home/mic-742/d457_query.cpp
```
A clean build ends with `Built target uddf_d457cameramodule_library` / `… uddf_max96712_library`.
Fresh `.so`s land in the build tree (see Deploy for the exact paths).

---

## B. Deploy (install onto the rig)

**1. Install the driver / deser / query `.so`s** to the SIPL driver dir `/usr/lib/nvsipl_drv/`:
```bash
SIPL=/home/mic-742/sipl_full/usr/src/jetson_sipl_api/sipl
sudo cp $SIPL/build/uddf/cdd_d457/libuddf_d457cameramodule_library.so       /usr/lib/nvsipl_drv/
sudo cp $SIPL/build/uddf/libraries/deserMAX96712/libnvuddf_max96712_library.so /usr/lib/nvsipl_drv/
sudo cp /tmp/libnvsipl_qry_d457.so                                           /usr/lib/nvsipl_drv/
```

**2. No `libnvsipl` patch.** The stock system lib is used. Historically a COPY in `~/sipl_libs/` was
binary-patched to force Tegra lanes=2 and loaded via `LD_LIBRARY_PATH`; that is retired — the deser
now drives 4 lanes @ 1100 Mbps, so `csiPort: "csi-ab"`'s native 4 lanes are correct. If a stale
patched copy is still on the rig, clear it with
`bash d457-sipl/sdk-patches/restore_libnvsipl_stock.sh` and drop `LD_LIBRARY_PATH` from run lines.

**3. Boot the SIPL label.** SIPL uses the non-default `sipl-d457` extlinux label (cdi-mgr overlay →
`/dev/cdi-mgr.9.a`); the DEFAULT `d4xx` label is the V4L2 fallback. Switch by editing `DEFAULT` in
`/boot/extlinux/extlinux.conf` and rebooting. Confirm after boot:
```bash
grep DEFAULT /boot/extlinux/extlinux.conf       # boot target for NEXT reboot
ls /dev/cdi-mgr.9.a                              # present => currently booted into SIPL
```

**4. Run** (power-cycle the DS5 first; full flag reference in the `nvsipl_camera` guide):
```bash
sudo i2cset -y 9 0x28 0x01 0x00; sleep 3; sudo i2cset -y 9 0x28 0x01 0x1f   # DS5 PoC power-cycle
sudo nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -r 15 -s
```
Pick the stream set with `D457_STREAMS` (e.g. `D457_STREAMS=rgb,ir`) + a matching query; the
`d457-sipl/tests/` suite automates build-query → run → assert for each combination.

> **One-line redeploy after a source edit:** sync the changed file into `$SIPL`, `make` the affected
> target, `sudo cp` the `.so` to `/usr/lib/nvsipl_drv/`. No reboot needed unless you change the
> cdi-mgr device-tree overlay.

---

## 0. The logging layers

| # | Layer | Where it logs | How to read it |
|---|-------|---------------|----------------|
| 1 | Consumer (`nvsipl_camera`) | stdout | run it in the foreground; `-v`, `-s` |
| 2 | SIPL library (`libnvsipl`) | stdout / trace | `nvsipl_camera -v 4`; `traceon.cpp` `LD_PRELOAD` shim |
| 3 | Camera HAL (`libnvcamerahal`) + **our UDDF driver** | **syslog** | `journalctl -f \| grep -Ei 'CameraHAL\|D457'` |
| 4 | Kernel **NVCSI / VI** capture drivers | kernel ring | `dmesg -w` |
| 5 | **RTCPU / Camera-RCE firmware** | ftrace + camrtc debugfs + dmesg | §4 — the deep layer |
| 6 | GMSL SerDes (MAX9295 / MAX96712) | I2C registers | `i2cget` (⚠ not while RGB streams) |
| 7 | DS5 ASIC firmware | FW log queue | `ds5-log-collector` skill (`rs-fw-logger`) |

**Rule of thumb:** *userspace says "Start failed / 0 frames"* → go **down** to layers 4–5 to find out
*why no pixels arrived*. The RTCPU trace (layer 5) is almost always the one that names the fault.

---

## 1. Quick triage — three terminals

Open three SSH sessions before you start a capture:

```bash
# Terminal A — HAL + driver (userspace)
journalctl -f | grep -Ei 'camerahal|nvsipl|D457|mipi|csi|vi'

# Terminal B — kernel CSI/VI
dmesg -w

# Terminal C — RTCPU trace (after enabling it, §4c)
cat /sys/kernel/debug/tracing/trace_pipe
```

Then in a fourth terminal: power-cycle the DS5 and run the capture (see the `nvsipl_camera` guide).

---

## 2. Userspace: HAL, SIPL, and the driver (layers 1–3)

- **The Camera HAL logs to syslog, not stdout** — this is the single biggest time-sink. Always:
  ```bash
  journalctl -f | grep -Ei 'CameraHAL'
  ```
- **Our UDDF driver** emits lines prefixed **`D457[<idx>] …`** (`UDDF_LOG_INFO/ERROR`) — they ride the
  same Camera-HAL syslog stream. Grep for `D457[`:
  ```bash
  journalctl -f | grep 'D457\['
  ```
  Expect `ProbeHardware`, `Init`, `StartStreaming stream=… VC… 1280x720@30`. A missing
  `StartStreaming` or a `mode write failed` line points at the I2C/HSL path.
- **SIPL library verbosity:** `nvsipl_camera … -v 4`. For SIPL/query/devblk internal traces use the
  `query/traceon.cpp` `LD_PRELOAD` shim (note `sudo` strips `LD_PRELOAD`, so set it inside the root
  shell):
  ```bash
  sudo bash -c "LD_PRELOAD=\$PWD/libtraceon.so nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -v 4 -r 3"
  ```

---

## 3. Kernel CSI / VI (layer 4) — `dmesg`

```bash
dmesg -w | grep -iE 'vi|nvcsi|csi|capture|host1x|syncpt'
```

Key lines and what they mean:

| dmesg line (substring) | Meaning | Likely cause here |
|------------------------|---------|-------------------|
| `PXL_SOF syncpt timeout` / `frame start syncpt timeout` | VI never saw Start-of-Frame | **No pixels arriving** — DS5 not streaming, link not locked, or deser pipe dropping the DT/VC |
| `FE syncpt timeout` / `frame end` timeout | SOF seen but no Frame-End | Short/truncated frame — resolution or lane-rate mismatch |
| `corr_err: discarding frame` | VI correlation/validation error | DT/VC mismatch or embedded-line mismatch |
| `pixel short line` / `PIX_SHORT` | fewer pixels/line than configured | width/stride mismatch, or RGB ISP stalling mid-stream |
| `nvcsi … err`, `t*-nvcsi … status` | NVCSI D-PHY / lane error | lane count or D-PHY rate mismatch (the `1100000` / lanes=4 set) |

A flood of syncpt timeouts with **nothing** in the RTCPU trace = data isn't even reaching the SoC
(look at SerDes lock, layer 6). Timeouts **with** `CHANSEL_*` events in the trace = data arrives but
is rejected (look at the decode table, §5).

---

## 4. RTCPU / Camera-RCE firmware (layer 5) — the deep trace

The Camera RTCPU (RCE) runs the firmware that programs NVCSI/VI and reports per-frame capture status
as **VI-notify** events. These are exposed through **ftrace** as `tegra_rtcpu_*` trace events, plus a
firmware log whose verbosity is set in **camrtc debugfs**.

### 4a. Boot / crash messages (dmesg)
```bash
dmesg | grep -iE 'rtcpu|camrtc|rce|tegra-camrtc|sce'
```
Look for `rtcpu booted`, firmware version, and — bad — `rtcpu … halted`, `firmware … timeout`, or
watchdog resets. A halted/crashed RTCPU explains a total capture stall that no register tweak fixes.

### 4b. Set the firmware log level (camrtc debugfs)
```bash
ls /sys/kernel/debug/camrtc/ 2>/dev/null            # (verify on rig)
echo 3 | sudo tee /sys/kernel/debug/camrtc/log-level # 0=off … higher=more verbose (verify max)
```
`(verify on rig)` — on some L4T builds this lives under `/sys/kernel/debug/tegra_camrtc*/` instead.

### 4c. Enable the VI-notify event trace (ftrace) — the gold
```bash
cd /sys/kernel/debug/tracing
echo 30720 | sudo tee buffer_size_kb            # grow the ring (KB per CPU)
echo 1     | sudo tee events/tegra_rtcpu/enable # RCE firmware + VI-notify events
echo 1     | sudo tee events/freertos/enable    # RCE RTOS task switches (timing/hangs)
echo 1     | sudo tee tracing_on
echo       | sudo tee trace                     # clear the buffer
# --- now run the capture ---
cat trace                                       # snapshot   ... or:
cat trace_pipe                                  # live stream (Terminal C)
```

You're looking for these event names (substrings) in the trace:

- `tegra_rtcpu_vinotify_event` — normal per-frame events (frame start/end, line counts).
- `tegra_rtcpu_vinotify_error` — **the faults** (decode in §5).
- `tegra_rtcpu_string` — raw RCE firmware log lines (the `log-level` from §4b feeds these).

### 4d. Discovery fallback (when a path/event name is missing)
```bash
ls /sys/kernel/debug/tracing/events/ | grep -iE 'rtcpu|vi|camrtc|csi|capture'
ls /sys/kernel/debug/tracing/events/tegra_rtcpu/
ls -d /sys/kernel/debug/*camrtc* /sys/kernel/debug/*rtcpu* 2>/dev/null
```
Enable whatever camera/VI/CSI event groups exist on this kernel and re-run.

---

## 5. What to look for — VI-notify / CHANSEL fault decode

These tags appear in `tegra_rtcpu_vinotify_error` (and some in `dmesg corr_err`). This is the table
to keep open during bring-up — most D457 SIPL failures land on one of these:

| Fault tag | What VI is saying | Most likely cause on this rig | Where to fix |
|-----------|-------------------|-------------------------------|--------------|
| `CHANSEL_NOMATCH` | incoming `(VC, datatype)` matched no configured pixel parser | **DT/VC mismatch** — DS5 `dtOut`/native DT, deser `0x1E→0x2E` remap, or VC routing wrong | `d457_ds5_registers.h` dtOut (`0x401C/0x409C`), `patch_max96712` pixel-map, query `virtualChannels` |
| `CHANSEL_SHORT_FRAME` / `PIXEL_SHORT_LINE` (PIX_SHORT) | fewer pixels/lines than declared | resolution mismatch, **D-PHY rate too low truncating** (the 4× code-vs-actual mismatch), or RGB ISP stalling after a mux read | width/height regs vs query; DS5 `0x0402` + deser DPLL + query `dphyRate` together; don't poll mux during RGB |
| `CHANSEL_PIXEL_LONG_LINE` | more pixels/line than declared | width too small in config vs what DS5 emits | width regs (`0x4004/0x4024/0x4084`) vs query |
| `CHANSEL_EMBED_*` (`EMBED_SHORT/LONG_LINE`) | embedded-data line mismatch | a non-zero `md_fmt` added an embedded top line | keep metaDataType word md_fmt=0 (`0x4002/0x4022/0x4082`) |
| `CHANSEL_FAULT` / `FAULT_FE` | fault during the frame, forced frame-end | upstream stream dropped mid-frame (link glitch, ISP stall) | SerDes lock (§6); RGB-stall gotcha |
| `CSIMUX_FRAME` error bits | CSI mux frame error (e.g. spurious/missing) | link instability / wrong lane mapping | NVCSI lanes, SerDes |
| `CSIMUX_STREAM` (FIFO overflow / spurious) | CSI stream FIFO overflow | rate too high for the configured lanes | match DS5 rate ↔ deser ↔ query |
| `PHY_INTR` / NVCSI PHY error | D-PHY lane / deskew / CRC error | lane count or D-PHY rate mismatch | the `lanes=4` / `1100000` matched set |
| `ATOMP_*` (packet overflow / FE) | memory-write (output) side error | buffer/stride config | usually downstream of a DT/size mismatch above |

**No `CSIMUX_FRAME` events at all** ⇒ no data reached the CSI mux ⇒ stop looking at VI; the problem
is the DS5 not streaming or the SerDes not forwarding (§6–7).

---

## 6. GMSL SerDes register state (layer 6)

The MAX96712 deserializer RX-lock and the MAX9295/MAX96712 pipe/DT routing tell you whether pixels
even leave the link. **⚠ Do NOT read the DS5 mux (or otherwise poll I2C) while RGB is streaming** —
it stalls the RealTek RGB ISP. SerDes reads on the deser's own address are safer, but prefer to
check these *before* starting RGB.

```bash
# MAX96712 video-pipe RX lock (example offset — confirm the deser I2C addr/offset for this build):
sudo i2cget -y 9 <deser_addr> 0x01DC   # bit0 = RX locked  (FINDINGS: 0 => never locked)
```
Cross-reference the SerDes register intents in `sdk-patches/patch_max9295_d457.py` /
`patch_max96712_d457.py` and the FINDINGS "Hard-won facts" (RCLKOUT for RGB, pipe DT-filters).

---

## 7. DS5 ASIC firmware logs (layer 7)

To see what the DS5 itself thinks (did it accept the stream-config write? did it reach SOF? eyesafety
gating?), pull its FW log queue with the **`ds5-log-collector`** skill (`rs-fw-logger` over a
librealsense checkout). Events of interest: `EVT_I2C_HOST_STREAM_CONFIG` (stream-start dispatch
fired), sensor/projector/MTR(MIPI) start, and any error/eyesafety codes.

⚠ Collecting DS5 FW logs may itself touch the camera over the control channel — **don't run it while
RGB streams**. Use it for depth/IR bring-up or between runs.

---

## 8. Symptom → where to look (decision matrix)

| Symptom | First look | Then | Likely root cause |
|---------|-----------|------|-------------------|
| **0 frames / 0 fps**, dmesg `PXL_SOF timeout` | RTCPU trace: any `CSIMUX_FRAME`? | if none → SerDes lock (§6), DS5 FW (§7) | DS5 not started, link not locked, deser pipe dropping DT/VC |
| 0 frames, RTCPU trace floods `CHANSEL_NOMATCH` | §5 decode | DS5 dtOut + deser remap + query VC | DT/VC mismatch |
| **Frames captured but truncated / `PIX_SHORT`** | RTCPU `CHANSEL_SHORT_FRAME` | rate set (DS5 `0x0402` ↔ deser DPLL ↔ query `dphyRate`) | D-PHY rate too low, or width/height mismatch |
| Frames OK but **content is garbage** | userspace: stream reinterpret | confirm `D457_STREAM` ↔ byte layout | wrong per-stream reinterpretation (Z16/YUYV/Y8I) — not a capture fault |
| **Works ~3–4 frames then stalls** (RGB) | dmesg `corr_err`/`FAULT_FE` | check for an I2C read on the mux during the run | mux polled while RGB streams — remove the read |
| **Wedges between runs** | — | power-cycle DS5 PoC, re-run | known DS5 wedge — `i2cset -y 9 0x28 0x01 0x00 / 0x1f` |
| Total stall, no register tweak helps | dmesg `rtcpu halted` | reboot | RTCPU/RCE firmware crash |

---

## 9. Reset the trace when done

```bash
cd /sys/kernel/debug/tracing
echo 0 | sudo tee tracing_on
echo 0 | sudo tee events/tegra_rtcpu/enable
echo 0 | sudo tee events/freertos/enable
echo   | sudo tee trace            # clear the ring
```

---

## 10. Quick reference

```bash
# Userspace (HAL + driver):
journalctl -f | grep -Ei 'CameraHAL|D457'
# Kernel CSI/VI:
dmesg -w | grep -iE 'vi|nvcsi|syncpt|corr_err'
# RTCPU/RCE trace (enable, then watch live):
cd /sys/kernel/debug/tracing
echo 1 | sudo tee events/tegra_rtcpu/enable; echo 1 | sudo tee tracing_on; echo | sudo tee trace
cat trace_pipe
# RTCPU boot/crash:
dmesg | grep -iE 'rtcpu|camrtc|rce'
```

| Layer | Command |
|-------|---------|
| HAL + driver | `journalctl -f \| grep -Ei 'CameraHAL\|D457'` |
| SIPL lib | `nvsipl_camera … -v 4` / `traceon.cpp` shim |
| CSI/VI | `dmesg -w \| grep -iE 'vi\|nvcsi\|syncpt'` |
| RTCPU events | `/sys/kernel/debug/tracing/events/tegra_rtcpu/` → `trace_pipe` |
| RTCPU fw log | `/sys/kernel/debug/camrtc/log-level` *(verify path)* |
| SerDes | `i2cget -y 9 <deser> 0x01DC` (RX lock; ⚠ not during RGB) |
| DS5 FW | `ds5-log-collector` skill (⚠ not during RGB) |

> The two highest-value, least-known sources for this stack: **`journalctl … CameraHAL`** (because
> the HAL never prints to stdout) and the **`tegra_rtcpu` VI-notify trace** (because it names the
> exact CHANSEL fault when frames are dropped). Reach for those two first.
</content>
