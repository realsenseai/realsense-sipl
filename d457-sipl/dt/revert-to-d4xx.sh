#!/usr/bin/env bash
# revert-to-d4xx.sh — roll the D457 back from the SIPL cdi-mgr path to the known-good
# V4L2 d4xx path on fw-advantech-thor-1. Run on the rig as: sudo bash revert-to-d4xx.sh
# Safe to run repeatedly; each step is guarded. See ROLLBACK.md.
set -u
log() { echo "[revert] $*"; }

# 1) Remove a runtime configfs overlay if present (Option A) — no reboot needed.
OV=/sys/kernel/config/device-tree/overlays/d457
if [ -d "$OV" ]; then log "removing runtime overlay $OV"; rmdir "$OV" 2>/dev/null && log "  removed" || log "  rmdir failed (in use?)"; fi

# 2) Restore the known-good boot config (Option B) if a backup exists.
if [ -f "$HOME/extlinux.conf.kgood" ]; then
  log "restoring /boot/extlinux/extlinux.conf from backup"
  cp "$HOME/extlinux.conf.kgood" /boot/extlinux/extlinux.conf && log "  restored (reboot to take effect)"
else
  log "no extlinux.conf.kgood backup found — if you edited extlinux.conf, restore it manually"
fi

# 3) Try to re-bind the d4xx camera nodes (deser/ser/4x DS5) without waiting for a reboot.
if [ -d /sys/bus/i2c/drivers/d4xx ]; then
  for d in 9-0029 9-0040 9-001a 9-001b 9-001c 9-001d; do
    echo "$d" > /sys/bus/i2c/drivers/d4xx/bind 2>/dev/null && log "bound $d" || true
  done
else
  log "d4xx driver not present in sysfs (module unloaded?) — 'sudo modprobe d4xx' then reboot"
fi

# 4) Report state.
log "video nodes now:"; ls /dev/video-rs-* 2>/dev/null || log "  (none — reboot likely required)"
log "if the camera is wedged (no ACK at 0x10/0x1a): power-cycle the PoC rail (see ROLLBACK.md §3),"
log "  e.g. CAM_VDD_EN sysfs LED toggle, or the rs-fw-ds5 hub-control skill, then reboot."
log "done. If /dev/video-rs-* are absent, run: sudo reboot"
