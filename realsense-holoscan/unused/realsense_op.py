import pyrealsense2 as rs
import numpy as np
import holoscan as holo
import cupy as cp
from holoscan.core import Tensor
import cv2

class RealSenseCaptureOp(holo.core.Operator):
    def __init__(self, fragment, *args, **kwargs):
        super().__init__(fragment, *args, **kwargs)

    def setup(self, spec):
        spec.output("output")
        self.pipeline = rs.pipeline()
        config = rs.config()
        config.enable_stream(rs.stream.color, 640, 480, rs.format.bgr8, 30)
        self.pipeline.start(config)
        
    def compute(self, op_input, op_output, context):
        import pyrealsense2 as rs
        import numpy as np
        import cv2
        import cupy as cp

        frames = self.pipeline.wait_for_frames()
        color_frame = frames.get_color_frame()
        if not color_frame:
            return

        image_bgr = np.asanyarray(color_frame.get_data())
        image_rgba = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGBA)

        op_output.emit(image_rgba, "output")  # no Tensor!


