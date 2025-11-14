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
# hslcompile: compile HSL source in pyhsl format.

import argparse
import logging
import sys
import os
from hslcpackager import *
from pyhslcore import *
from pathlib import Path

if __name__ == '__main__':

    pyhslassert(sys.version_info >= (3, 9), 'pyhsl requires Python 3.9 or later')

    logging.basicConfig()
    mylogger = logging.getLogger('pyhsl')
    mylogger.setLevel(logging.INFO)

    parser = argparse.ArgumentParser(description='Compile HSL source in pyhsl format')
    parser.add_argument('-i', '--input', help='source file to compile',
                        metavar='source-file')
    parser.add_argument('-o', '--output', help=f'.{HSL_CONTAINER_FILE_EXTENSION} output file',
                        default=f'out.{HSL_CONTAINER_FILE_EXTENSION}',
                        metavar='output-file')
    parser.add_argument('-H', '--hslb_directory', help=f'output directory for .{HSL_RAW_BYTECODE_FILE_EXTENSION} files',
                        default=None,
                        metavar='hslb-directory')
    parser.add_argument('-D', '--define', help='define a configuration parameter',
                        action='append',
                        metavar='parameter=value')
    parser.add_argument('-l', '--loglevel', help='Logging level',
                        default='warning', choices=('debug', 'info', 'warning', 'error', 'critical'))
    args = parser.parse_args()

    mylogger.setLevel(args.loglevel.upper())

    output_file = Path(args.output)
    if not output_file.is_file():
        pyhslassert(not output_file.exists(), f'{args.output} already exists and is not a file')
    else:
        mylogger.warning(f'Overwriting existing file {output_file.resolve()}')

    if args.hslb_directory:
        hslb_dir = Path(args.hslb_directory)
        if not hslb_dir.is_dir():
            pyhslassert(not hslb_dir.exists(), f'{args.hslb_directory} already exists and is not a directory')
            mylogger.debug(f'Creating output directory {hslb_dir.resolve()}')
            hslb_dir.mkdir()

    input_file = Path(args.input)
    pyhslassert(input_file.is_file(), f'{args.input} is not a file')

    config_params = {}
    if args.define:
        for arg in args.define:
            (name, value) = arg.split('=', 1)
            pyhslassert(value, f'Configuration parameter {name} should have format "name=value"')
            config_params[name] = value
    mylogger.debug(f'Params: {config_params}')

    packager = HslcPackager()
    Configuration.set_values(config_params, packager, mylogger, hslb_dir if args.hslb_directory else None)

    # Import the source file as a module. This code is complex because we want to
    # import source files with extensions other than .py (i.e. .hsl).
    import importlib.machinery
    import importlib.util
    abspath = os.path.abspath(args.input)
    sourcedir = os.path.dirname(abspath)
    sourcebase = os.path.basename(abspath)
    if len(sourcedir) > 0:
        sys.path.insert(0, sourcedir)
    else:
        sys.path.insert(0, '.')
    module_name = sourcebase.removesuffix(".py")
    spec = importlib.util.spec_from_loader(module_name, importlib.machinery.SourceFileLoader(module_name, abspath))
    sourcemodule = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = sourcemodule
    spec.loader.exec_module(sourcemodule)
    main = sourcemodule.main

    main()

    with open(output_file, 'wb') as f:
        packager.package(f)
