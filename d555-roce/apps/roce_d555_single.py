# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""One D555e stream (depth or RGB) over RoCEv2 into Holoscan Sensor Bridge's RoceReceiverOp.

The camera firmware writes each frame with UC RDMA WRITEs and ends it with a WRITE-with-immediate
carrying the HSB frame metadata. This app starts one stream, checks every frame's metadata and
logs content statistics. Runs inside the HSB 2.7 demo container.

    python3 roce_d555_single.py --stream depth --ibv-name roceP4p3s0f1 --frame-limit 300
    python3 roce_d555_single.py --stream rgb --profile 6 --ibv-name roceP4p3s0f1
    python3 roce_d555_single.py --stream depth --ibv-name roceP4p3s0f1 --gpu GPU-<dGPU uuid>
"""

import argparse
import logging
import os
import sys
import time

import cuda.bindings.driver as cuda
import cupy as cp
import holoscan

import hololink as hololink_module
from d555_profiles import BYTES_PER_PIXEL, DEFAULT_PROFILE, STREAMS

# Firmware stream command register: data[31:24] stream id, [23:16] command, [15:0] profile.
STREAM_COMMAND_REGISTER = 0x03000210
CMD_START = 1
CMD_STOP = 2
CMD_SET_PROFILE = 3
# A STOP is sent once, unretried: HSB's own write retry resends with a new sequence number, the
# camera doesn't dedupe, and a second STOP after a lost reply underflows its start count.
STOP_TIMEOUT_S = 2.0

# Above this total payload the camera's RoCE send path (~540 Mbps) falls behind; in dual mode
# that has starved its control plane so the stop never landed and it needed a power cycle.
MAX_PAYLOAD_MBPS = 500


def payload_mbps(width, height, fps):
    return width * height * BYTES_PER_PIXEL * 8 * fps / 1e6


class D555Stream:
    """Just enough of an HSB camera device for the receiver: start/stop one stream."""

    def __init__(self, hololink_channel, stream_id, profile):
        self._hololink = hololink_channel.hololink()
        self._stream_id = stream_id
        self._profile = profile
        self.stop_failed = False

    def _command(self, command, profile=0, **write_args):
        value = (self._stream_id << 24) | (command << 16) | (profile & 0xFFFF)
        logging.info(f"D555 stream command 0x{value:08x}")
        return self._hololink.write_uint32(STREAM_COMMAND_REGISTER, value, **write_args)

    def start(self):
        self._command(CMD_SET_PROFILE, self._profile)
        self._command(CMD_START)

    def stop(self):
        # One unretried write (see STOP_TIMEOUT_S). Without an ack the stop may or may not have
        # landed, so record the failure (the run can't pass) rather than risk a second STOP; the
        # receiver's stop() would otherwise swallow it.
        try:
            if self._command(CMD_STOP, timeout=hololink_module.Timeout(STOP_TIMEOUT_S),
                             retry=False) is not False:
                return
            logging.error("stream stop: no ack")
        except Exception as e:
            logging.error(f"stream stop: {e}")
        self.stop_failed = True
        logging.error("stream stop failed: the camera may still be streaming or be wedged. Use "
                      "--stop-only only if it still sends packets (rx_vport_rdma_unicast_packets "
                      "rising): the stop may have landed")


class FrameCheckOp(holoscan.core.Operator):
    """Counts frames, checks metadata continuity, optionally logs per-frame times."""

    TIMING_FIELDS = ("frame_number", "timestamp_s", "timestamp_ns", "metadata_s", "metadata_ns",
                     "received_s", "received_ns")

    def __init__(self, *args, stream, width, height, stats_every=150, timing_csv=None, **kwargs):
        self._stream = stream
        self._frame_size = width * height * BYTES_PER_PIXEL
        self._stats_every = stats_every
        # timestamp_*/metadata_* are camera clock (ISYS, TX start); received_*/app_ns host clock.
        self._timing = open(timing_csv, "w") if timing_csv else None
        if self._timing:
            self._timing.write(",".join(self.TIMING_FIELDS + ("app_ns",)) + "\n")
        self._frames = 0
        self._first_time = None
        self._last_time = None
        self._max_interval = 0.0
        self._last_frame_number = None
        self._frame_number_gaps = 0
        self._short_frames = 0
        self._receiver_dropped = 0
        super().__init__(*args, **kwargs)

    def setup(self, spec):
        spec.input("input")

    def warm_up(self):
        # Compile CuPy's kernels (incl. the median sort) before frames arrive: the receiver keeps
        # only the newest buffer, so a slow first check would drop frames.
        raw = cp.random.randint(1, 256, self._frame_size, dtype=cp.uint8)
        for _ in range(2):
            self._content_of(raw)

    def _content_of(self, raw):
        raw = raw[: self._frame_size]
        if self._stream == "depth":
            depth = raw.view(cp.uint16)
            valid = depth[depth > 0]
            median = int(cp.median(valid)) if valid.size else 0
            return f"valid={valid.size / depth.size:.1%} median_depth={median}"
        luma = raw[0::2].astype(cp.float32)  # YUYV: Y0 U Y1 V
        chroma = raw[1::2].astype(cp.float32)
        return (f"Y_mean={float(luma.mean()):.1f} Y_std={float(luma.std()):.1f} "
                f"UV_mean={float(chroma.mean()):.1f}")

    def compute(self, op_input, op_output, context):
        message = op_input.receive("input")
        app_ns = time.time_ns()
        now = time.monotonic()
        if self._first_time is None:
            self._first_time = now
        else:
            self._max_interval = max(self._max_interval, now - self._last_time)
        self._last_time = now
        self._frames += 1

        metadata = self.metadata
        if self._frames == 1:
            logging.info(f"first frame metadata: {dict(metadata.items())}")
        if self._timing:
            self._timing.write(",".join(str(metadata.get(k, "")) for k in self.TIMING_FIELDS)
                               + f",{app_ns}\n")
        frame_number = metadata.get("frame_number")
        if frame_number is not None:
            # HSB carries the low 16 bits of the firmware's frame counter
            if self._last_frame_number is not None \
                    and frame_number != (self._last_frame_number + 1) & 0xFFFF:
                self._frame_number_gaps += 1
                logging.info(f"frame_number gap: {self._last_frame_number} -> {frame_number}")
            self._last_frame_number = frame_number
        bytes_written = metadata.get("bytes_written")
        if bytes_written is not None and bytes_written < self._frame_size:
            self._short_frames += 1
        # HSB's running count of frames it discarded because this operator was too slow
        self._receiver_dropped = metadata.get("dropped", self._receiver_dropped)

        if self._frames == 1 or (self._stats_every and self._frames % self._stats_every == 0):
            tensor = next(iter(message.values())) if hasattr(message, "values") else message
            content = self._content_of(cp.asarray(tensor).ravel().view(cp.uint8))
            stats_ms = (time.monotonic() - now) * 1000.0
            elapsed = now - self._first_time
            fps = (self._frames - 1) / elapsed if elapsed > 0 else 0.0
            logging.info(
                f"frame {self._frames} fn={frame_number} bytes={bytes_written} "
                f"{content} stats_ms={stats_ms:.1f} fps={fps:.2f} "
                f"fn_gaps={self._frame_number_gaps} short={self._short_frames} "
                f"dropped={self._receiver_dropped}")

    def stop(self):
        if self._timing:
            self._timing.close()
            self._timing = None

    def result(self, frame_limit, stop_failed):
        """(RESULT line, passed). frame_limit counts receiver ticks, which also advance (1 s
        each) while no frames arrive, so fewer frames than the limit is a failure."""
        elapsed = (self._last_time - self._first_time) if self._first_time else 0.0
        fps = (self._frames - 1) / elapsed if elapsed > 0 else 0.0
        failures = [name for name, bad in (
            (f"frames<{frame_limit}", self._frames < frame_limit),
            ("gaps", self._frame_number_gaps), ("short", self._short_frames),
            ("dropped", self._receiver_dropped), ("stop", stop_failed)) if bad]
        status = "FAIL(" + ",".join(failures) + ")" if failures else "PASS"
        return (f"RESULT stream={self._stream} frames={self._frames} fps={fps:.2f} "
                f"seconds={elapsed:.1f} frame_number_gaps={self._frame_number_gaps} "
                f"short_frames={self._short_frames} receiver_dropped={self._receiver_dropped} "
                f"max_interval_ms={self._max_interval * 1000:.1f} "
                f"frame_bytes={self._frame_size} status={status}"), not failures


def add_stream(app, name, channel, camera, profile, ibv_name, ibv_port, frame_limit, **check_args):
    """Add receiver -> checker for one stream to `app`; returns the checker."""
    width, height, _ = STREAMS[name].profiles[profile]
    receiver = hololink_module.operators.RoceReceiverOp(
        app,
        holoscan.conditions.CountCondition(app, name=f"count_{name}", count=frame_limit),
        name=f"receiver_{name}",
        frame_size=width * height * BYTES_PER_PIXEL,
        frame_context=app.cuda_context,
        ibv_name=ibv_name,
        ibv_port=ibv_port,
        hololink_channel=channel,
        device=camera,
    )
    checker = FrameCheckOp(app, name=f"frame_check_{name}", stream=name, width=width,
                           height=height, **check_args)
    checker.warm_up()
    app.add_flow(receiver, checker, {("output", "input")})
    app.streams.append((checker, camera, frame_limit))
    return checker


def check_bandwidth(parser, profiles, allow):
    """Refuse stream sets whose payload the camera can't send (see MAX_PAYLOAD_MBPS)."""
    total = sum(payload_mbps(*STREAMS[name].profiles[p]) for name, p in profiles.items())
    if total > MAX_PAYLOAD_MBPS:
        text = (f"{total:.0f} Mbps payload is over the {MAX_PAYLOAD_MBPS} Mbps limit (the camera "
                f"sends ~540 Mbps over RoCE): expect drops, and dual runs have wedged the camera")
        if not allow:
            parser.error(text + " (--allow-overload to run anyway)")
        logging.warning(text)


def init_cuda(gpu):
    """Primary context on `gpu` (a CUDA_VISIBLE_DEVICES value; None = default device).

    IGX-SW 2.0 exposes one GPU per process, the Thor iGPU by default (HSB then uses pinned host
    memory); a dGPU picked by UUID gets GPUDirect RDMA into VRAM. Returns (device, context).
    """
    if gpu is not None:
        os.environ["CUDA_VISIBLE_DEVICES"] = gpu  # before anything initializes CUDA

    def check(result, *values):
        assert result == cuda.CUresult.CUDA_SUCCESS, result
        return values[0] if len(values) == 1 else values

    check(*cuda.cuInit(0))
    device = check(*cuda.cuDeviceGet(0))
    logging.info(f"CUDA device: {check(*cuda.cuDeviceGetName(64, device)).rstrip(bytes(1)).decode()}")
    return device, check(*cuda.cuDevicePrimaryCtxRetain(device))


def find_d555(camera_ip):
    """Enumeration metadata for the camera, adjusted for what the D555e implements."""
    metadata = hololink_module.Enumerator.find_channel(
        channel_ip=camera_ip, timeout=hololink_module.Timeout(10))
    # WR_DWORD / RD_DWORD only: no block transfers, no vsync
    metadata.update(hololink_module.Metadata({"block_enable": 0, "vsync_enable": 0}))
    return metadata


def open_stream(enumeration, name, profile):
    """DataChannel + D555Stream for one stream, on its own copy of the enumeration metadata."""
    metadata = hololink_module.Metadata(enumeration)
    hololink_module.DataChannel.use_sensor(metadata, STREAMS[name].sensor)
    width, height, fps = STREAMS[name].profiles[profile]
    logging.info(f"{name} {width}x{height}@{fps}: vp_address=0x{metadata['vp_address']:x} "
                 f"vp_mask=0x{metadata['vp_mask']:x}")
    channel = hololink_module.DataChannel(metadata)
    return channel, D555Stream(channel, STREAMS[name].stream_id, profile)


def run(app, hololink, device):
    """Run the graph, log each stream's RESULT; returns the process exit code."""
    app.enable_metadata(True)
    hololink.start()
    try:
        app.run()
    finally:
        hololink.stop()
    passed = True
    for checker, camera, frame_limit in app.streams:
        line, ok = checker.result(frame_limit, camera.stop_failed)
        logging.info(line)
        passed &= ok
    cuda.cuDevicePrimaryCtxRelease(device)
    return 0 if passed else 1


def add_common_args(parser):
    parser.add_argument("--hololink", default="192.168.0.2", help="camera IP")
    parser.add_argument("--ibv-name", default=None, help="IB device (default: first)")
    parser.add_argument("--ibv-port", type=int, default=1)
    parser.add_argument("--gpu", default=None,
                        help="CUDA_VISIBLE_DEVICES value for the frame buffers; a dGPU UUID "
                             "(nvidia-smi -L) = GPUDirect into VRAM. Default: Thor iGPU")
    parser.add_argument("--frame-limit", type=int, default=300,
                        help="frames per stream; counts receiver ticks, so a stream that sends "
                             "nothing ends after this many seconds (and fails)")
    parser.add_argument("--allow-overload", action="store_true",
                        help=f"run stream sets above {MAX_PAYLOAD_MBPS} Mbps payload anyway")
    parser.add_argument("--stats-every", type=int, default=150,
                        help="log content stats every N frames (0 = first frame only)")
    parser.add_argument("--log-level", type=int, default=20)


class RoceD555App(holoscan.core.Application):
    def __init__(self, cuda_context, compose_fn):
        super().__init__()
        self.cuda_context = cuda_context
        self._compose_fn = compose_fn
        self.streams = []  # (checker, camera, frame_limit), filled by add_stream()

    def compose(self):
        self._compose_fn(self)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stream", default="depth", choices=sorted(STREAMS))
    parser.add_argument("--profile", type=int, default=None,
                        help="firmware profile index (d555_profiles.py; default 640x360@30)")
    parser.add_argument("--timing-csv", default=None,
                        help="per-frame camera/host times, input to roce_latency.py")
    parser.add_argument("--stop-only", action="store_true",
                        help="send one stream stop (unretried) and exit. Use ONLY after a session "
                             "died without stopping, while the camera still sends packets (the "
                             "stream then refuses profile changes): it rebalances the camera's "
                             "start count. A stop with no leaked start can underflow the count "
                             "(seen on FW 7.58) and the camera then refuses profile changes until "
                             "rebooted, so on a failed stop re-check the packet counter rather "
                             "than repeating it")
    add_common_args(parser)
    args = parser.parse_args()
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(message)s")
    hololink_module.logging_level(args.log_level)

    profile = DEFAULT_PROFILE[args.stream] if args.profile is None else args.profile
    if profile not in STREAMS[args.stream].profiles:
        parser.error(f"--profile for {args.stream}: one of {sorted(STREAMS[args.stream].profiles)}")
    channel, camera = open_stream(find_d555(args.hololink), args.stream, profile)
    if args.stop_only:
        channel.hololink().start()
        try:
            camera.stop()
        finally:
            channel.hololink().stop()
        return 1 if camera.stop_failed else 0

    check_bandwidth(parser, {args.stream: profile}, args.allow_overload)
    ibv_name = args.ibv_name or hololink_module.infiniband_devices()[0]
    device, context = init_cuda(args.gpu)

    def compose(app):
        add_stream(app, args.stream, channel, camera, profile, ibv_name, args.ibv_port,
                   args.frame_limit, stats_every=args.stats_every, timing_csv=args.timing_csv)

    return run(RoceD555App(context, compose), channel.hololink(), device)


if __name__ == "__main__":
    sys.exit(main())
