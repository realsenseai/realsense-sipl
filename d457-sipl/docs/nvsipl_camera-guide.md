# Using the stock `nvsipl_camera` application (D457 / SIPL bring-up)

`nvsipl_camera` is NVIDIA's reference SIPL client, shipped with the Jetson Camera SIPL package
(`nvidia-l4t-camera`). It builds a `PlatformCfg`, calls
`INvSIPLCamera::SetPlatformCfg / SetPipelineCfg / Init / Start`, pulls the frame-completion queue,
and optionally dumps frames to disk. For the D457-over-GMSL effort it is **the only consumer we
have today** — there is no RealSense viewer app yet — so it is how we prove the camera streams and
capture frames for verification.

This guide is specific to **`fw-advantech-thor-1`** (Advantech MIC-742, Jetson Thor, L4T r39.2).
It assumes the D457 SIPL driver + query plugin from this repo are installed.

> ⚠️ The `nvsipl_camera` on the rig is the **GMSL** build (has `-H`, `-m`, `-s`). The sample source
> shipped with the SDK (`samples/coe_camera/`) is the **Camera-over-Ethernet** variant and
> lacks those flags — use it only as a reference for the *common* options, not the exact binary.

---

## 1. Prerequisites (before the first run)

The camera must be on the SIPL path, not the V4L2 `d4xx` path, and the patched libraries must be
in place. On a fresh boot, verify:

1. **Booted into the SIPL label.** The non-default `sipl-d457` extlinux label loads the `cdi-mgr`
   device-tree overlay so SIPL can open `/dev/cdi-mgr.9.a`. The DEFAULT label is the `d4xx` V4L2
   fallback. Check the device node exists:
   ```bash
   ls -l /dev/cdi-mgr.9.a        # must exist; if missing you're on the d4xx label — reboot to sipl-d457
   ```
2. **`d4xx` is not bound.** The V4L2 driver contends for the same CSI/I2C — it must be unloaded
   (the `sipl-d457` boot label handles this; confirm `lsmod | grep d4xx` is empty).
3. **Driver + query plugin installed** in `/usr/lib/nvsipl_drv/`:
   - `libuddf_d457cameramodule_library.so` (the UDDF module driver)
   - `libnvsipl_qry_d457.so` (the query plugin — registers the `D457_Camera` config name)
4. **No patched libraries.** The stock system `libnvsipl.so` is used — the old 2-lane binary patch
   in `$HOME/sipl_libs/` (loaded via `LD_LIBRARY_PATH`) is retired since the 4-lane switch.
5. **Power-cycle the DS5 first.** The DS5 ASIC wedges between runs; cycle the PoC before every fresh
   capture:
   ```bash
   sudo i2cset -y 9 0x28 0x01 0x00 && sleep 3 && sudo i2cset -y 9 0x28 0x01 0x1f && sleep 2
   ```

Confirm the config is registered:
```bash
nvsipl_camera -h        # the help text lists supported -c configs; "D457_Camera" should appear
```

---

## 2. Command-line flags (the ones that matter here)

| Flag | Long form | Meaning / our usage |
|------|-----------|---------------------|
| `-c <name>` | `--platform-config` | Named config resolved by the query DB. **Use `-c D457_Camera`** (paired with `-H`). |
| `-t <file>` | `--test-config-file` | Load a platform config JSON directly instead of the query DB. Alternative to `-c`: `-t ../d457_gmsl.json`. |
| `-H` | — | Use the **HAL / UDDF query path** (GMSL build only). Required with `-c D457_Camera`. |
| `-m <mask>` | — | GMSL **link mask** (GMSL build only). `-m 0x0001` = link 0 only (our single D457). |
| `-R` | `--enableRawOutput` | Enable the **raw (ICP) output** path. **Required** — the D457 delivers RAW16; ISP is off. |
| `-0 -1 -2` | `--disableISP{0,1,2}Output` | Disable the three **ISP outputs**. These are *output disables*, **not** pipeline selectors. We disable all three because ISP is unused (raw only). |
| `-f <prefix>` | `--filedump-prefix` | Dump frames to files named `<prefix>_sensor<N>_raw_frame_<i>.raw`. ⚠️ See §5 safety. |
| `-W <n>` | `--writeFrames` | Number of frames to write. Default `0` stops the dump early — pass a **large** value to keep writing. |
| `-r <secs>` | `--runfor` | Run for N seconds then exit. **Always pass it** when backgrounding (see §6). Default is 5 s. |
| `-v <level>` | `--verbosity` | Library verbosity (0–4). `-v 4` for bring-up debugging, `-v 1` normal. |
| `-s` | — | Print FPS / frame statistics (GMSL build). |
| `-N <folder>` | `--nito` | ISP tuning (NITO) folder — **not used** (ISP is off for the D457). |
| `-h` | `--help` | Print usage and the list of registered configs. |

**Stream selection is NOT a flag** — it is the `D457_STREAM` environment variable read by our driver
inside the HAL process: `D457_STREAM=depth|rgb|ir` (default `depth`). One stream at a time on VC0 in
the single-sensor config.

---

## 3. Recipes

### 3a. Smoke test — does it initialize and lock the link?
```bash
sudo i2cset -y 9 0x28 0x01 0x00 && sleep 3 && sudo i2cset -y 9 0x28 0x01 0x1f && sleep 2
sudo nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -v 4 -r 5
```
Expect: `Init()` OK, GMSL link lock, D457 reachable, and a non-zero frame count after Start. The
Camera HAL logs to **syslog**, not stdout — see §7.

### 3b. Capture frames to file (single stream)
Frames **must** land on a size-capped tmpfs (see §5). Manual capture of depth:
```bash
sudo mount -t tmpfs -o size=400M tmpfs /tmp/live && mountpoint -q /tmp/live || exit 1
sudo i2cset -y 9 0x28 0x01 0x00 && sleep 3 && sudo i2cset -y 9 0x28 0x01 0x1f && sleep 2
sudo env D457_STREAM=depth \
    nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera \
    -f /tmp/live/f -r 20 -W 1000000000
```
Swap `D457_STREAM=rgb` or `ir` for the other streams. Each frame file is **1280×720×2 =
1,843,200 bytes** RAW16. Reinterpret per stream (see §4).

### 3c. Live MJPEG viewer (the convenience wrapper)
`tools/d457_live.sh` (deployed to `$HOME/d457_live.sh`) does all of the above — tmpfs mount,
power-cycle, capture, and an MJPEG server with an FPS/resolution overlay:
```bash
sudo sh $HOME/d457_live.sh rgb 8080      # stream = rgb|ir|depth, port = 8080
# from your PC:
ssh -L 8080:localhost:8080 "$RIG_USER@$RIG_HOST"  # then open http://localhost:8080/
# Ctrl-C in the rig shell stops capture + unmounts.
```

### 3d. Run from a config JSON instead of the query DB
```bash
sudo nvsipl_camera -t /path/to/d457_gmsl.json -R -0 -1 -2 -r 10 -v 3
```

---

## 4. Interpreting the output

- **Success line** (stats / `-s`): `Frame captured: <N>, <fps> fps, 0 errors`. ~30 fps and 0 errors
  on a 720p30 config is the good state. The depth bring-up reference was *374 frames, 29.998 fps,
  83.8% non-zero, median 978 mm*.
- **Frame files** are raw **RAW16, 1280×720, little-endian**, 1,843,200 bytes each. The DS5 routes
  every stream as RAW16 (DT 0x2E) on VC0; the bytes are reinterpreted per stream:
  | `D457_STREAM` | On-the-wire | Reinterpret as |
  |---------------|-------------|----------------|
  | `depth` | RAW16 (byte-swapped LE) | Z16 depth in mm |
  | `rgb` | RAW16 | YUYV (YUV422) color |
  | `ir` | RAW16 | interleaved left/right Y8 (Y8I) |
- **Zero frames + huge file growth** ⇒ the stream never started but `nvsipl_camera` spews ~50 MB/s
  of error frames. This is why §5 (size-capped tmpfs) is mandatory.

---

## 5. ⚠️ Safety: never let it write to the root disk

`nvsipl_camera` writes **~50 MB/s** when it captures 0 frames (continuous error spew). Two rules:

1. **Always dump frames to a size-capped tmpfs**, never the root filesystem. Mount it and *verify*
   the mount before starting — abort if the mount failed:
   ```bash
   sudo mount -t tmpfs -o size=400M tmpfs /tmp/live
   mountpoint -q /tmp/live || { echo "no tmpfs — abort"; exit 1; }
   ```
2. **Never redirect its stdout/stderr to a file** (`> log.txt`) — same disk-fill risk. Send it to
   `/dev/null` or watch it live.

---

## 6. Stopping it

```bash
sudo pkill -9 -x nvsipl_camera        # -x = exact name match (NOT -f)
```
- Use **`-x`**, not `-f` — an `-f` match can catch unintended processes.
- It **ignores `timeout`**, so rely on `-r <secs>` for a bounded run, or `pkill` to stop early.
- Always **pass `-r <secs>`** when backgrounding the app. Without it, a backgrounded run gets stdin
  EOF, the interactive runtime menu tears the session down after a few hundred frames, and the
  stream freezes at 0 fps.
- Pass a **large `-W`** (e.g. `1000000000`) to keep writing every frame; the default `-W 0` stops
  the file dump early.

---

## 7. Diagnostics

- **Camera HAL logs go to syslog, not stdout** (this was a major time-sink). Watch them with:
  ```bash
  journalctl -f | grep CameraHAL
  ```
- **Kernel CSI/VI errors** (capture short, PIX_SHORT, lane errors):
  ```bash
  dmesg -w
  ```
- **SIPL library traces** — `-v 4` raises verbosity. For deeper SIPL/query/devblk traces, the
  `LD_PRELOAD` shim `query/traceon.cpp` enables `INvSIPLTrace` levels (note: `sudo` strips
  `LD_PRELOAD`, so set it inside the root shell):
  ```bash
  sudo bash -c "LD_PRELOAD=\$PWD/libtraceon.so nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -v 4 -r 3"
  ```

---

## 8. Gotchas (hard-won)

- **Power-cycle the DS5 before every fresh run** — it wedges between runs (`i2c-9 0x28`, §1).
- **Do not poll the DS5 mux over I2C while RGB streams** — an I2C read on the shared mux stalls the
  RealTek RGB ISP (~3–4 frames then PIX_SHORT, both pipelines torn down). Reads are only safe in the
  depth/IR single-stream case.
- **`-0 -1 -2` are ISP-output disables, not pipeline selectors.** Pipelines come from the query DB
  `sensorInfo[]` (one sensor = one VC = one pipeline). Stream choice is the `D457_STREAM` env var.
- **One stream at a time** in the single-sensor config; simultaneous depth+RGB (multi-VC) is a
  separate, in-progress config.
- The frame dump is the *only* verification surface today — there is no RealSense viewer/consumer
  app yet.

---

## 9. Quick reference

```bash
# Standard single-stream capture (depth) to a safe tmpfs, 20 s:
sudo mount -t tmpfs -o size=400M tmpfs /tmp/live && mountpoint -q /tmp/live || exit 1
sudo i2cset -y 9 0x28 0x01 0x00 && sleep 3 && sudo i2cset -y 9 0x28 0x01 0x1f && sleep 2
sudo env D457_STREAM=depth \
    nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -f /tmp/live/f -r 20 -W 1000000000

# Stop:
sudo pkill -9 -x nvsipl_camera
# Logs:
journalctl -f | grep CameraHAL          # HAL    (syslog)
dmesg -w                                # CSI/VI (kernel)
```

| Item | Value |
|------|-------|
| Rig | `fw-advantech-thor-1` (MIC-742, Thor, L4T r39.2) — tooling default, override with `RIG_HOST`/`RIG_USER`/`RIG_KEY` |
| Config name | `D457_Camera` (with `-H`) |
| Stream select | `D457_STREAM=depth\|rgb\|ir` (env, default depth) |
| Link mask | `-m 0x0001` (link 0) |
| Frame format | RAW16, 1280×720, 1,843,200 B/frame |
| Patched libs | none (stock `libnvsipl.so`; 4 Tegra lanes @ 1100 Mbps) |
| DS5 power-cycle | `i2cset -y 9 0x28 0x01 0x00 / …0x1f` |
| Device node | `/dev/cdi-mgr.9.a` (sipl-d457 boot label) |
| Stop | `pkill -9 -x nvsipl_camera` |
| HAL logs | `journalctl \| grep CameraHAL` |
</content>
</invoke>
