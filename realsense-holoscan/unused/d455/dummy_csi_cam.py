import hololink as hololink_module
import logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

class DummyCSICamera:
    def __init__(self):
        self._width = 640
        self._height = 480
        self._pixel_format = hololink_module.sensors.csi.PixelFormat.RAW_10
        self._mode = None

    def set_mode(self, mode):
        logging.info(f"[DummyCamera] Mode set to: {mode}")
        self._mode = mode

    def configure_converter(self, converter):
        width = 640
        height = 480
        pixel_format = hololink_module.operators.CsiToBayerOp.PixelFormat.RAW_10

        # Dummy CSI values
        frame_start_size = 32
        frame_end_size = 0
        line_start_size = 0
        line_end_size = 0
        margin_top = 8

        # ✅ Use positional args, not keywords
        converter.configure(
            width,
            height,
            pixel_format,
            frame_start_size,
            frame_end_size,
            line_start_size,
            line_end_size,
            0,  # margin_left
            margin_top,
            0,  # margin_right
            0   # margin_bottom
    )

    def pixel_format(self):
        return self._pixel_format

    def bayer_format(self):
        return hololink_module.sensors.csi.BayerFormat.RGGB

    def setup_clock(self):
        logging.info("[DummyCamera] Clock setup simulated")

    def configure(self, mode):
        logging.info(f"[DummyCamera] Configured with mode: {mode}")

    def set_digital_gain_reg(self, val):
        logging.info(f"[DummyCamera] Digital gain set to: {val}")

    def test_pattern(self, pattern):
        logging.info(f"[DummyCamera] Test pattern set: {pattern}")


class DummyCSICameraMode:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"DummyCameraMode({self.value})"
