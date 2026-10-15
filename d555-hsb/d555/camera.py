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

"""Discover a D555 and start or stop one of its streams.

The D555 speaks the Holoscan Sensor Bridge control protocol itself, so it is reached with the
stock Hololink enumeration and DataChannel classes. Streams are controlled through one firmware
register: data[31:24] = stream ID, data[23:16] = command, data[15:0] = profile index.
"""

import logging

import hololink as hololink_module

from .profiles import STREAMS

STREAM_COMMAND_REGISTER = 0x03000210
COMMAND_START = 1
COMMAND_STOP = 2
COMMAND_SET_PROFILE = 3

# A stop is sent exactly once. Hololink's write retry resends with a new sequence number and the
# camera does not deduplicate, so a second stop after a lost reply would decrement the stream's
# start count twice; the camera then refuses profile changes until it is rebooted.
STOP_TIMEOUT_S = 2.0

ENUMERATION_TIMEOUT_S = 10


class D555Stream:
    """The device object a Holoscan Sensor Bridge receiver drives: start and stop one stream.

    The receiver operators call start() once the data channel is configured and stop() when the
    application ends.
    """

    def __init__(self, hololink_channel, name, profile):
        self._hololink = hololink_channel.hololink()
        self.name = name
        self._stream_id = STREAMS[name].stream_id
        self._profile = profile
        self.stop_failed = False

    def _command(self, command, profile=0, **write_args):
        value = (self._stream_id << 24) | (command << 16) | (profile & 0xFFFF)
        logging.debug(f"{self.name}: stream command 0x{value:08x}")
        return self._hololink.write_uint32(STREAM_COMMAND_REGISTER, value, **write_args)

    def start(self):
        logging.info(f"{self.name}: starting profile {self._profile}")
        self._command(COMMAND_SET_PROFILE, self._profile)
        self._command(COMMAND_START)

    def stop(self):
        # Without an acknowledgement the stop may or may not have landed, so it is reported as a
        # failure rather than retried (see STOP_TIMEOUT_S).
        try:
            acknowledged = self._command(
                COMMAND_STOP, timeout=hololink_module.Timeout(STOP_TIMEOUT_S), retry=False
            )
            if acknowledged is not False:
                logging.info(f"{self.name}: stopped")
                return
            logging.error(f"{self.name}: stream stop was not acknowledged")
        except Exception as error:
            logging.error(f"{self.name}: stream stop failed: {error}")
        self.stop_failed = True
        logging.error(
            f"{self.name}: the camera may still be streaming; power-cycle it before the next run"
        )


def find_camera(camera_ip):
    """Return the enumeration metadata of the D555 at camera_ip."""
    metadata = hololink_module.Enumerator.find_channel(
        channel_ip=camera_ip, timeout=hololink_module.Timeout(ENUMERATION_TIMEOUT_S)
    )
    # The D555 implements single-word register reads and writes only: no block writes and no
    # vsync generator.
    metadata.update(hololink_module.Metadata({"block_enable": 0, "vsync_enable": 0}))
    return metadata


def open_stream(enumeration, name, profile):
    """Return (DataChannel, D555Stream) for one stream, each on its own copy of the metadata."""
    metadata = hololink_module.Metadata(enumeration)
    hololink_module.DataChannel.use_sensor(metadata, STREAMS[name].sensor)
    channel = hololink_module.DataChannel(metadata)
    return channel, D555Stream(channel, name, profile)
