# D457 SIPL `cdi-mgr` device-tree overlay

Switches the D457 on `fw-advantech-thor-1` (Advantech MIC-742, Thor r39.2) from the V4L2
`d4xx` binding to the SIPL `cdi-mgr` device-block binding, so `nvsipl_camera` can reach the
camera. Required because SIPL opens `/dev/cdi-mgr.<bus>.<port>` (created by `cdi_mgr.ko`,
a `compatible="nvidia,cdi-mgr"` DT platform driver) — see `../../.triage/FINDINGS.md`.

## Source of truth
Derived from the live d4xx camera DT (`dtc -I fs -O dts /proc/device-tree`) under
`i2c@810c6a0000` (= `/dev/i2c-9`), and `struct cdi_mgr_platform_data`
(`/usr/src/nvidia/nvidia-public/include/media/cdi-mgr.h`).

### d4xx DT → cdi-mgr / SIPL-JSON mapping (resolved facts)
| d4xx DT | value | maps to |
|---|---|---|
| `i2c@810c6a0000` | adapter 9 (`/dev/i2c-9`) | overlay `i2c-bus = <9>`; JSON `i2cDevice: 9` |
| `max96712@29` `csi-mode "2x4"` `channel "a"` | deser @0x29, CSI-A | overlay `csi-port=<0>`; JSON `deserInfo {Max96712GmslDeserializer, 0x29}`, `csiPort "csi-ab"` |
| `max9295_a@40` | ser @0x40 | JSON `serInfo {MAX9295, 0x40}` |
| `ds5@1a` "Depth" `def-addr 0x10` | sensor @0x1a | JSON sensor `i2cAddress 0x1a` |
| `ds5@1a/mode0` | 1280x720, `num_lanes 2`, `csi_pixel_bit_depth 16`, `pix_clk 74.25MHz`, `vc-id 0` | JSON 1280x720@30, lanes=2, `inputFormat raw16`, `dphyRate ~594000` |
| `ds5@1b/1c/1d` | RGB@0x1b vc1, IR@0x1c vc2, IMU@0x1d vc3 | future multi-stream (separate VCs) |

## Apply (⚠️ reboot + wedge risk — read first)
This changes the boot device tree; it requires a reboot and **disables the V4L2 d4xx camera**.
On a marginal GMSL link this can hit the RSDSO-21558 I2C wedge (recover via PoC power-cycle).
Reboots on these rigs are slow (3–15 min). Do this only with console/recovery access.

1. Compile: `dtc -@ -I dts -O dtb -o d457-cdi-mgr-overlay.dtbo d457-cdi-mgr-overlay.dts`
   (needs symbols `-@`; the live DT must expose `__symbols__`, else convert `target-path`
   fragments — already used here — which don't need symbols).
2. Apply (Thor/JetPack 7): stage the `.dtbo` and enable it via the platform's overlay
   mechanism — `/boot/extlinux/extlinux.conf` `OVERLAYS=` entry, or `jetson-io`, or the
   bootloader `DTBO` slot. (Exact path is board-BSP specific on the MIC-742.)
3. Reboot. Verify: `ls /dev/cdi-mgr.*` (expect `/dev/cdi-mgr.9.a`), `lsmod | grep cdi_mgr`,
   `find /proc/device-tree -iname '*cdi*'`.
4. If d4xx still grabbed the bus: ensure the `ds5@*`/`max96712`/`max9295` nodes are disabled
   (this overlay does that), or unbind at runtime: `echo <dev> > /sys/bus/i2c/drivers/d4xx/unbind`.

## Validation checklist (the [V#] tags in the .dts)
- **[V1]** Confirm the i2c node parent path (`target-path`). Check: in the live `.dts`, find the
  enclosing node of `i2c@810c6a0000` (it may be `/bus@0/...` or another SoC bus).
- **[V2]** Confirm cdi-mgr placement (under the i2c node vs root with `i2c-bus`).
- **[V3]** Confirm CSI-A → `csi-port` integer and the `/dev/cdi-mgr.N.c` name SIPL expects.
- **[V4]** Power/cam GPIOs — none in the d4xx DT (rail likely always-on). Add if PoC is gated.
- **[V5]** Confirm the official `nvidia,cdi-mgr` DT property spellings against NVIDIA's binding
  (fields are correct per `cdi-mgr.h`; DT property names assumed standard).

A reference copy of the relevant live d4xx DT subtree is in `d4xx-camera.dts.ref`.
