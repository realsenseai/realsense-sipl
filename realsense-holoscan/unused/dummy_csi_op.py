import numpy as np
import holoscan as holo
from holoscan.core import Operator, OperatorSpec, Tensor
import ctypes
import numpy as np

class DummyCSIOperator(Operator):
    def __init__(self, fragment, image_path="./dummy_frame_bayer.raw", width=640, height=480, *args, **kwargs):
        self.image_path = image_path
        self.width = width
        self.height = height
        super().__init__(fragment, *args, **kwargs)

    def setup(self, spec: OperatorSpec):
        spec.output("output")

    def compute(self, op_input, op_output, context):
        import numpy as np

        frame = np.random.randint(0, 1024, (480, 640, 1), dtype=np.uint16)
        tensor = Tensor(data=frame)

        # Emit to downstream operator
        op_output.emit(tensor, "output")
