#! /usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from pyhsl import *
from d555_data import d555_content

from enum import Enum


# set up logging
Configuration.set_default_logger('DEBUG')
if __name__ == '__main__':
    packager = HslcPackager()
    Configuration.set_values({}, packager, ph_logger(), None)


# Camera I2C address.
CAM_I2C_ADDRESS      = 0x1A

# register map
SYSTEM_FSM_STATE_REG = 0x000
PROFILE_COUNT = 17  # Total number of profiles for RealSense D555 camera per stream


class RealSense_StreamId(Enum):
    RGB = 0
    DEPTH = 2

class RealSense_StreamCommand(Enum):
    START_STREAM = 1
    STOP_STREAM = 2
    SET_PROFILE = 3
    
    
class RealSense_RGB_Mode(Enum):
    RGB_896x504_30FPS = 0 # 17 in single stream
    RGB_896x504_15FPS = 1 # 18
    RGB_896x504_5FPS  = 2 # 19
    RGB_896x504_60FPS = 3 # 20
    RGB_1280x800_30FPS = 4 # 21
    RGB_1280x800_15FPS = 5 # 22
    RGB_1280x720_30FPS = 6 # 23
    RGB_1280x720_15FPS = 7 # 24
    RGB_1280x720_5FPS = 8 # 25
    RGB_640x360_60FPS = 9 # 26
    RGB_640x360_30FPS = 10 # 27
    RGB_640x360_15FPS = 11 # 28
    RGB_640x360_5FPS = 12 # 29
    RGB_448x252_60FPS = 13 # 30
    RGB_448x252_30FPS = 14 # 31
    RGB_448x252_15FPS = 15 # 32
    RGB_448x252_5FPS = 16 # 33

class RealSense_Depth_Mode(Enum):
    DEPTH_896x504_30FPS = 0
    DEPTH_896x504_15FPS = 1
    DEPTH_896x504_5FPS  = 2
    DEPTH_896x504_60FPS = 3
    DEPTH_1280x720_30FPS = 4
    DEPTH_1280x720_15FPS = 5
    DEPTH_1280x720_5FPS = 6
    DEPTH_640x360_60FPS = 7 
    DEPTH_640x360_30FPS = 8
    DEPTH_640x360_15FPS = 9
    DEPTH_640x360_5FPS = 10
    DEPTH_448x252_60FPS = 11
    DEPTH_448x252_30FPS = 12
    DEPTH_448x252_15FPS = 13
    DEPTH_448x252_5FPS = 14
    DEPTH_1280x800_15FPS = 15
    DEPTH_256x144_90FPS = 16

def write_buffer(dev, offset, data):
    chunk_size = 120
    with dev:
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i+chunk_size]
            write(offset+i, chunk)

# Setup the devices
d555 = I2CDevice(CAM_I2C_ADDRESS, 16, 16, 'd555')

def seq_start():
    with Sequence('start'):
        annotate('start')
        with d555:
            pass

def seq_stop():
    with Sequence('stop'):
        annotate('stop')
        with d555:
            write(0x0000, 0x0002) # stop streaming RGB
            write(0x0000, 0x0202) # stop streaming DEPTH

# Per-stream stops. The combined 'stop' above decrements the FW start-count of BOTH
# streams, so using it after starting only one underflows the other counter and the FW
# then refuses every later profile update (start count wraps to 0xFFFFFFFD). Stopping
# only the stream that was started keeps the counts balanced, which is what lets a
# later run switch between depth and RGB without a camera reboot.
def seq_stop_rgb():
    with Sequence('stop_rgb'):
        annotate('stop_rgb')
        with d555:
            write(0x0000, 0x0002) # stop streaming RGB

def seq_stop_depth():
    with Sequence('stop_depth'):
        annotate('stop_depth')
        with d555:
            write(0x0000, 0x0202) # stop streaming DEPTH

# RGB sequences for all modes
def seq_rgb_m896x504_30fps():
    with Sequence('rgb_m896x504_30fps'):
        annotate('rgb_m896x504_30fps')
        with d555:
            write(0x0000, 0x0003) # set profile RGB 896x504_30fps (index 0)
            write(0x0000, 0x0001) # start streaming RGB

    
def seq_rgb_m896x504_15fps():
    with Sequence('rgb_m896x504_15fps'):
        annotate('rgb_m896x504_15fps')
        with d555:
            write(0x0001, 0x0003) # set profile RGB 896x504_15fps (index 1)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m896x504_5fps():
    with Sequence('rgb_m896x504_5fps'):
        annotate('rgb_m896x504_5fps')
        with d555:
            write(0x0002, 0x0003) # set profile RGB 896x504_5fps (index 2)
            write(0x0000, 0x0001) # start streaming RGB


def seq_rgb_m896x504_60fps():
    with Sequence('rgb_m896x504_60fps'):
        annotate('rgb_m896x504_60fps')
        with d555:
            write(0x0003, 0x0003) # set profile RGB 896x504_60fps (index 3)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m1280x800_30fps():
    with Sequence('rgb_m1280x800_30fps'):
        annotate('rgb_m1280x800_30fps')
        with d555:
            write(0x0004, 0x0003) # set profile RGB 1280x800_30fps (index 4)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m1280x800_15fps():
    with Sequence('rgb_m1280x800_15fps'):
        annotate('rgb_m1280x800_15fps')
        with d555:
            write(0x0005, 0x0003) # set profile RGB 1280x800_15fps (index 5)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m1280x720_30fps():
    with Sequence('rgb_m1280X720_30fps'):
        annotate('rgb_m1280X720_30fps')
        with d555:
            write(0x0006, 0x0003) # set profile RGB 1280x720_30fps (index 6)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m1280x720_15fps():
    with Sequence('rgb_m1280x720_15fps'):
        annotate('rgb_m1280x720_15fps')
        with d555:
            write(0x0007, 0x0003) # set profile RGB 1280x720_15fps (index 7)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m1280x720_5fps():
    with Sequence('rgb_m1280x720_5fps'):
        annotate('rgb_m1280x720_5fps')
        with d555:
            write(0x0008, 0x0003) # set profile RGB 1280x720_5fps (index 8)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m640x360_60fps():
    with Sequence('rgb_m640x360_60fps'):
        annotate('rgb_m640x360_60fps')
        with d555:
            write(0x0009, 0x0003) # set profile RGB 640x360_60fps (index 9)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m640x360_30fps():
    with Sequence('rgb_m640X360_30fps'):
        annotate('rgb_m640X360_30fps')
        with d555:
            write(0x000A, 0x0003) # set profile RGB 640x360_30fps (index 10)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m640x360_15fps():
    with Sequence('rgb_m640x360_15fps'):
        annotate('rgb_m640x360_15fps')
        with d555:
            write(0x000B, 0x0003) # set profile RGB 640x360_15fps (index 11)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m640x360_5fps():
    with Sequence('rgb_m640x360_5fps'):
        annotate('rgb_m640x360_5fps')
        with d555:
            write(0x000C, 0x0003) # set profile RGB 640x360_5fps (index 12)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m448x252_60fps():
    with Sequence('rgb_m448x252_60fps'):
        annotate('rgb_m448x252_60fps')
        with d555:
            write(0x000D, 0x0003) # set profile RGB 448x252_60fps (index 13)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m448x252_30fps():
    with Sequence('rgb_m448x252_30fps'):
        annotate('rgb_m448x252_30fps')
        with d555:
            write(0x000E, 0x0003) # set profile RGB 448x252_30fps (index 14)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m448x252_15fps():
    with Sequence('rgb_m448x252_15fps'):
        annotate('rgb_m448x252_15fps')
        with d555:
            write(0x000F, 0x0003) # set profile RGB 448x252_15fps (index 15)
            write(0x0000, 0x0001) # start streaming RGB

def seq_rgb_m448x252_5fps():
    with Sequence('rgb_m448x252_5fps'):
        annotate('rgb_m448x252_5fps')
        with d555:
            write(0x0010, 0x0003) # set profile RGB 448x252_5fps (index 16)
            write(0x0000, 0x0001) # start streaming RGB


# DEPTH sequences for all modes
def seq_depth_m896x504_30fps():
    with Sequence('depth_m896x504_30fps'):
        annotate('depth_m896x504_30fps')
        with d555:
            write(0x0000, 0x0203) # set profile DEPTH 896x504_30fps (index 4)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m896x504_15fps():
    with Sequence('depth_m896x504_15fps'):
        annotate('depth_m896x504_15fps')
        with d555:
            write(0x0001, 0x0203) # set profile DEPTH 896x504_15fps (index 1)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m896x504_5fps():
    with Sequence('depth_m896x504_5fps'):
        annotate('depth_m896x504_5fps')
        with d555:
            write(0x0002, 0x0203) # set profile DEPTH 896x504_5fps (index 2)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m896x504_60fps():
    with Sequence('depth_m896x504_60fps'):
        annotate('depth_m896x504_60fps')
        with d555:
            write(0x0003, 0x0203) # set profile DEPTH 896x504_60fps (index 3)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m1280x720_30fps():
    with Sequence('depth_m1280X720_30fps'):
        annotate('depth_m1280X720_30fps')
        with d555:
            write(0x0004, 0x0203) # set profile DEPTH 1280x720_30fps (index 4)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m1280x720_15fps():
    with Sequence('depth_m1280x720_15fps'):
        annotate('depth_m1280x720_15fps')
        with d555:
            write(0x0005, 0x0203) # set profile DEPTH 1280x720_15fps (index 5)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m1280x720_5fps():
    with Sequence('depth_m1280x720_5fps'):
        annotate('depth_m1280x720_5fps')
        with d555:
            write(0x0006, 0x0203) # set profile DEPTH 1280x720_5fps (index 6)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m640x360_60fps():
    with Sequence('depth_m640x360_60fps'):
        annotate('depth_m640x360_60fps')
        with d555:
            write(0x0007, 0x0203) # set profile DEPTH 640x360_60fps (index 7)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m640x360_30fps():
    with Sequence('depth_m640X360_30fps'):
        annotate('depth_m640X360_30fps')
        with d555:
            write(0x0008, 0x0203) # set profile DEPTH 640x360_30fps (index 8)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m640x360_15fps():
    with Sequence('depth_m640x360_15fps'):
        annotate('depth_m640x360_15fps')
        with d555:
            write(0x0009, 0x0203) # set profile DEPTH 640x360_15fps (index 9)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m640x360_5fps():
    with Sequence('depth_m640x360_5fps'):
        annotate('depth_m640x360_5fps')
        with d555:
            write(0x000A, 0x0203) # set profile DEPTH 640x360_5fps (index 10)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m448x252_60fps():
    with Sequence('depth_m448x252_60fps'):
        annotate('depth_m448x252_60fps')
        with d555:
            write(0x000B, 0x0203) # set profile DEPTH 448x252_60fps (index 11)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m448x252_30fps():
    with Sequence('depth_m448x252_30fps'):
        annotate('depth_m448x252_30fps')
        with d555:
            write(0x000C, 0x0203) # set profile DEPTH 448x252_30fps (index 12)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m448x252_15fps():
    with Sequence('depth_m448x252_15fps'):
        annotate('depth_m448x252_15fps')
        with d555:
            write(0x000D, 0x0203) # set profile DEPTH 448x252_15fps (index 13)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m448x252_5fps():
    with Sequence('depth_m448x252_5fps'):
        annotate('depth_m448x252_5fps')
        with d555:
            write(0x000E, 0x0203) # set profile DEPTH 448x252_5fps (index 14)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m1280x800_15fps():
    with Sequence('depth_m1280x800_15fps'):
        annotate('depth_m1280x800_15fps')
        with d555:
            write(0x000F, 0x0203) # set profile DEPTH 1280x800_15fps (index 15)
            write(0x0000, 0x0201) # start streaming DEPTH

def seq_depth_m256x144_90fps():
    with Sequence('depth_m256x144_90fps'):
        annotate('depth_m256x144_90fps')
        with d555:
            write(0x0010, 0x0203) # set profile DEPTH 256x144_90fps (index 16)
            write(0x0000, 0x0201) # start streaming DEPTH

# start the main dump

def main():
    # RGB sequences
    seq_rgb_m896x504_30fps()
    seq_rgb_m896x504_15fps()
    seq_rgb_m896x504_5fps()
    seq_rgb_m896x504_60fps()
    seq_rgb_m1280x800_30fps()
    seq_rgb_m1280x800_15fps()
    seq_rgb_m1280x720_30fps()
    seq_rgb_m1280x720_15fps()
    seq_rgb_m1280x720_5fps()
    seq_rgb_m640x360_60fps()
    seq_rgb_m640x360_30fps()
    seq_rgb_m640x360_15fps()
    seq_rgb_m640x360_5fps()
    seq_rgb_m448x252_60fps()
    seq_rgb_m448x252_30fps()
    seq_rgb_m448x252_15fps()
    seq_rgb_m448x252_5fps()

    # Depth sequences
    seq_depth_m896x504_30fps()
    seq_depth_m896x504_60fps()
    seq_depth_m896x504_15fps()
    seq_depth_m896x504_5fps()
    seq_depth_m1280x720_30fps()
    seq_depth_m1280x720_15fps()
    seq_depth_m1280x720_5fps()
    seq_depth_m640x360_60fps()
    seq_depth_m640x360_30fps()
    seq_depth_m640x360_15fps()
    seq_depth_m640x360_5fps()
    seq_depth_m448x252_60fps()
    seq_depth_m448x252_30fps()
    seq_depth_m448x252_15fps()
    seq_depth_m448x252_5fps()
    seq_depth_m1280x800_15fps()
    seq_depth_m256x144_90fps()

    # Control sequences
    seq_start()
    seq_stop()
    seq_stop_rgb()
    seq_stop_depth()

    ph_logger().info('Success')

if __name__ == '__main__':
    main()
