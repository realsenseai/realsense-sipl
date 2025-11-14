#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#
# pyhslcore: core functionality for pyhsl and related tools.

import logging
from pathlib import Path

HSL_CONTAINER_FILE_EXTENSION = 'hslc'
HSL_RAW_BYTECODE_FILE_EXTENSION = 'hslb'

class PyHslException(Exception):
    pass

def pyhslassert(cond, message=None):
    if not cond:
        raise PyHslException(message)

class Configuration:
    """
    Read-only configuration data coming from compiler invocation.
    """

    s_params = {}
    s_logger = None
    s_packager = None
    s_output_dir = None
    @staticmethod
    def set_values(params:dict, packager, logger:logging.Logger, output_dir:Path=None):
        Configuration.s_params = params
        Configuration.s_packager = packager
        Configuration.s_logger = logger
        Configuration.s_output_dir = output_dir

    @staticmethod
    def set_default_logger(logLevel:str):
        if not Configuration.s_logger:
            logging.basicConfig()
            Configuration.s_logger = logging.getLogger('pyhsl')
            Configuration.s_logger.setLevel(logLevel)

