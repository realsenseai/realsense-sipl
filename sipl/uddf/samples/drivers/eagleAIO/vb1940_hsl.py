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
from vb1940_data import vb1940_content

from enum import Enum


# set up logging
Configuration.set_default_logger('DEBUG')
if __name__ == '__main__':
    packager = HslcPackager()
    Configuration.set_values({}, packager, ph_logger(), None)


# Camera I2C address.
CAM_I2C_ADDRESS      = 0x10

# register map
DEVICE_MODEL_ID      = 0x0000
DEVICE_REVISION_REG  = 0x0004
SYSTEM_FSM_STATE_REG = 0x0044
BOOT_FSM_REG         = 0x0200
SYSTEM_UP_REG        = 0x0514
BOOT_REG             = 0x0515
SW_STBY_REG          = 0x0516
STREAMING_REG        = 0x0517



class system_fsm_state(Enum):
    HW_STBY    = 0x0
    SYSTEM_UP  = 0x1
    BOOT       = 0x2
    SW_STBY    = 0x3
    STREAMING  = 0x4
    STALL      = 0x5
    HALT       = 0x6


class boot_fsm_state(Enum):
    HW_STBY                        = 0x00
    COLD_BOOT                      = 0x01
    CLOCK_INIT                     = 0x02
    NVM_DWLD                       = 0x10
    NVM_UNPACK                     = 0x11
    SYSTEM_BOOT                    = 0x12
    NONCE_GRNERATION               = 0x20
    EPH_KEYS_GENERATION            = 0x21
    WAIT_CERTIFICATE               = 0x22
    CERTIFICATE_PARSING            = 0x23
    CERIFICATE_VERIF_ROOT          = 0x24
    CERTIFICATE_VERIF_USER         = 0x25
    CERTIFICATE_CHECK_FIELDS       = 0x26
    ECDH                           = 0x30
    ECDH_SS_GEN                    = 0x31
    ECDH_MASTER_KEY_GEN            = 0x32
    ECDH_SESSION_KEY_GEN           = 0x33
    AUTHENTICATION                 = 0x40
    AUTHENTICATION_MSG_CREATE      = 0x41
    AUTHENTICATION_MSG_SIGN        = 0x42
    PROVISIONING                   = 0x50
    PROVISIONING_UID               = 0x51
    PROVISIONING_EC_PRIV_KEY       = 0x52
    PROVISIONING_EC_PUB_KEY        = 0x53
    CTM_PROVISIONING_CID           = 0x54
    CTM_PROVISIONING_OEM_ROOT_KEY  = 0x54
    PAIRING                        = 0x55
    FWP_SETUP                      = 0x60
    VTP_SETUP                      = 0x61
    FA_RETURN                      = 0x70
    BOOT_WAITING_CMD               = 0x80
    BOOT_COMPLETED                 = 0xBC


def write_buffer(dev, offset, data):
    chunk_size = 120
    with dev:
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i+chunk_size]
            write(offset+i, chunk)

# Setup the devices
vb1940 = I2CDevice(CAM_I2C_ADDRESS, 16, 8, 'vb1940')

def seq_verify_model():
    with Sequence('verify_model'):
        annotate('verify_model')
        expected = [ ord(c) for c in '049S' ]
        with vb1940:
            readVerifyStream(DEVICE_MODEL_ID, expected)

def seq_start():
    with Sequence('start'):
        annotate('start')
        with vb1940:
            readVerify(SYSTEM_FSM_STATE_REG, system_fsm_state.SW_STBY.value, 0xff)
            write(SW_STBY_REG, 0x01),  # start streaming
            delay(0x64)
            poll(SW_STBY_REG, 0x00, 0xff, 10, 30)
            poll(SYSTEM_FSM_STATE_REG, system_fsm_state.STREAMING.value, 0xff, 10, 30)

def seq_stop():
    with Sequence('stop'):
        annotate('stop')
        with vb1940:
            write(STREAMING_REG, 0x01),  # stop streaming
            poll(STREAMING_REG, 0x00, 0xff, 10, 30)

def seq_secure_boot():
    with Sequence('secure_boot'):
        annotate('secure_boot')
        with vb1940:
            write(SYSTEM_UP_REG, 0x01)
        annotate('secure_boot: certificate')
        write_buffer(vb1940, 0x1AA8, vb1940_content.certificate)
        with vb1940:
            write(BOOT_REG, 0x01)
            poll(BOOT_FSM_REG, boot_fsm_state.BOOT_WAITING_CMD.value, 0xff, 15, 250)
        annotate('secure_boot: fw_update')
        write_buffer(vb1940, 0x2000, vb1940_content.fw_update)
        with vb1940:
            write(BOOT_REG, 0x02)
            poll(BOOT_FSM_REG, boot_fsm_state.FWP_SETUP.value, 0xff, 15, 250)
            write(BOOT_REG, 0x10)
            poll(BOOT_FSM_REG, boot_fsm_state.BOOT_COMPLETED.value, 0xff, 15, 250)



def seq_m2560X1984_30fps():
    with Sequence('m2560X1984_30fps'):
        annotate('m2560X1984_30fps')
        with vb1940:
            # MIPI_DATA_RATE(570MHz) 0x21F98280
            write(0x738, 0x80)  # least significant byte
            write(0x739, 0x82)
            write(0x73A, 0xF9)
            write(0x73B, 0x21)  # most significant byte
            # ROI_B
            write(0x916, 0x00)
            write(0x917, 0x00)  # width offset
            write(0x918, 0x00)
            write(0x919, 0x00)  # height offset
            write(0x91A, 0x00)
            write(0x91B, 0x0A)  # width:2560 0xA00
            write(0x91C, 0xC0)
            write(0x91D, 0x07)  # height:1984 0x7C0
            write(0x91E, 0x2B)  # data type RAW10
            # slave mode and GPIO0 as input
            write(0xAC6, 0x00)  # master mode
            write(0xAD4, 0x05)  # GPIO0 FSYNC_IN
            write(0xAD6, 0x00)  # GPIO2 strobe
            write(0xAD7, 0x00)  # GPIO3 strobe
            # context switch configuration
            write(0xADC, 0x01)  # CONTEXT_SWITCH_SEQUENCE_VECTOR_0
            write(0xADD, 0x00)
            write(0xADE, 0x00)
            write(0xADF, 0x00)
            write(0xAE0, 0x00)  # CONTEXT_SWITCH_SEQUENCE_VECTOR_1
            write(0xAE1, 0x00)
            write(0xAE2, 0x00)
            write(0xAE3, 0x00)
            write(0xAE4, 0x00)  # CONTEXT_SWITCH_LOOP_ELEMENT
            # stream static global
            write(0x934, 0x48)  # LINE_LENGTH 0x2B48 = 11080d, line time = 11080/372 = 29.785us
            write(0x935, 0x2B)
            write(0x937, 0x00)  # ORIENTATION (0=normal)
            # stream static ctx1
            write(0xB88, 0x06)  # CONFIG6_GS_OP_RGB_PWLOFF_SINGLE_EXP_VTSS1_DESCSS1_CFA_BAYER_RAW10_31
            write(0xB8E, 0x68)  # FRAME_LENGTH(frame time = 1128*29.785=33.597ms)
            write(0xB8F, 0x04)
            write(0xB8C, 0x00)  # vc0
            write(0xB90, 0x02)  # roi selection(2= Enable ROI_B)
            write(0xB91, 0x30)  # disable GPIO0-3
            # stream dynamic ctx1
            write(0xCA1, 0x00)  # analog gain(1)
            write(0xCA2, 0xF4)  # INTEGRATION_TIME_PRIMARY (500) 0x1F4
            write(0xCA3, 0x01)

def seq_vt_patch():
    with Sequence('vt_patch'):
        annotate('vt_patch')

        annotate('vt_patch: ldec_ram @ 0x2000')
        write_buffer(vb1940, 0x2000, vb1940_content.ldec_ram)

        annotate('vt_patch: rd_ram_seq_1 @ 0x5000')
        write_buffer(vb1940, 0x5000, vb1940_content.rd_ram_seq_1)

        annotate('vt_patch: gt_ram_pat @ 0x5BC0')
        write_buffer(vb1940, 0x5BC0, vb1940_content.gt_ram_pat)

        annotate('vt_patch: gt_ram_seq_1 @ 0x5E40')
        write_buffer(vb1940, 0x5E40, vb1940_content.gt_ram_seq_1)

        annotate('vt_patch: gt_ram_seq_2 @ 0x5F80')
        write_buffer(vb1940, 0x5F80, vb1940_content.gt_ram_seq_2)

        annotate('vt_patch: gt_ram_seq_3 @ 0x60C0')
        write_buffer(vb1940, 0x60C0, vb1940_content.gt_ram_seq_3)

        annotate('vt_patch: gt_ram_seq_4 @ 0x61C0')
        write_buffer(vb1940, 0x61C0, vb1940_content.gt_ram_seq_4)

        annotate('vt_patch: rd_ram_pat @ 0x6AC0')
        write_buffer(vb1940, 0x6AC0, vb1940_content.rd_ram_pat)


# start the main dump

def main():

    seq_verify_model()

    seq_secure_boot()
    seq_vt_patch()

    seq_m2560X1984_30fps()

    seq_start()
    seq_stop()

    ph_logger().info('Success')

if __name__ == '__main__':
    main()
