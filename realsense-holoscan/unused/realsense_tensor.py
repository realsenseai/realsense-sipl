from holoscan.core import Application, Operator
from holoscan.operators import HolovizOp
from holoscan.resources import UnboundedAllocator
from realsense_op_tensor import RealSenseCaptureOp
from custom_viewer_op import OpenCVViewer

class DebugLogger(Operator):
    def setup(self, spec):
        spec.input("input")

    def compute(self, op_input, op_output, context):
        tensor = op_input.receive("input")
        print(f"[DebugLogger] Frame received: shape={tensor.shape}, dtype={tensor.dtype}")

class RealSenseApp(Application):
    def compose(self):
        camera = RealSenseCaptureOp(self, name="realsense_capture")

        logger = DebugLogger(self, "logger")
        self.add_flow(camera, logger, port_pairs={("out", "input")})
        
        visualizer = HolovizOp(
            self,
            name="holoviz",
            tensors=[{
                "name": "video",
                "type": "color",
                "opacity": 1.0,
                "priority": 0,
            }],
            headless=False
        )
        
        self.add_flow(camera, visualizer, {("out", "receivers")})

if __name__ == "__main__":
    app = RealSenseApp()
    app.run()
