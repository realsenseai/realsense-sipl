# SPDX-FileCopyrightText: Copyright (c) 2026, RealSense AI. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""D555 stream profiles, as indexed by the camera firmware.

A profile index is the position in the firmware's per-stream profile table. Depth 16
(256x144 at 90 fps, used for self-calibration) and RGB 17 (Bayer) are not listed.

    python3 -m d555.profiles    # one "stream profile width height fps" line per profile
"""

from collections import namedtuple

# Depth is Z16 and RGB is YUYV: both are two bytes per pixel.
BYTES_PER_PIXEL = 2

# Above this total payload the camera's RoCE send path (about 540 Mbps) falls behind, and
# dual-stream runs past it have left the camera unable to take a stop command.
MAX_PAYLOAD_MBPS = 500

# stream_id: the firmware's stream ID; sensor: the DataChannel.use_sensor() index.
Stream = namedtuple("Stream", "stream_id sensor profiles")

STREAMS = {
    "depth": Stream(
        2,
        0,
        {
            0: (896, 504, 30),
            1: (896, 504, 15),
            2: (896, 504, 5),
            3: (896, 504, 60),
            4: (1280, 720, 30),
            5: (1280, 720, 15),
            6: (1280, 720, 5),
            7: (640, 360, 60),
            8: (640, 360, 30),
            9: (640, 360, 15),
            10: (640, 360, 5),
            11: (448, 252, 60),
            12: (448, 252, 30),
            13: (448, 252, 15),
            14: (448, 252, 5),
            15: (1280, 800, 15),
        },
    ),
    "rgb": Stream(
        0,
        1,
        {
            0: (896, 504, 30),
            1: (896, 504, 15),
            2: (896, 504, 5),
            3: (896, 504, 60),
            # 4 and 5, 1280x800, are refused by the camera.
            6: (1280, 720, 30),
            7: (1280, 720, 15),
            8: (1280, 720, 5),
            9: (640, 360, 60),
            10: (640, 360, 30),
            11: (640, 360, 15),
            12: (640, 360, 5),
            13: (448, 252, 60),
            14: (448, 252, 30),
            15: (448, 252, 15),
            16: (448, 252, 5),
        },
    ),
}

# 640x360 at 30 fps for both streams.
DEFAULT_PROFILE = {"depth": 8, "rgb": 10}


def payload_mbps(width, height, fps):
    """Payload bit rate of one stream, in Mbps."""
    return width * height * BYTES_PER_PIXEL * 8 * fps / 1e6


def total_payload_mbps(profiles):
    """Payload of a {stream name: profile index} set, in Mbps."""
    return sum(payload_mbps(*STREAMS[name].profiles[index]) for name, index in profiles.items())


def describe(name, index):
    width, height, fps = STREAMS[name].profiles[index]
    return f"{name} profile {index}: {width}x{height} at {fps} fps"


if __name__ == "__main__":
    for stream_name, stream in STREAMS.items():
        for profile_index, (w, h, f) in stream.profiles.items():
            print(stream_name, profile_index, w, h, f)
