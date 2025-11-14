from holoscan.core import Application, Operator
from holoscan.operators import HolovizOp
from holoscan.resources import UnboundedAllocator
from operators.realsense_op import RealSenseCaptureOp
from custom_viewer_op import OpenCVViewer

class DebugLogger(Operator):
    def setup(self, spec):
        spec.input("input")

    def compute(self, op_input, op_output, context):
        tensor = op_input.receive("input")
        print(f"[DebugLogger] Frame received: shape={tensor.shape}, dtype={tensor.dtype}")

class RealSenseApp(Application):
    def compose(self):
        # allocator = UnboundedAllocator(self, name="pool")

        # camera = RealSenseCaptureOp(self, name="realsense_capture")
        camera = DummyCSIOperator(self, name="dummy_csi")

        # visualizer = HolovizOp(
        #     self,
        #     "holoviz",
        #     tensors=[
        #         dict(name="video", type="color", opacity=1.0, priority=0)
        #     ],
        #     headless=True  # or True
        # )

        # self.add_flow(camera, visualizer, {("output", "receivers")})


        viewer = OpenCVViewer(self, name ="viewer")
        self.add_flow(camera, viewer, port_pairs={("output", "input")})

        # logger = DebugLogger(self, "logger")
        # self.add_flow(camera, logger, port_pairs={("output", "input")})

if __name__ == "__main__":
    app = RealSenseApp()
    app.run()
