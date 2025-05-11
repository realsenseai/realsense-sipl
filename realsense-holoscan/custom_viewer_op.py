import cv2
import cupy as cp
import numpy as np
from holoscan.core import Operator

class OpenCVViewer(Operator):
    def setup(self, spec):
        cv2.namedWindow("ss", cv2.WINDOW_NORMAL)
        spec.input("input")

    def compute(self, op_input, op_output, context):
        tensor = op_input.receive("input")

        # Convert tensor to NumPy
        cupy_tensor = tensor.data
        numpy_img = tensor.data  # Now it should be a valid NumPy array
        cv2.imshow("Holoscan RealSense Feed", np.asarray(numpy_img).astype(np.uint8))
        cv2.waitKey(1)