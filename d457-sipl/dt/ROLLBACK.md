# Live-testing fallback / rollback plan — D457 cdi-mgr bring-up

Applying the `cdi-mgr` overlay = changing the boot device tree + disabling the working V4L2
`d4xx` camera + a reboot, on a remote rig with the RSDSO-21558 I2C-wedge risk and slow
(3–15 min) reboots. This is the recovery plan so a failed attempt is always revertible.

> ⚠️ The rig went **unreachable mid-session** (DNS NXDOMAIN on `fw-advantech-thor-1`). That is
> precisely the failure mode this plan must survive — and the one piece software cannot fix:
> if the box hangs on boot or never returns on SSH, recovery needs **out-of-band access**
> (serial console / recovery mode / a remotely power-cyclable PDU). **Do live testing only
> when someone can console/power-cycle the rig**, or with that access pre-arranged.

## 0. Pre-flight (RUN ON THE RIG BEFORE ANY CHANGE — back up the known-good state)
```bash
# known-good boot config + active DTB
sudo cp /boot/extlinux/extlinux.conf ~/extlinux.conf.kgood
FDT=$(awk '/^[[:space:]]*FDT/ {print $2; exit}' /boot/extlinux/extlinux.conf); echo "active FDT=$FDT"
[ -n "$FDT" ] && sudo cp "$FDT" "${FDT}.kgood"
# known-good DT + camera baseline (proof of what "working" looks like)
dtc -I fs -O dts /proc/device-tree > ~/dt-kgood.dts 2>/dev/null
ls -l /dev/video-rs-* > ~/baseline.txt; lsmod | grep -E '^d4xx|^max9' >> ~/baseline.txt
# prove the reboot cycle itself works (BEFORE adding the overlay): reboot, confirm SSH returns
sudo reboot   # then re-verify /dev/video-rs-depth-0 streams on the KNOWN-GOOD config first
```
Do not proceed to the overlay until a plain reboot returns the rig and the d4xx camera still works.

## 1. Apply method — CONFIRMED for this board
Runtime **configfs overlays are NOT available** on this rig (`NO_CONFIGFS_OVERLAYS`), so use the
**extlinux `OVERLAYS=` + sibling LABEL** method. The board already uses exactly this pattern:
`DEFAULT d4xx` boots `Image.d4xx` with `OVERLAYS /boot/tegra264-camera-d4xx-overlay-advantech.dtbo`.
Our cdi-mgr overlay is an **alternative** to that camera overlay — selected by its own LABEL — so
the working d4xx camera stays the `DEFAULT` (power-cycle auto-fallback) and our overlay never
touches the d4xx nodes.

```bash
# 1) build + stage the overlay (verified to compile: dtc_exit 0, 585B .dtbo)
dtc -@ -I dts -O dtb -o d457-cdi-mgr-overlay.dtbo d457-cdi-mgr-overlay.dts
echo <pw> | sudo -S cp d457-cdi-mgr-overlay.dtbo /boot/

# 2) add a sibling LABEL (copy the d4xx LABEL verbatim, swap ONLY the OVERLAYS line).
#    Do NOT change `DEFAULT d4xx` yet.
#    /boot/extlinux/extlinux.conf:
# LABEL sipl-d457
#       MENU LABEL SIPL D457 (cdi-mgr)
#       LINUX /boot/Image.d4xx
#       INITRD /boot/initrd.img-6.8.12-tegra
#       APPEND ${cbootargs} root=PARTUUID=7f9c8a7f-... <copy from d4xx label>
#       FDT /boot/dtb/kernel_tegra264-p4071-0000+p3834-0008-nv.dtb
#       OVERLAYS /boot/d457-cdi-mgr-overlay.dtbo
```

### ⚠️ Booting the new label requires changing DEFAULT — this is the console-gated step
extlinux has no boot-attempt auto-revert. To actually boot `sipl-d457` you must set
`DEFAULT sipl-d457` (or pick it at the serial-console menu) and reboot. If that boot hangs or
SSH never returns, **only an out-of-band console/PDU can set `DEFAULT` back to `d4xx`.**
Therefore: **do this step only with serial console or a remote power-cycle available.**
Mitigation if you have console: leave `TIMEOUT 30` and the serial menu so you can manually pick
`d4xx` at the prompt on the recovery boot. **Never overwrite the base DTB or the d4xx label.**

## 2. Revert to the known-good d4xx path
Use the helper: `sudo bash revert-to-d4xx.sh` (next to this file), or manually:
```bash
# remove runtime overlay (Option A)
sudo rmdir /sys/kernel/config/device-tree/overlays/d457 2>/dev/null
# restore boot config (Option B)
sudo cp ~/extlinux.conf.kgood /boot/extlinux/extlinux.conf
sudo reboot
# after boot, if d4xx didn't auto-bind:
for d in 9-001a 9-001b 9-001c 9-001d 9-0029 9-0040; do echo $d | sudo tee /sys/bus/i2c/drivers/d4xx/bind 2>/dev/null; done
ls /dev/video-rs-*   # expect the 7 RealSense nodes back
```

## 3. I2C-wedge recovery (RSDSO-21558)
A wedge takes the camera link down and **a reboot does NOT power-cycle the PoC camera**.
Recover by toggling the camera power rail, then re-bind:
```bash
# CAM_VDD_EN via sysfs LED (per rs-fw-ds5 RSDSO-21558 notes — confirm the led name on this rig)
echo 0 | sudo tee /sys/class/leds/<cam_vdd_en>/brightness; sleep 5
echo 1 | sudo tee /sys/class/leds/<cam_vdd_en>/brightness
# or use the rs-fw-ds5 `hub-control` skill to power-cycle the camera/PoC
```

## 4. Abort criteria
- SSH does not return within ~15 min of a reboot → **stop, escalate to out-of-band** (console).
- Camera not at I2C 0x10/0x1a after recovery → run the wedge recovery (§3), then reboot.
- Two failed overlay boots → revert to known-good (§2) and re-examine the `[V1–V5]` tags.
