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

import os
import pyhsl
import opal



def server_info():
    addr = os.environ.get('OPALSERVER', None)
    try:
        a = addr.split(':')
        return (a[0],int(a[1]))
    except:
        return (None,0)

host,port = server_info()

g_client = None
if host:
    g_client = opal.opalClient(host,port)



def sequence_start():
    if g_client:
        try:
            ret = g_client.sequence_start()
            if ret:
                pyhsl.ph_logger().error(ret)
                return False
        except Exception as e:
            pyhsl.ph_logger().error(str(e))
            return False
    return True

def sequence_bytecode(data):
    if g_client:
        try:
            ret = g_client.sequence_bytecode(data)
            if ret:
                pyhsl.ph_logger().error(ret)
                return False
        except Exception as e:
            pyhsl.ph_logger().error(str(e))
            return False
    return True

def sequence_end():
    if g_client:
        try:
            ret = g_client.sequence_end()
            if ret:
                pyhsl.ph_logger().error(ret)
                return False
        except Exception as e:
            pyhsl.ph_logger().error(str(e))
            return False
    return True
