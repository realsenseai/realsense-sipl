# d457_registers.py

from collections import namedtuple
from enum import Enum

import hololink as hololink_module

MAX9295A_I2C_ADDR = 0x40 # serialaizer
MAX9296A_I2C_ADDR = 0x48 # deserializer
MUX_I2C_ADDR = 0x1A

REALSENSE_TABLE_WAIT_MS = 0x0F

# Format: (i2c_address, register_address, value)
MAX9295_MAX9296_REG_INIT_WRITES = [
    (MAX9296A_I2C_ADDR, 0x0010, 0x0001),
    (MAX9296A_I2C_ADDR, 0x0010, 0x0021),
    (MAX9296A_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
    (MAX9295A_I2C_ADDR, 0x0010, 0x0021),
    (MAX9296A_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
    (MAX9295A_I2C_ADDR, 0x006b, 0x0010),
    (MAX9295A_I2C_ADDR, 0x0073, 0x0011),
    (MAX9295A_I2C_ADDR, 0x007b, 0x0031),
    (MAX9295A_I2C_ADDR, 0x0083, 0x0031),
    (MAX9295A_I2C_ADDR, 0x0093, 0x0031),
    (MAX9295A_I2C_ADDR, 0x009b, 0x0031),
    (MAX9295A_I2C_ADDR, 0x00a3, 0x0031),
    (MAX9295A_I2C_ADDR, 0x00ab, 0x0031),
    (MAX9295A_I2C_ADDR, 0x008b, 0x0031),
    (MAX9295A_I2C_ADDR, 0x0044, 0x0034),
    (MAX9295A_I2C_ADDR, 0x0045, 0x0020),
    (MAX9295A_I2C_ADDR, 0x02be, 0x0090),
    (MAX9295A_I2C_ADDR, 0x02bf, 0x0060),
    (MAX9295A_I2C_ADDR, 0x03f1, 0x0089),
    (MAX9296A_I2C_ADDR, 0x0332, 0x00f0),
    (MAX9295A_I2C_ADDR, 0x0002, 0x00f3),
    (MAX9295A_I2C_ADDR, 0x0331, 0x0011),
    (MAX9295A_I2C_ADDR, 0x0308, 0x006f),
    (MAX9295A_I2C_ADDR, 0x0311, 0x00f0),
    (MAX9295A_I2C_ADDR, 0x0312, 0x000f),
    (MAX9295A_I2C_ADDR, 0x0314, 0x005e),
    (MAX9295A_I2C_ADDR, 0x0315, 0x0052),
    (MAX9295A_I2C_ADDR, 0x0309, 0x0001),
    (MAX9295A_I2C_ADDR, 0x030a, 0x0000),
    (MAX9295A_I2C_ADDR, 0x031c, 0x0030),
    (MAX9295A_I2C_ADDR, 0x0102, 0x000e),
    (MAX9295A_I2C_ADDR, 0x0315, 0x00d2),
    (MAX9295A_I2C_ADDR, 0x0312, 0x000f),
    (MAX9295A_I2C_ADDR, 0x0316, 0x005e),
    (MAX9295A_I2C_ADDR, 0x0317, 0x0052),
    (MAX9295A_I2C_ADDR, 0x030b, 0x0002),
    (MAX9295A_I2C_ADDR, 0x030c, 0x0000),
    (MAX9295A_I2C_ADDR, 0x031d, 0x0030),
    (MAX9295A_I2C_ADDR, 0x010a, 0x000e),
    (MAX9295A_I2C_ADDR, 0x0315, 0x00d2),
    (MAX9295A_I2C_ADDR, 0x0312, 0x000f),
    (MAX9295A_I2C_ADDR, 0x0318, 0x005e),
    (MAX9295A_I2C_ADDR, 0x0319, 0x0052),
    (MAX9295A_I2C_ADDR, 0x030d, 0x0004),
    (MAX9295A_I2C_ADDR, 0x030e, 0x0000),
    (MAX9295A_I2C_ADDR, 0x031e, 0x0030),
    (MAX9295A_I2C_ADDR, 0x0112, 0x000e),
    (MAX9295A_I2C_ADDR, 0x0315, 0x00d2),
    (MAX9295A_I2C_ADDR, 0x0312, 0x000f),
    (MAX9295A_I2C_ADDR, 0x031a, 0x005e),
    (MAX9295A_I2C_ADDR, 0x031b, 0x0052),
    (MAX9295A_I2C_ADDR, 0x030f, 0x0008),
    (MAX9295A_I2C_ADDR, 0x0310, 0x0000),
    (MAX9295A_I2C_ADDR, 0x031f, 0x0030),
    (MAX9295A_I2C_ADDR, 0x011a, 0x000e),
    (MAX9295A_I2C_ADDR, 0x0315, 0x00d2),
    (MAX9296A_I2C_ADDR, 0x040b, 0x000f),
    (MAX9296A_I2C_ADDR, 0x040d, 0x001e),
    (MAX9296A_I2C_ADDR, 0x040e, 0x001e),
    (MAX9296A_I2C_ADDR, 0x040f, 0x0000),
    (MAX9296A_I2C_ADDR, 0x0410, 0x0000),
    (MAX9296A_I2C_ADDR, 0x0411, 0x0001),
    (MAX9296A_I2C_ADDR, 0x0412, 0x0001),
    (MAX9296A_I2C_ADDR, 0x0413, 0x0012),
    (MAX9296A_I2C_ADDR, 0x0414, 0x0012),
    (MAX9296A_I2C_ADDR, 0x042d, 0x0055),
    (MAX9296A_I2C_ADDR, 0x0100, 0x0023),
    (MAX9296A_I2C_ADDR, 0x1458, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1459, 0x0068),
    (MAX9296A_I2C_ADDR, 0x1558, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1559, 0x0068),
    (MAX9296A_I2C_ADDR, 0x044a, 0x00D0),
    (MAX9296A_I2C_ADDR, 0x0320, 0x002f),
    (MAX9296A_I2C_ADDR, 0x0473, 0x0010),
    (MAX9296A_I2C_ADDR, 0x044b, 0x000f),
    (MAX9296A_I2C_ADDR, 0x044d, 0x005e),
    (MAX9296A_I2C_ADDR, 0x044e, 0x005e),
    (MAX9296A_I2C_ADDR, 0x044f, 0x0040),
    (MAX9296A_I2C_ADDR, 0x0450, 0x0040),
    (MAX9296A_I2C_ADDR, 0x0451, 0x0041),
    (MAX9296A_I2C_ADDR, 0x0452, 0x0041),
    (MAX9296A_I2C_ADDR, 0x0453, 0x0052),
    (MAX9296A_I2C_ADDR, 0x0454, 0x0052),
    (MAX9296A_I2C_ADDR, 0x046d, 0x0055),
    (MAX9296A_I2C_ADDR, 0x0112, 0x0023),
    (MAX9296A_I2C_ADDR, 0x1458, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1459, 0x0068),
    (MAX9296A_I2C_ADDR, 0x1558, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1559, 0x0068),
    (MAX9296A_I2C_ADDR, 0x044a, 0x00D0),
    (MAX9296A_I2C_ADDR, 0x0320, 0x002f),
    (MAX9296A_I2C_ADDR, 0x0473, 0x0010),
    (MAX9296A_I2C_ADDR, 0x048b, 0x000f),
    (MAX9296A_I2C_ADDR, 0x048d, 0x009e),
    (MAX9296A_I2C_ADDR, 0x048e, 0x009e),
    (MAX9296A_I2C_ADDR, 0x048f, 0x0080),
    (MAX9296A_I2C_ADDR, 0x0490, 0x0080),
    (MAX9296A_I2C_ADDR, 0x0491, 0x0081),
    (MAX9296A_I2C_ADDR, 0x0492, 0x0081),
    (MAX9296A_I2C_ADDR, 0x0493, 0x0092),
    (MAX9296A_I2C_ADDR, 0x0494, 0x0092),
    (MAX9296A_I2C_ADDR, 0x04ad, 0x0055),
    (MAX9296A_I2C_ADDR, 0x0124, 0x0023),
    (MAX9296A_I2C_ADDR, 0x1458, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1459, 0x0068),
    (MAX9296A_I2C_ADDR, 0x1558, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1559, 0x0068),
    (MAX9296A_I2C_ADDR, 0x044a, 0x00D0),
    (MAX9296A_I2C_ADDR, 0x0320, 0x002f),
    (MAX9296A_I2C_ADDR, 0x0473, 0x0010),
    (MAX9296A_I2C_ADDR, 0x04cb, 0x000f),
    (MAX9296A_I2C_ADDR, 0x04cd, 0x00de),
    (MAX9296A_I2C_ADDR, 0x04ce, 0x00de),
    (MAX9296A_I2C_ADDR, 0x04cf, 0x00c0),
    (MAX9296A_I2C_ADDR, 0x04d0, 0x00c0),
    (MAX9296A_I2C_ADDR, 0x04d1, 0x00c1),
    (MAX9296A_I2C_ADDR, 0x04d2, 0x00c1),
    (MAX9296A_I2C_ADDR, 0x04d3, 0x00d2),
    (MAX9296A_I2C_ADDR, 0x04d4, 0x00d2),
    (MAX9296A_I2C_ADDR, 0x04ed, 0x0055),
    (MAX9296A_I2C_ADDR, 0x0136, 0x0023),
    (MAX9296A_I2C_ADDR, 0x1458, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1459, 0x0068),
    (MAX9296A_I2C_ADDR, 0x1558, 0x0028),
    (MAX9296A_I2C_ADDR, 0x1559, 0x0068),
    (MAX9296A_I2C_ADDR, 0x044a, 0x00D0),
    (MAX9296A_I2C_ADDR, 0x0320, 0x002f),
    (MAX9296A_I2C_ADDR, 0x0473, 0x0010),
]

# Register to start rgb streaming
REALSENSE_START_STREAMING_RGB = [
    (MUX_I2C_ADDR, 0x1000, 0x0201),
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register to start depth streaming
REALSENSE_START_STREAMING_DEPTH = [
    (MUX_I2C_ADDR, 0x1000, 0x0200),
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register to stop rgb streaming
REALSENSE_STOP_STREAMING_RGB = [
    (MUX_I2C_ADDR, 0x1000, 0x0101),
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register to stop depth streaming
REALSENSE_STOP_STREAMING_DEPTH = [
    (MUX_I2C_ADDR, 0x1000, 0x0100),
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

REALSENSE_STOP_ALL_STREAM = REALSENSE_STOP_STREAMING_RGB + REALSENSE_STOP_STREAMING_DEPTH
REALSENSE_START_ALL_STREAM = (
    REALSENSE_START_STREAMING_RGB + REALSENSE_START_STREAMING_DEPTH
)

DS5_RGB_CONFIG_STATUS = 0x4802
DS5_RGB_STREAM_STATUS = 0x1008

DS5_DEPTH_CONFIG_STATUS = 0x4800
DS5_DEPTH_STREAM_STATUS = 0x1004

DS5_STATUS_STREAMING = 0x1
DS5_STREAM_STREAMING = 0x2


# Register configuration for depth stream (640x480 @ 30fps)
realsense_mode_depth_640x480_30fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4000, 0x0031),  # Stream config: Depth only
    (MUX_I2C_ADDR, 0x4002, 0x0012),  # No metadata
    (MUX_I2C_ADDR, 0x401C, 0x001E),  # Override output DT
    (MUX_I2C_ADDR, 0x400C, 0x001E),  # 30 fps
    (MUX_I2C_ADDR, 0x4004, 0x0280),  # Width = 640
    (MUX_I2C_ADDR, 0x4008, 0x01E0),  # Height = 480
    (MUX_I2C_ADDR, 0x1000, 0x0200),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (640x480 @ 60fps)
realsense_mode_depth_640x480_60fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4000, 0x0031),  # Stream config: Depth only
    (MUX_I2C_ADDR, 0x4002, 0x0012),  # No metadata
    (MUX_I2C_ADDR, 0x401C, 0x001E),  # Override output DT
    (MUX_I2C_ADDR, 0x400C, 0x003C),  # 60 fps
    (MUX_I2C_ADDR, 0x4004, 0x0280),  # Width = 640
    (MUX_I2C_ADDR, 0x4008, 0x01E0),  # Height = 480
    (MUX_I2C_ADDR, 0x1000, 0x0200),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1280x720 @ 30fps)
realsense_mode_depth_1280x720_30fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4000, 0x0031),  # Stream config: Depth only
    (MUX_I2C_ADDR, 0x4002, 0x0012),  # No metadata
    (MUX_I2C_ADDR, 0x401C, 0x001E),  # Override output DT
    (MUX_I2C_ADDR, 0x400C, 0x001E),  # 30 fps
    (MUX_I2C_ADDR, 0x4004, 0x0500),  # Width = 1280
    (MUX_I2C_ADDR, 0x4008, 0x02D0),  # Height = 720
    (MUX_I2C_ADDR, 0x1000, 0x0200),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1280x720 @ 60fps)
realsense_mode_depth_1280x720_60fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4000, 0x0031),  # Stream config: Depth only
    (MUX_I2C_ADDR, 0x4002, 0x0012),  # No metadata
    (MUX_I2C_ADDR, 0x401C, 0x001E),  # Override output DT
    (MUX_I2C_ADDR, 0x400C, 0x003C),  # 60 fps
    (MUX_I2C_ADDR, 0x4004, 0x0500),  # Width = 1280
    (MUX_I2C_ADDR, 0x4008, 0x02D0),  # Height = 720
    (MUX_I2C_ADDR, 0x1000, 0x0200),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for color stream (640x480 @ 30fps)
realsense_mode_color_640x480_30fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x001E),  # 30 fps
    (MUX_I2C_ADDR, 0x4024, 0x0280),  # Width = 640
    (MUX_I2C_ADDR, 0x4028, 0x01E0),  # Height = 480
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]


# Register configuration for color stream (640x480 @ 60fps)
realsense_mode_color_640x480_60fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x003C),  # 60 fps
    (MUX_I2C_ADDR, 0x4024, 0x0280),  # Width = 640
    (MUX_I2C_ADDR, 0x4028, 0x01E0),  # Height = 480
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1280x720 @ 30fps)
realsense_mode_color_1280x720_30fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x001E),  # 30 fps
    (MUX_I2C_ADDR, 0x4024, 0x0500),  # Width = 1280
    (MUX_I2C_ADDR, 0x4028, 0x02D0),  # Height = 720
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1280x720 @ 60fps)
realsense_mode_color_1280x720_60fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x003C),  # 60 fps
    (MUX_I2C_ADDR, 0x4024, 0x0500),  # Width = 1280
    (MUX_I2C_ADDR, 0x4028, 0x02D0),  # Height = 720
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1920x1080 @ 30fps)
realsense_mode_color_1920x1080_30fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x001E),  # 30 fps
    (MUX_I2C_ADDR, 0x4024, 0x0780),  # Width = 1920
    (MUX_I2C_ADDR, 0x4028, 0x0438),  # Height = 1080
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

# Register configuration for depth stream (1920x1080 @ 60fps)
realsense_mode_color_1920x1080_60fps = [
    (MUX_I2C_ADDR, 0x0400, 0x0001),  # MIPI: 2 lanes
    (MUX_I2C_ADDR, 0x0402, 0x04E6),  # Data rate
    (MUX_I2C_ADDR, 0x4020, 0x001E),  # Stream config: RGB only
    (MUX_I2C_ADDR, 0x4022, 0x0112),  # No metadata
    (MUX_I2C_ADDR, 0x402C, 0x003C),  # 30 fps
    (MUX_I2C_ADDR, 0x4024, 0x0780),  # Width = 1920
    (MUX_I2C_ADDR, 0x4028, 0x0438),  # Height = 1080
    (MUX_I2C_ADDR, 0x1000, 0x0201),  # Start stream
    (MUX_I2C_ADDR, REALSENSE_TABLE_WAIT_MS, REALSENSE_TABLE_WAIT_MS),
]

class RealSense_Mode(Enum):
    REALSENSE_MODE_DEPTH_640x480_30FPS = 0
    REALSENSE_MODE_DEPTH_640x480_60FPS = 1
    REALSENSE_MODE_DEPTH_1280x720_30FPS = 2
    REALSENSE_MODE_DEPTH_1280x720_60FPS = 3
    REALSENSE_MODE_RGB_640x480_30FPS = 4
    REALSENSE_MODE_RGB_640x480_60FPS = 5
    REALSENSE_MODE_RGB_1280x720_30FPS = 6
    REALSENSE_MODE_RGB_1280x720_60FPS = 7
    REALSENSE_MODE_RGB_1920x1080_30FPS = 8
    REALSENSE_MODE_RGB_1920x1080_60FPS = 9
    Unknown = 10

frame_format = namedtuple(
    "FrameFormat", ["width", "height", "framerate", "pixel_format", "start_registers",
                     "config_status_register", "stream_status_register", "stop_registers"]
)

realsense_frame_format = []
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_DEPTH_640x480_30FPS.value,
    frame_format(640, 480, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.Z16,
                  realsense_mode_depth_640x480_30fps, DS5_DEPTH_CONFIG_STATUS, 
                  DS5_DEPTH_STREAM_STATUS, REALSENSE_STOP_STREAMING_DEPTH),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_DEPTH_640x480_60FPS.value,
    frame_format(640, 480, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.Z16,
                  realsense_mode_depth_640x480_60fps, DS5_DEPTH_CONFIG_STATUS,
                    DS5_DEPTH_STREAM_STATUS, REALSENSE_STOP_STREAMING_DEPTH),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_DEPTH_1280x720_30FPS.value,
    frame_format(1280, 720, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.Z16,
                  realsense_mode_depth_1280x720_30fps, DS5_DEPTH_CONFIG_STATUS,
                    DS5_DEPTH_STREAM_STATUS, REALSENSE_STOP_STREAMING_DEPTH),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_DEPTH_1280x720_60FPS.value,
    frame_format(1280, 720, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.Z16,
                  realsense_mode_depth_1280x720_60fps, DS5_DEPTH_CONFIG_STATUS,
                    DS5_DEPTH_STREAM_STATUS, REALSENSE_STOP_STREAMING_DEPTH),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_640x480_30FPS.value,
    frame_format(640, 480, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_640x480_30fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_640x480_60FPS.value,
    frame_format(640, 480, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_640x480_60fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_1280x720_30FPS.value,
    frame_format(1280, 720, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_1280x720_30fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_1280x720_60FPS.value,
    frame_format(1280, 720, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_1280x720_60fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_1920x1080_30FPS.value,
    frame_format(1920, 1080, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_1920x1080_30fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)
realsense_frame_format.insert(
    RealSense_Mode.REALSENSE_MODE_RGB_1920x1080_60FPS.value,
    frame_format(1920, 1080, 30, hololink_module.operators.ImageDecoderOp.PixelFormat.YUYV,
                  realsense_mode_color_1920x1080_60fps, DS5_RGB_CONFIG_STATUS,
                    DS5_RGB_STREAM_STATUS, REALSENSE_STOP_STREAMING_RGB),
)

