# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
"""D555e stream profiles, shared by the RoCE apps and scripts (no GPU dependencies).

Profile index = position in the camera firmware's tables (HKR device-mgr/hsb/src/HsbProfiles.cpp).
Left out: depth 16 (256x144@90 self-calibration) and RGB 17 (BYR2 Bayer).

    python3 d555_profiles.py    # one "stream profile width height fps" line per profile
"""

from collections import namedtuple

BYTES_PER_PIXEL = 2  # depth Z16, RGB YUYV

# stream_id: the firmware's HsbStreamID; sensor: the host's DataChannel.use_sensor() index
Stream = namedtuple("Stream", "stream_id sensor profiles")

STREAMS = {
    "depth": Stream(2, 0, {
        0: (896, 504, 30), 1: (896, 504, 15), 2: (896, 504, 5), 3: (896, 504, 60),
        4: (1280, 720, 30), 5: (1280, 720, 15), 6: (1280, 720, 5),
        7: (640, 360, 60), 8: (640, 360, 30), 9: (640, 360, 15), 10: (640, 360, 5),
        11: (448, 252, 60), 12: (448, 252, 30), 13: (448, 252, 15), 14: (448, 252, 5),
        15: (1280, 800, 15),
    }),
    "rgb": Stream(0, 1, {
        0: (896, 504, 30), 1: (896, 504, 15), 2: (896, 504, 5), 3: (896, 504, 60),
        4: (1280, 800, 30), 5: (1280, 800, 15),
        6: (1280, 720, 30), 7: (1280, 720, 15), 8: (1280, 720, 5),
        9: (640, 360, 60), 10: (640, 360, 30), 11: (640, 360, 15), 12: (640, 360, 5),
        13: (448, 252, 60), 14: (448, 252, 30), 15: (448, 252, 15), 16: (448, 252, 5),
    }),
}
DEFAULT_PROFILE = {"depth": 8, "rgb": 10}  # 640x360@30


if __name__ == "__main__":
    for name, stream in STREAMS.items():
        for index, (width, height, fps) in stream.profiles.items():
            print(name, index, width, height, fps)
