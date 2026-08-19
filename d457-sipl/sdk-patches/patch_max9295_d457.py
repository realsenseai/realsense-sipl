#!/usr/bin/env python3
"""
patch_max9295_d457.py — make the stock SIPL MAX9295 serializer driver support the
RealSense D457 (single GMSL link, MAX9295A) in addition to the HAWK module (MAX9295D, 2 sensors).

WHY
  The public Jetson SIPL "Camera SIPL" MAX9295 driver (uddf/drivers/serializers/MAX9295/)
  hardcodes the HAWK case: MAX9295::SerInit() only configures the video pipes/clock when
  m_config.numSensors == 2 and otherwise returns false with
      "Currently supported MAX9295D in HAWK module for 2 sensors. Add support for numSensors: N"
  The D457 presents a single muxed CSI stream (numSensors == 1), so device-block Init failed
  immediately (NVSIPL_STATUS_ERROR / 10) at serializer init, before any sensor I/O.

WHAT THIS DOES (idempotent)
  1. MAX9295Hsl.py: adds HSL sequence `set_ser_video_phy_clock_max9295a` — the single-link
     pipe-enable + PHY/clock + per-pipe data-type config, ported from the known-good RealSense
     d4xx serializer init (the d4xx Hololink path, MAX9295A writes).
     The GMSL2 stream->pipe routing block (regs 0x006b..0x008b) is intentionally EXCLUDED: that
     routing is owned by the SIPL framework / stock MAX96712 deserializer driver, and writing it
     on the MAX96712 link NACKs (HSL_I2C_ERROR_NACK / EREMOTEIO at 0x008B).
  2. MAX9295.cpp: adds an `else if (m_config.numSensors == 1)` branch to SerInit that submits the
     new sequence (the HAWK numSensors==2 path is left untouched).

RESULT (verified on fw-advantech-thor-1, L4T r39.2)
  CNvMCamera::Init() now SUCCEEDS: deserializer (MAX96712) link-locks, serializer (MAX9295A,
  Device ID 0x91) is detected and configured, the D457 sensor + pipeline are created.
  => the D457 is DISCOVERED + initialized over SIPL. (Streaming/Start is the next phase: the DS5
     mux at 0x1A must be reached over GMSL, which needs correct serializer address translation —
     SerInit still runs the HAWK `set_translator_a` + `set_gpio_pins_max9295d`.)

USAGE
  python3 patch_max9295_d457.py [SIPL_ROOT]
  SIPL_ROOT defaults to ~/sipl_full/usr/src/jetson_sipl_api/sipl
"""
import io, os, shutil, sys



def _write_atomic(path, text):
    """Write `text` to `path` without ever leaving it truncated.

    These files live in the NVIDIA SDK tree. Writing in place means a partial match or an interrupt
    leaves the SDK unrecoverable without a reinstall, and build_deploy.sh -p runs these
    automatically. Keep a one-time .orig alongside, then swap the new content in atomically.
    """
    orig = path + ".orig"
    if not os.path.exists(orig):
        shutil.copy2(path, orig)
    tmp = path + ".tmp"
    with io.open(tmp, "w", encoding="utf-8") as f:
        f.write(text)
    shutil.copystat(path, tmp)
    os.replace(tmp, path)




# Two-phase apply. Every patch function VALIDATES and computes its new text, staging it here; nothing
# reaches the SDK tree until all of them have succeeded. Writing as we went meant an early file was
# already modified when a later anchor failed to match, and build_deploy.sh runs these automatically,
# so the next build consumed a half-patched tree. Reads go through _read() so a second patch to the
# same file sees the staged text rather than the pristine one on disk.
_PENDING = {}


def _read(path):
    if path in _PENDING:
        return _PENDING[path]
    with io.open(path, encoding="utf-8") as f:
        return f.read()


def _stage(path, text):
    _PENDING[path] = text


def _flush():
    for path, text in _PENDING.items():
        _write_atomic(path, text)
    return len(_PENDING)
SIPL_ROOT = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.expanduser("~/sipl_full/usr/src/jetson_sipl_api/sipl")
BASE = os.path.join(SIPL_ROOT, "uddf/drivers/serializers/MAX9295")
PY  = os.path.join(BASE, "MAX9295Hsl.py")
CPP = os.path.join(BASE, "MAX9295.cpp")

NEW_SEQ = '''    # MAX9295A single-link serializer setup for the RealSense D457 — based on the d4xx kernel
    # driver's max9295_init_settings() (global init + per-pipe DT/VC/BPP), plus the I2C
    # address-translation slot the SIPL framework needs to reach the DS5 mux.
    #
    # TWO D457-over-SIPL additions beyond bare max9295_init_settings (both VERIFIED on the rig):
    #  (1) RCLKOUT 0x03F1 + GPIO 0x02BE/0x02BF — forward a reference clock to the camera module.
    #      The depth/IR OV9282 stereo imagers are self-clocked (work without it), but the RGB path's
    #      RealTek RTS5845 ISP needs this serializer-provided clock to OUTPUT pixels: without it the
    #      ISP ACKs I2C "preview on" but never produces a frame, the DS5 MTR lanes stay LP-STOP, and
    #      RGB captures 0 frames. (The d4xx *patch* doesn't write these; the stock NVIDIA max9295
    #      base driver d4xx builds on does. See FINDINGS RGB/IR section.)
    #  (2) PIPE-PER-VC (d4xx __max9295_set_pipe model): each CSI virtual channel rides its own
    #      serializer pipe. pipe X (VC0) forwards depth as RAW16 0x2E (0x0314=0x6E); pipe Y (VC1)
    #      forwards RGB as native YUV422 0x1E (0x0316=0x5E). The pipe DT-filters by data type and
    #      PRESERVES the source VC — so depth stays VC0/0x2E and RGB stays VC1/0x1E on the link.
    #      The deserializer (patch_max96712_d457.py) then demuxes the two VCs to two pipes and remaps
    #      RGB 0x1E->0x2E (the SIPL ICP drops YUV422, so both must reach Tegra as RAW16 0x2E).
    #
    # SIMULTANEOUS depth(VC0/0x2E) + RGB(VC1/0x1E): PIPE-PER-VC (d4xx __max9295_set_pipe model,
    # ser_pipe_id = vc_id). The d4xx path streams depth+RGB+IR+IMU on VC0..VC3 by giving each VC its
    # OWN serializer pipe (pipe N pulls only VC N via 0x0309+2N = 1<<vc, and DT-filters via
    # 0x0314+2N = 0x40|dt). The serializer PRESERVES the source VC (no remap); the deserializer
    # (patch_max96712_d457.py) demuxes the two VCs into two pipes -> two Tegra pipelines.
    #   - pipe X (N=0) pulls VC0 only (0x0309=0x01) and forwards RAW16 0x2E (0x0314=0x6E) -> depth.
    #   - pipe Y (N=1) pulls VC1 only (0x030b=0x02, seeded below) and forwards native YUV422 0x1E
    #     (0x0316=0x5E) -> RGB (DS5 ignores RGB dtOut, so RGB rides the link as 0x1E; the deser
    #     remaps 0x1E->0x2E on its own pipe-1 map block).
    # EARLIER (single-pipe) attempt funneled BOTH VCs through pipe 0 (0x0309=0x03 + pipe-0 dt2=0x1E):
    # two VCs sharing one pipe's framing faulted at the Tegra VI after ~8 frames (ChanselFault). d4xx
    # avoids this with one pipe per VC — replicated here.
    def seq_set_ser_video_phy_clock_max9295a(self):
        with Sequence('set_ser_video_phy_clock_max9295a'):
            annotate('d4xx-based MAX9295A video pipe init for D457 (single link) + RGB-ISP clock + RAW16')
            with self.max9295:
                # I2C address translation slot 1: virtual 0x1A (host) -> physical 0x10 (DS5 mux)
                write(0x0044, 0x34)   # ARRAY1 source      = 0x1A << 1
                write(0x0045, 0x20)   # ARRAY1 destination = 0x10 << 1
                # Reference-clock + GPIO forwarding to the camera module (REQUIRED for RGB/RealTek ISP)
                write(0x02be, 0x90)   # GPIO_A config
                write(0x02bf, 0x60)   # GPIO_B config
                write(0x03f1, 0x89)   # RCLKOUT enable (reference clock out to camera module)
                # --- d4xx max9295_init_settings(): global init (map_pipe_opt) ---
                write(0x0002, 0xf3)   # PIPE_EN: enable all pipes
                write(0x0331, 0x11)   # MIPI_RX1: 2 data lanes (0x33 = 4 lanes)
                write(0x0308, 0x6f)   # CSI_PORT_SEL: all pipes clock from port B
                write(0x0311, 0xf0)   # START_PIPE: all pipes data from port B
                write(0x0312, 0x0f)
                write(0x0314, 0x6e)   # pipe X dt1 = en|RAW16 (0x2E)
                write(0x0315, 0x52)
                # pipe-X (N=0) VC-select bitmask (reg 0x0309+0x02*N = 1<<vc): VC0 only -> depth.
                # RGB is pulled by pipe Y (0x030b=0x02, seeded below), NOT funneled through pipe X.
                write(0x0309, 0x01)   # pipe X pulls VC0 (depth) only
                write(0x030a, 0x00)
                write(0x031c, 0x30)
                write(0x0102, 0x0e)
                write(0x0315, 0xd2)
                write(0x0312, 0x0f)
                write(0x0316, 0x5e)   # pipe Y dt1 = en|YUV422 (0x1E) -> forwards RGB native DT
                write(0x0317, 0x52)
                write(0x030b, 0x02)   # pipe Y pulls VC1 (RGB) only
                write(0x030c, 0x00)
                write(0x031d, 0x30)
                write(0x010a, 0x0e)
                write(0x0315, 0xd2)
                write(0x0312, 0x0f)
                write(0x0318, 0x6e)
                write(0x0319, 0x52)
                write(0x030d, 0x04)   # pipe Z -> VC2 (d4xx: vc_id = pipe_id)
                write(0x030e, 0x00)
                write(0x031e, 0x30)
                write(0x0112, 0x0e)
                write(0x0315, 0xd2)
                write(0x0312, 0x0f)
                write(0x031a, 0x6e)
                write(0x031b, 0x52)
                write(0x030f, 0x08)
                write(0x0310, 0x00)
                write(0x031f, 0x30)
                write(0x011a, 0x0e)
                write(0x0315, 0xd2)
                # PIPE-PER-VC: pipe X (0x0314=0x6E) forwards depth RAW16 0x2E + embedded (dt2 stays
                # 0xD2 = en|EMBED). The earlier funnel hack (pipe-0 dt2=0xDE = forward RGB's 0x1E on
                # pipe 0) is REMOVED — RGB now rides its own pipe Y (0x0316=0x5E, VC1) instead.

'''

OLD_ELSE = '''    else {
        UDDF_LOG_ERROR(*context.driverServices,
            "Currently supported MAX9295D in HAWK module for 2 sensors. Add support for numSensors: %zu",
            m_config.numSensors);
        return false;
    }'''

NEW_ELSE = '''    else if (m_config.numSensors == 1) {
        // RealSense D457: single GMSL link, MAX9295A. Apply the known-good single-link
        // video pipe + PHY/clock configuration (ported from the d4xx serializer init).
        context.hwAccess->SubmitSequence(hsl::set_ser_video_phy_clock_max9295a);
        UDDF_LOG_INFO(*context.driverServices,
            "Video pipe + PHY/clock configured for MAX9295A (D457, single link)");
    }
    else {
        UDDF_LOG_ERROR(*context.driverServices,
            "Unsupported numSensors: %zu (supported: 1 for D457/MAX9295A, 2 for HAWK/MAX9295D)",
            m_config.numSensors);
        return false;
    }'''


def patch_py():
    py = _read(PY)
    if "seq_set_ser_video_phy_clock_max9295a" in py:
        return "PY: already patched"
    marker = "    def compile(self):"
    # Returned, not asserted: `python -O` strips assert, and a stripped check here silently wrote an
    # UNPATCHED file back while still reporting "patched". main() treats anything that is not
    # patched/already patched/not applicable as a failure.
    if marker not in py:
        return "PY: compile() NOT FOUND in MAX9295Hsl.py"
    py = py.replace(marker, NEW_SEQ + marker, 1)
    call = "        self.seq_set_external_fsync_max9295a()\n"
    if call not in py:
        return "PY: external_fsync_max9295a call NOT FOUND in compile()"
    py = py.replace(call, call + "        self.seq_set_ser_video_phy_clock_max9295a()\n", 1)
    _stage(PY, py)
    return "PY: patched"


def patch_cpp():
    cpp = _read(CPP)
    if "set_ser_video_phy_clock_max9295a" in cpp:
        return "CPP: already patched"
    if OLD_ELSE not in cpp:
        return "CPP: SerInit else-block NOT FOUND verbatim in MAX9295.cpp"
    cpp = cpp.replace(OLD_ELSE, NEW_ELSE, 1)
    _stage(CPP, cpp)
    return "CPP: patched"


if __name__ == "__main__":
    # Every patch validates and stages first; nothing is written unless all of them succeeded. A
    # non-applying patch must fail the build (not print and exit 0) AND leave the SDK untouched.
    results = [patch_py(), patch_cpp()]
    ok = (": patched", ": already patched", ": not applicable")
    failed = [r for r in results if not any(m in r for m in ok)]
    for r in results:
        print(r)
    if failed:
        print("FAILED: " + "; ".join(failed) + " -- nothing was written", file=sys.stderr)
        sys.exit(1)
    _flush()
    print("DONE")
