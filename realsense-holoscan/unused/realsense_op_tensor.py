import pyrealsense2 as rs
import numpy as np
import holoscan as holo
import cupy as cp
from holoscan.core import Tensor
import cv2

class RealSenseCaptureOp(holo.core.Operator):
    def setup(self, spec):
        spec.output("out")
        self.pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
        self.pipeline.start(config)

    def compute(self, op_input, op_output, context):
        frame = self.pipeline.wait_for_frames().get_color_frame()
        if not frame:
            return

        image_bgr = np.asanyarray(frame.get_data())
        image_rgba = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGBA)

        # Ensure stream context is bound correctly
        stream = cp.cuda.get_current_stream()  # Uses Holoscan default
        with stream:
            gpu_array = cp.asarray(image_rgba)

        tensor = Tensor.from_dlpack(gpu_array)
        op_output.emit(tensor, "out")
