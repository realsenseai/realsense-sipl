#! /usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from abc import ABC, abstractmethod
from pyhsl import *

class MAX967XX(ABC):
    I2C_ADDRESS = None
    DESER_TYPE = None

    def __init__(self):
        if self.I2C_ADDRESS is None:
            raise ValueError("I2C_ADDRESS must be set by subclass")
        if self.DESER_TYPE is None:
            raise ValueError("DESER_TYPE must be set by subclass")
        ph_define_constant('MAX967XX_HSL_I2C_ADDRESS', self.I2C_ADDRESS)
        self.max967xx = I2CDevice(self.I2C_ADDRESS, 16, 8, f'max{self.DESER_TYPE}')

        link_reset_data_layout = MemoryLayout("LinkResetData")
        link_reset_data_layout.addItem('linkResetData', 1)
        self.link_reset_data_block = MemoryBlock(link_reset_data_layout, 'link_reset_data_block')

        link_index_data_layout = MemoryLayout("LinkIndexData")
        link_index_data_layout.addItem('linkIndex', 1)
        self.link_index_data_block = MemoryBlock(link_index_data_layout, 'link_index_data_block')

        errb_data_layout = MemoryLayout("ErrbData")
        errb_data_layout.addItem('ERRBData', 1)
        self.errb_data_block = MemoryBlock(errb_data_layout, 'errb_data_block')

        device_id_data_layout = MemoryLayout("DeviceIdData")
        device_id_data_layout.addItem('deviceId', 1)
        device_id_data_layout.addItem('revisionId', 1)
        self.device_id_data_block = MemoryBlock(device_id_data_layout, 'device_id_data_block')

        mipi_phy_datarate_config_layout = MemoryLayout("MipiPhyDatarateConfig")
        mipi_phy_datarate_config_layout.addItem('mipiSpeed', 1)
        mipi_phy_datarate_config_layout.addItem('reg_0415_current', 1)
        mipi_phy_datarate_config_layout.addItem('reg_0418_current', 1)
        mipi_phy_datarate_config_layout.addItem('reg_041B_current', 1)
        mipi_phy_datarate_config_layout.addItem('reg_041E_current', 1)
        mipi_phy_datarate_config_layout.addItem('reg_0415_value', 1)
        mipi_phy_datarate_config_layout.addItem('reg_0418_value', 1)
        mipi_phy_datarate_config_layout.addItem('reg_041B_value', 1)
        mipi_phy_datarate_config_layout.addItem('reg_041E_value', 1)
        self.mipi_phy_datarate_config_block = MemoryBlock(mipi_phy_datarate_config_layout, 'mipi_phy_datarate_config_block')

        video_pipeline_mapping_link_a_layout = MemoryLayout("VideoPipelineMappingDataLinkA")
        video_pipeline_mapping_link_a_layout.addItem('reg_092D', 1)
        video_pipeline_mapping_link_a_layout.addItem('reg_0939', 1)
        self.video_pipeline_mapping_link_a_block = MemoryBlock(video_pipeline_mapping_link_a_layout, 'video_pipeline_mapping_link_a_block')

        video_pipeline_mapping_link_b_layout = MemoryLayout("VideoPipelineMappingDataLinkB")
        video_pipeline_mapping_link_b_layout.addItem('reg_096D', 1)
        video_pipeline_mapping_link_b_layout.addItem('reg_0979', 1)
        self.video_pipeline_mapping_link_b_block = MemoryBlock(video_pipeline_mapping_link_b_layout, 'video_pipeline_mapping_link_b_block')

        video_pipeline_mapping_link_c_layout = MemoryLayout("VideoPipelineMappingDataLinkC")
        video_pipeline_mapping_link_c_layout.addItem('reg_09AD', 1)
        video_pipeline_mapping_link_c_layout.addItem('reg_09B9', 1)
        self.video_pipeline_mapping_link_c_block = MemoryBlock(video_pipeline_mapping_link_c_layout, 'video_pipeline_mapping_link_c_block')

        video_pipeline_mapping_link_d_layout = MemoryLayout("VideoPipelineMappingDataLinkD")
        video_pipeline_mapping_link_d_layout.addItem('reg_09ED', 1)
        video_pipeline_mapping_link_d_layout.addItem('reg_09F9', 1)
        self.video_pipeline_mapping_link_d_block = MemoryBlock(video_pipeline_mapping_link_d_layout, 'video_pipeline_mapping_link_d_block')

        # ---- External FSYNC configuration (TX ID is serializer-dependent) ----
        # Each link's GPIO TX ID register value is computed by the C++ runtime as
        # (base_value | fsync_tx_id) so that the TX ID matches the connected
        # serializer's RX ID (e.g. 0x02 for MAX9295, 0x1F for MAX96717).
        fsync_config_layout = MemoryLayout("FsyncConfig")
        fsync_config_layout.addItem('linkA_gpio_tx_id', 1)
        fsync_config_layout.addItem('linkB_gpio_tx_id', 1)
        fsync_config_layout.addItem('linkC_gpio_tx_id', 1)
        fsync_config_layout.addItem('linkD_gpio_tx_id', 1)
        self.fsync_config_block = MemoryBlock(fsync_config_layout, 'fsync_config_block')

        # ---- Dual-sensor pipeline mapping (shared layout for all links) ----
        # Each link uses two consecutive pipes with an identical register structure;
        # only the base addresses differ (pipe_base = 0x0900 + pipe_index * 0x40).
        # Includes mapping_enable (0x07=3 maps, 0x0F=4 maps with EMB) and EMB registers.
        dual_layout = MemoryLayout("DualSensorPipelineMappingData")
        dual_layout.addItem('primary_mapping_enable', 1)
        dual_layout.addItem('primary_map_dst', 1)
        dual_layout.addItem('primary_dt_in', 1)
        dual_layout.addItem('primary_dt_out', 1)
        dual_layout.addItem('primary_emb_in', 1)
        dual_layout.addItem('primary_emb_out', 1)
        dual_layout.addItem('secondary_mapping_enable', 1)
        dual_layout.addItem('secondary_map_dst', 1)
        dual_layout.addItem('secondary_dt_in', 1)
        dual_layout.addItem('secondary_dt_out', 1)
        dual_layout.addItem('secondary_emb_in', 1)
        dual_layout.addItem('secondary_emb_out', 1)
        dual_layout.addItem('pipe_sel', 1)
        self.dual_pipeline_mapping_block = MemoryBlock(dual_layout, 'dual_pipeline_mapping_block')

    def seq_check_presence(self):
        with Sequence('check_presence'):
            annotate(f'Reading MAX{self.DESER_TYPE} Device ID and Revision ID at address {self.I2C_ADDRESS}')
            with self.max967xx:
                readToMemory(0x000D, self.device_id_data_block.deviceId)    # Device ID register
                readToMemory(0x004C, self.device_id_data_block.revisionId)  # Revision ID register

    def seq_activate_link_reset(self):
        with Sequence('activate_link_reset'):
            annotate('Activate link reset')
            with self.max967xx:
                writeFromMemory(0x0018, self.link_reset_data_block.linkResetData)

    def seq_release_link_reset(self):
        with Sequence('release_link_reset'):
            annotate('Release link reset')
            with self.max967xx:
                write(0x0018, 0x00)

    # GMSL2 6GBPS Link Speed Sequences
    def seq_setup_6gbps_gmsl2_link_a(self):
        with Sequence('setup_gmsl2_6gbps_link_a'):
            annotate('Set GMSL2 6GBPS Link A')
            with self.max967xx:
                writeMasked(0x0006, 0x10, 0x10) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0010, 0x02, 0x03) # GMSL Link Speed 6GBPS

    def seq_setup_6gbps_gmsl2_link_b(self):
        with Sequence('setup_gmsl2_6gbps_link_b'):
            annotate('Set GMSL2 6GBPS Link B')
            with self.max967xx:
                writeMasked(0x0006, 0x20, 0x20) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0010, 0x20, 0x30) # GMSL Link Speed 6GBPS

    def seq_setup_6gbps_gmsl2_link_c(self):
        with Sequence('setup_gmsl2_6gbps_link_c'):
            annotate('Set GMSL2 6GBPS Link C')
            with self.max967xx:
                writeMasked(0x0006, 0x40, 0x40) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0011, 0x02, 0x03) # GMSL Link Speed 6GBPS

    def seq_setup_6gbps_gmsl2_link_d(self):
        with Sequence('setup_gmsl2_6gbps_link_d'):
            annotate('Set GMSL2 6GBPS Link D')
            with self.max967xx:
                writeMasked(0x0006, 0x80, 0x80) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0011, 0x20, 0x30) # GMSL Link Speed 6GBPS

    # GMSL2 3GBPS Link Speed Sequences
    def seq_setup_3gbps_gmsl2_link_a(self):
        with Sequence('setup_gmsl2_3gbps_link_a'):
            annotate('Set GMSL2 3GBPS Link A')
            with self.max967xx:
                writeMasked(0x0006, 0x10, 0x10) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0010, 0x01, 0x03) # GMSL Link Speed 3GBPS

    def seq_setup_3gbps_gmsl2_link_b(self):
        with Sequence('setup_gmsl2_3gbps_link_b'):
            annotate('Set GMSL2 3GBPS Link B')
            with self.max967xx:
                writeMasked(0x0006, 0x20, 0x20) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0010, 0x10, 0x30) # GMSL Link Speed 3GBPS

    def seq_setup_3gbps_gmsl2_link_c(self):
        with Sequence('setup_gmsl2_3gbps_link_c'):
            annotate('Set GMSL2 3GBPS Link C')
            with self.max967xx:
                writeMasked(0x0006, 0x40, 0x40) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0011, 0x01, 0x03) # GMSL Link Speed 3GBPS

    def seq_setup_3gbps_gmsl2_link_d(self):
        with Sequence('setup_gmsl2_3gbps_link_d'):
            annotate('Set GMSL2 3GBPS Link D')
            with self.max967xx:
                writeMasked(0x0006, 0x80, 0x80) # GMSL Mode 2
                delay(5000) # TODO: Remove
                writeMasked(0x0011, 0x10, 0x30) # GMSL Link Speed 3GBPS

    def seq_set_i2c_port0(self):
        with Sequence('set_i2c_port0'):
            annotate('Set I2C Port 0')
            with self.max967xx:
                write(0x0003, 0xAB)
                write(0x0007, 0x00)
                write(0x0003, 0xAA)

                write(0x0640, 0x25)
                write(0x0650, 0x25)
                write(0x0660, 0x25)
                write(0x0670, 0x25)

    def seq_set_i2c_port1(self):
        with Sequence('set_i2c_port1'):
            annotate('Set I2C Port 1')
            with self.max967xx:
                write(0x0003, 0x00)
                write(0x0007, 0xF0)
                write(0x0003, 0x00)

                write(0x0640, 0x25)
                write(0x0650, 0x25)
                write(0x0660, 0x25)
                write(0x0670, 0x25)

    def seq_set_i2c_port(self):
        with Sequence('set_i2c_port'):
            annotate('Set I2C Port (deprecated - use port-specific versions)')
            with self.max967xx:
                write(0x0003, 0xAB)
                write(0x0007, 0x00)
                write(0x0003, 0xAA)

                # TODO: Do we really need as we are going to update in self.seq_set_i2c_mode() ?
                write(0x0640, 0x25)
                write(0x0650, 0x25)
                write(0x0660, 0x25)
                write(0x0670, 0x25)

    def seq_set_i2c_mode(self):
        with Sequence('set_i2c_mode'):
            annotate('Set I2C Mode')
            with self.max967xx:
                write(0x0640, 0x05) # I2C Port 0
                write(0x0641, 0x76)
                write(0x0650, 0x05)
                write(0x0651, 0x76)
                write(0x0660, 0x05)
                write(0x0661, 0x76)
                write(0x0670, 0x05)
                write(0x0671, 0x76)

                write(0x0680, 0x05) # I2C Port 1
                write(0x0681, 0x76)
                write(0x0690, 0x05)
                write(0x0691, 0x76)
                write(0x06A0, 0x05)
                write(0x06A1, 0x76)
                write(0x06B0, 0x05)
                write(0x06B1, 0x76)

    def seq_disable_all_pipelines(self):
        with Sequence('disable_all_pipelines'):
            annotate('Disable all pipelines')
            with self.max967xx:
                writeMasked(0x00F4, 0x00, 0x0F)

    def seq_check_csi_pll_lock_2x4(self):
        with Sequence('check_csi_pll_lock_2x4'):
            annotate('Check CSI PLL Lock 2x4')
            with self.max967xx:
                poll(0x0400, 0x20, 0x20, 10000, 20)

    def seq_check_csi_pll_lock_4x2(self):
        with Sequence('check_csi_pll_lock_4x2'):
            annotate('Check CSI PLL Lock 4x2 (D457: PHY0/1 only -> 2x4 pattern 0x60)')
            with self.max967xx:
                poll(0x0400, 0x20, 0x20, 10000, 20)

    def seq_disable_csi_out(self):
        with Sequence('disable_csi_out'):
            annotate('Disable CSI out')
            with self.max967xx:
                writeMasked(0x040B, 0x0, 0x02)

    def seq_enable_csi_out(self):
        with Sequence('enable_csi_out'):
            annotate('Enable CSI out')
            with self.max967xx:
                writeMasked(0x040B, 0x2, 0x02)

    def seq_check_link_lock_A(self):
        with Sequence('check_link_lock_A'):
            annotate('Check link lock A')
            with self.max967xx:
                poll(0x001A, 0x08, 0x08, 10000, 50) # Link A

    def seq_check_link_lock_B(self):
        with Sequence('check_link_lock_B'):
            annotate('Check link lock B')
            with self.max967xx:
                poll(0x000A, 0x08, 0x08, 10000, 50) # Link B

    def seq_check_link_lock_C(self):
        with Sequence('check_link_lock_C'):
            annotate('Check link lock C')
            with self.max967xx:
                poll(0x000B, 0x08, 0x08, 10000, 50) # Link C

    def seq_check_link_lock_D(self):
        with Sequence('check_link_lock_D'):
            annotate('Check link lock D')
            with self.max967xx:
                poll(0x000C, 0x08, 0x08, 10000, 50) # Link D

    def seq_unset_errb_rx_en_A(self):
        with Sequence('unset_errb_rx_en_A'):
            annotate('Unset ERRB RX EN A')
            with self.max967xx:
                write(0x0030, 0x45)

    def seq_unset_errb_rx_en_B(self):
        with Sequence('unset_errb_rx_en_B'):
            annotate('Unset ERRB RX EN B')
            with self.max967xx:
                write(0x0031, 0x45)

    def seq_unset_errb_rx_en_C(self):
        with Sequence('unset_errb_rx_en_C'):
            annotate('Unset ERRB RX EN C')
            with self.max967xx:
                write(0x0032, 0x45)

    def seq_unset_errb_rx_en_D(self):
        with Sequence('unset_errb_rx_en_D'):
            annotate('Unset ERRB RX EN D')
            with self.max967xx:
                write(0x0033, 0x45)

    def seq_set_errb_rx_en_A(self):
        with Sequence('set_errb_rx_en_A'):
            annotate('Set ERRB RX EN A')
            with self.max967xx:
                write(0x0030, 0xC5)

    def seq_set_errb_rx_en_B(self):
        with Sequence('set_errb_rx_en_B'):
            annotate('Set ERRB RX EN B')
            with self.max967xx:
                write(0x0031, 0xC5)

    def seq_set_errb_rx_en_C(self):
        with Sequence('set_errb_rx_en_C'):
            annotate('Set ERRB RX EN C')
            with self.max967xx:
                write(0x0032, 0xC5)

    def seq_set_errb_rx_en_D(self):
        with Sequence('set_errb_rx_en_D'):
            annotate('Set ERRB RX EN D')
            with self.max967xx:
                write(0x0033, 0xC5)

    def seq_set_ser_video_pipeline_mapping_A(self):
        with Sequence('set_ser_video_pipeline_mapping_A'):
            annotate('Set SER Video Pipeline Mapping for Link A with configurable txPort-based value')
            with self.max967xx:
                # D457 dual-VC: TWO deser pipes from the ONE GMSL stream (both <- stream X), one pipe per VC,
                # so each VC has its own line buffer. A single pipe cannot cleanly interleave 2 VCs (depth+RGB
                # alone are each stable, together -> PIX_SHORT/ICP teardown). pipe0=VC0 depth, pipe1=VC1 RGB.
                write(0x00F0, 0x10)  # pipe-per-VC: deser pipe0<-stream0 (depth), pipe1<-stream1 (RGB)
                write(0x00F4, 0x07)  # VIDEO_PIPE_EN: enable pipes 0,1,2 (depth, RGB, IR)
                # --- pipe 0: VC0 depth (0x2E passthrough) ---
                write(0x090B, 0x07)  # 3 maps: PIX+FS+FE
                write(0x092D, 0x15)  # -> PHY1
                write(0x090D, 0x2e)  # PIX SRC VC0/0x2E
                write(0x090E, 0x2e)  # PIX DST VC0/0x2E
                write(0x090F, 0x00)  # FS SRC VC0
                write(0x0910, 0x00)  # FS DST VC0
                write(0x0911, 0x01)  # FE SRC VC0
                write(0x0912, 0x01)  # FE DST VC0
                write(0x0800, 0x00)  # pipe0 TX_EXT0-3 = vc_msb 0 (VC0<4) — explicit, avoid stale ext-VC
                write(0x0801, 0x00)
                write(0x0802, 0x00)
                write(0x0803, 0x00)
                write(0x0100, 0x23)  # pipe0 VIDEO_RX: DIS_PKT_DET
                # --- pipe 1: VC1 RGB (0x1E -> 0x2E, stays on VC1) ---
                write(0x094B, 0x0F)  # 4 maps (RGB 0x1E + IR 0x2E on VC1)
                write(0x096D, 0x55)  # 4 slots -> PHY1
                write(0x094D, 0x5e)  # PIX SRC VC1/0x1E (0x40|0x1E)
                write(0x094E, 0x6e)  # PIX DST VC1/0x2E
                write(0x094F, 0x40)  # FS SRC VC1
                write(0x0950, 0x40)  # FS DST VC1
                write(0x0951, 0x41)  # FE SRC VC1
                write(0x0952, 0x41)  # FE DST VC1
                write(0x0953, 0x6e)  # slot3 SRC VC1/0x2E (IR)
                write(0x0954, 0x6e)  # slot3 DST VC1/0x2E
                write(0x0810, 0x00)  # pipe1 TX_EXT0-3 = vc_msb 0 (VC1<4) — explicit, avoid stale ext-VC
                write(0x0811, 0x00)
                write(0x0812, 0x00)
                write(0x0813, 0x00)
                write(0x0112, 0x23)  # pipe1 VIDEO_RX: DIS_PKT_DET
                # --- pipe 2: VC2 IR (0x2E passthrough, stays on VC2) --- base = 0x090B + 0x80
                write(0x098B, 0x07)  # 3 maps: PIX+FS+FE
                write(0x09AD, 0x55)  # pipe2 dest-PHY -> PHY1 (0x092D + 0x80)
                write(0x098D, 0xae)  # PIX SRC VC2/0x2E (0x80|0x2E)
                write(0x098E, 0xae)  # PIX DST VC2/0x2E
                write(0x098F, 0x80)  # FS SRC VC2
                write(0x0990, 0x80)  # FS DST VC2
                write(0x0991, 0x81)  # FE SRC VC2
                write(0x0992, 0x81)  # FE DST VC2
                write(0x0820, 0x00)  # pipe2 TX_EXT0-3 = vc_msb 0 (VC2<4)
                write(0x0821, 0x00)
                write(0x0822, 0x00)
                write(0x0823, 0x00)
                write(0x0124, 0x23)  # pipe2 VIDEO_RX: DIS_PKT_DET (0x0100 + 0x12*2)
                # ===== MULTI-CAMERA: link1 (2nd D457) on deser pipes 3,4,5 -> output VC4,5,6 =====
                # Each output VC>=4 uses extended-VC: MAP_DST carries the VC low 2 bits, MIPI_TX_EXT
                # (0x800+0x10*pipe) carries vc_msb<<2. VC4:lsb0/msb1, VC5:lsb1/msb1, VC6:lsb2/msb1.
                # --- pipe 3: link1 depth VC0/0x2E -> out VC4 (base 0x090B+0xC0=0x09CB) ---
                write(0x09CB, 0x07)
                write(0x09ED, 0x55)  # dest-PHY1 (0x092D+0xC0)
                write(0x09CD, 0x2e)  # PIX SRC VC0/0x2E
                write(0x09CE, 0x2e)  # PIX DST VC4 low=0x2E (+TX_EXT msb)
                write(0x09CF, 0x00)  # FS SRC VC0
                write(0x09D0, 0x00)  # FS DST VC4 low
                write(0x09D1, 0x01)  # FE SRC VC0
                write(0x09D2, 0x01)  # FE DST VC4 low
                write(0x0830, 0x04)  # TX_EXT0 pipe3 vc_msb=1 (<<2)
                write(0x0831, 0x04)
                write(0x0832, 0x04)
                write(0x0833, 0x04)
                write(0x0136, 0x23)  # pipe3 VIDEO_RX (0x0100+0x12*3)
                # --- pipe 4: link1 rgb VC1/0x1E -> out VC5 (base 0x0A0B) ---
                write(0x0A0B, 0x07)
                write(0x0A2D, 0x55)
                write(0x0A0D, 0x5e)  # PIX SRC VC1/0x1E
                write(0x0A0E, 0x6e)  # PIX DST VC5 low (0x40|0x2E)
                write(0x0A0F, 0x40)  # FS SRC VC1
                write(0x0A10, 0x40)  # FS DST VC5 low
                write(0x0A11, 0x41)  # FE SRC VC1
                write(0x0A12, 0x41)  # FE DST VC5 low
                write(0x0840, 0x04)
                write(0x0841, 0x04)
                write(0x0842, 0x04)
                write(0x0843, 0x04)
                write(0x0148, 0x23)  # pipe4 VIDEO_RX (0x0100+0x12*4)
                # --- pipe 5: link1 ir VC2/0x2E -> out VC6 (base 0x0A4B) ---
                write(0x0A4B, 0x07)
                write(0x0A6D, 0x55)
                write(0x0A4D, 0xae)  # PIX SRC VC2/0x2E
                write(0x0A4E, 0xae)  # PIX DST VC6 low (0x80|0x2E)
                write(0x0A4F, 0x80)  # FS SRC VC2
                write(0x0A50, 0x80)  # FS DST VC6 low
                write(0x0A51, 0x81)  # FE SRC VC2
                write(0x0A52, 0x81)  # FE DST VC6 low
                write(0x0850, 0x04)
                write(0x0851, 0x04)
                write(0x0852, 0x04)
                write(0x0853, 0x04)
                write(0x0160, 0x23)  # pipe5 VIDEO_RX (0x0100+0x12*5 +6 errata for P>=5 => 0x0160)

                # Video pipeline Selection: pipe<-(link<<2)|ser_pipe. link0 ser0/1/2=0x0/1/2; link1=0x4/5/6.
                write(0x00F0, 0x10)  # pipe0<-(L0,s0)=0x0 ; pipe1<-(L0,s1)=0x1
                write(0x00F1, 0x42)  # pipe2<-(L0,s2)=0x2 ; pipe3<-(L1,s0)=0x4
                write(0x00F2, 0x65)  # pipe4<-(L1,s1)=0x5 ; pipe5<-(L1,s2)=0x6
                write(0x00F4, 0x3F)  # VIDEO_PIPE_EN: pipes 0..5
                writeFromMemory(0x0939, self.video_pipeline_mapping_link_a_block.reg_0939)
                write(0x0018, 0x01)
                delay(100000)
                delay(20000)

    def seq_set_ser_video_pipeline_mapping_B(self):
        with Sequence('set_ser_video_pipeline_mapping_B'):
            # MULTI-CAM: no-op. mapping_A programs ALL deser pipes (0-5, both links) in one place to
            # avoid the two per-link sequences racing on the shared VIDEO_PIPE_SEL/EN regs. Link B's
            # pixel-map (pipes 3-5, VC4/5/6) is set by mapping_A.
            annotate('D457 multi-cam: link B pipes programmed by mapping_A (no-op here)')

    def seq_set_ser_video_pipeline_mapping_C(self):
        with Sequence('set_ser_video_pipeline_mapping_C'):
            annotate('D457 multi-cam: link C pipes programmed by mapping_A (no-op here)')

    def seq_set_ser_video_pipeline_mapping_D(self):
        with Sequence('set_ser_video_pipeline_mapping_D'):
            annotate('D457 multi-cam: link D pipes programmed by mapping_A (no-op here)')

    # --------------------------------------------------------------------------
    # Dual-sensor (HAWK-style) video pipeline mapping sequences
    # --------------------------------------------------------------------------
    # Register layout per pipe: base = 0x0900 + pipe_index * 0x40
    #   MAP_EN  = base + 0x0B    MAP_DST = base + 0x2D
    #   DT_IN   = base + 0x0D    DT_OUT  = base + 0x0E
    #   FS_IN   = base + 0x0F    FE_IN   = base + 0x10
    #   FS_OUT  = base + 0x11    FE_OUT  = base + 0x12
    # PIPE_SEL register = 0x00F0 + link_index
    # --------------------------------------------------------------------------

    _LINK_NAMES = ['A', 'B', 'C', 'D']

    def _seq_set_dual_sensor_video_pipeline_mapping(self, link_idx):
        first_pipe_idx = link_idx * 2       # 0, 2, 4, 6
        second_pipe_idx = link_idx * 2 + 1  # 1, 3, 5, 7
        base_reg = 0x0900
        primary_base   = base_reg + first_pipe_idx * 0x40
        secondary_base = base_reg + second_pipe_idx * 0x40
        pipe_sel_reg   = 0x00F0 + link_idx
        name = self._LINK_NAMES[link_idx]
        mapping_block = self.dual_pipeline_mapping_block

        with Sequence(f'set_dual_sensor_video_pipeline_mapping_{name}'):
            annotate(f'Set dual-sensor pipeline mapping for Link {name} '
                     f'(pipes {first_pipe_idx} and {second_pipe_idx})')
            with self.max967xx:
                writeFromMemory(primary_base + 0x0B, mapping_block.primary_mapping_enable)
                writeFromMemory(primary_base + 0x2D, mapping_block.primary_map_dst)
                writeFromMemory(primary_base + 0x0D, mapping_block.primary_dt_in)
                writeFromMemory(primary_base + 0x0E, mapping_block.primary_dt_out)
                write(primary_base + 0x0F, 0x00)
                write(primary_base + 0x10, 0x00)
                write(primary_base + 0x11, 0x01)
                write(primary_base + 0x12, 0x01)
                writeFromMemory(primary_base + 0x13, mapping_block.primary_emb_in)
                writeFromMemory(primary_base + 0x14, mapping_block.primary_emb_out)

                writeFromMemory(secondary_base + 0x0B, mapping_block.secondary_mapping_enable)
                writeFromMemory(secondary_base + 0x2D, mapping_block.secondary_map_dst)
                writeFromMemory(secondary_base + 0x0D, mapping_block.secondary_dt_in)
                writeFromMemory(secondary_base + 0x0E, mapping_block.secondary_dt_out)
                write(secondary_base + 0x0F, 0x00)
                write(secondary_base + 0x10, 0x40)
                write(secondary_base + 0x11, 0x01)
                write(secondary_base + 0x12, 0x41)
                writeFromMemory(secondary_base + 0x13, mapping_block.secondary_emb_in)
                writeFromMemory(secondary_base + 0x14, mapping_block.secondary_emb_out)

                writeFromMemory(pipe_sel_reg, mapping_block.pipe_sel)
                delay(20000)

    def seq_video_pipeline_enable(self):
        with Sequence('video_pipeline_enable'):
            annotate('Enable Video Pipeline')
            with self.max967xx:
                writeFromMemory(0x00F4, self.link_index_data_block.linkIndex)
                delay(20000)

    def seq_enable_double_pixel_mode(self):
        with Sequence('enable_double_pixel_mode'):
            annotate('Enable Double Pixel Mode')
            with self.max967xx:
                write(0x0933, 0x01)
                write(0x0973, 0x01)
                write(0x09B3, 0x01)
                write(0x09F3, 0x01)

    # Undouble 8-bit Embedded Data type
    def seq_set_undouble_8bit_emb_data(self):
        with Sequence('set_undouble_8bit_emb_data'):
            annotate('Undouble 8-bit Embedded Data, if embedded data type is enabled')
            with self.max967xx:
                writeMasked(0x0933, 0x10, 0x10) # Un-double 8-bit data in controller 0
                writeMasked(0x0973, 0x10, 0x10) # Un-double 8-bit data in controller 1
                writeMasked(0x09B3, 0x10, 0x10) # Un-double 8-bit data in controller 2
                writeMasked(0x09F3, 0x10, 0x10) # Un-double 8-bit data in controller 3

    def seq_enable_double_pixel_mode_ext_A(self):
        with Sequence('enable_double_pixel_mode_ext_A'):
            annotate('Enable Double Pixel Mode Ext A')
            with self.max967xx:
                write(0x0100, 0x22)
                write(0x0106, 0x02)

    def seq_enable_double_pixel_mode_ext_B(self):
        with Sequence('enable_double_pixel_mode_ext_B'):
            annotate('Enable Double Pixel Mode Ext B')
            with self.max967xx:
                write(0x0112, 0x22)
                write(0x0118, 0x02)

    def seq_enable_double_pixel_mode_ext_C(self):
        with Sequence('enable_double_pixel_mode_ext_C'):
            annotate('Enable Double Pixel Mode Ext C')
            with self.max967xx:
                write(0x0124, 0x22)
                write(0x012A, 0x02)

    def seq_enable_double_pixel_mode_ext_D(self):
        with Sequence('enable_double_pixel_mode_ext_D'):
            annotate('Enable Double Pixel Mode Ext D')
            with self.max967xx:
                write(0x0136, 0x22)
                write(0x013C, 0x02)

    def seq_set_serdes_gpio_forward(self):
        with Sequence('set_serdes_gpio_forward'):
            annotate('Set SERDES GPIO Forward')
            with self.max967xx:
                # TODO: Set SERDES GPIO Forward
                pass

    # --- Abstract: device-specific FSYNC sequences (register addresses differ per device) ---
    @abstractmethod
    def seq_set_gmsl2_external_fsync_link_A(self):
        ...

    @abstractmethod
    def seq_set_gmsl2_external_fsync_link_B(self):
        ...

    @abstractmethod
    def seq_set_gmsl2_external_fsync_link_C(self):
        ...

    @abstractmethod
    def seq_set_gmsl2_external_fsync_link_D(self):
        ...

    def enable_link_A(self):
        with Sequence('enable_link_A'):
            annotate('Enable link A')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x01, 0x01) # Link A
                poll(0x001A, 0x08, 0x08, 10000, 50) # Link A
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def enable_link_B(self):
        with Sequence('enable_link_B'):
            annotate('Enable link B')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x02, 0x02) # Link B
                poll(0x000A, 0x08, 0x08, 10000, 50) # Link B
                writeMasked(0x0005, 0x80, 0x80) # EnableLOCK error

    def enable_link_C(self):
        with Sequence('enable_link_C'):
            annotate('Enable link C')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x04, 0x04) # Link C
                poll(0x000B, 0x08, 0x08, 10000, 50) # Link C
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def enable_link_D(self):
        with Sequence('enable_link_D'):
            annotate('Enable link D')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x08, 0x08) # Link D
                poll(0x000C, 0x08, 0x08, 10000, 50) # Link D
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def disable_link_A(self):
        with Sequence('disable_link_A'):
            annotate('Disable link A')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x00, 0x01) # Link A
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def disable_link_B(self):
        with Sequence('disable_link_B'):
            annotate('Disable link B')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x00, 0x02) # Link B
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def disable_link_C(self):
        with Sequence('disable_link_C'):
            annotate('Disable link C')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x00, 0x04) # Link C
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def disable_link_D(self):
        with Sequence('disable_link_D'):
            annotate('Disable link D')
            with self.max967xx:
                writeMasked(0x0005, 0x00, 0x80) # Disable LOCK error
                writeMasked(0x0006, 0x00, 0x08) # Link D
                writeMasked(0x0005, 0x80, 0x80) # Enable LOCK error

    def reset_links(self):
        with Sequence('reset_links'):
            annotate('Reset links')
            with self.max967xx:
                write(0x0006, 0xF0)

    def seq_read_errb(self):
        with Sequence('read_errb'):
            annotate('Read ERRB')
            with self.max967xx:
                annotate('Read ERRB error')
                readToMemory(0x001A, self.errb_data_block.ERRBData)

    def seq_read_mipi_phy_registers(self):
        with Sequence('read_mipi_phy_registers'):
            annotate('Read MIPI PHY registers for read-modify-write')
            with self.max967xx:
                # Read current PHY register values to preserve bits 6-7
                readToMemory(0x0415, self.mipi_phy_datarate_config_block.reg_0415_current)
                readToMemory(0x0418, self.mipi_phy_datarate_config_block.reg_0418_current)
                readToMemory(0x041B, self.mipi_phy_datarate_config_block.reg_041B_current)
                readToMemory(0x041E, self.mipi_phy_datarate_config_block.reg_041E_current)

    def seq_set_mipi_c_phy(self):
        with Sequence('set_mipi_c_phy'):
            annotate('Set MIPI C PHY')
            with self.max967xx:
                #TODO: to make it configurable
                write(0x08A2, 0x04)
                write(0x08AD, 0x13)
                write(0x08AE, 0x7C)

                write(0x08A2, 0xF4)

                write(0x090A, 0xE0)
                write(0x094A, 0xE0)

                write(0x098A, 0xE0)
                write(0x09CA, 0xE0)

                write(0x08A3, 0xE4)
                write(0x08A4, 0xE4)

                # Write MIPI speed configuration to PHY registers
                writeFromMemory(0x0415, self.mipi_phy_datarate_config_block.reg_0415_value)
                writeFromMemory(0x0418, self.mipi_phy_datarate_config_block.reg_0418_value)
                writeFromMemory(0x041B, self.mipi_phy_datarate_config_block.reg_041B_value)
                writeFromMemory(0x041E, self.mipi_phy_datarate_config_block.reg_041E_value)

    def seq_set_mipi_d_phy(self):
        with Sequence('set_mipi_d_phy'):
            annotate('D457 2x4-2lane d4xx-exact')
            with self.max967xx:
                write(0x08A0, 0x24)
                write(0x094A, 0x50)
                write(0x08A3, 0xE4)
                write(0x0973, 0x10)
                write(0x1C00, 0xF4)
                write(0x1D00, 0xF4)
                write(0x0418, 0x39)
                write(0x1C00, 0xF5)
                write(0x1D00, 0xF5)
                write(0x08A2, 0x34)

    def seq_setup_initial_deskew_all_controllers(self):
        """Setup initial deskew for all controllers (DPHY mode with MIPI speed >= 1.5GHz)"""
        with Sequence('setup_initial_deskew_all_controllers'):
            annotate('Setup initial deskew for all Controllers (PHY0-PHY3)')
            with self.max967xx:
                write(0x0903, 0x97)  # Controller 0
                write(0x0943, 0x97)  # Controller 1
                write(0x0983, 0x97)  # Controller 2
                write(0x09C3, 0x97)  # Controller 3

    def seq_trigger_deskew_all_controllers(self):
        """Trigger deskew calibration for all controllers (called during DoStart)"""
        with Sequence('trigger_deskew_all_controllers'):
            annotate('Trigger deskew for all Controllers (PHY0-PHY3)')
            with self.max967xx:
                # First iteration - set bit 5 for all controllers
                writeMasked(0x0903, 0x20, 0x20)  # Controller 0
                writeMasked(0x0943, 0x20, 0x20)  # Controller 1
                writeMasked(0x0983, 0x20, 0x20)  # Controller 2
                writeMasked(0x09C3, 0x20, 0x20)  # Controller 3
                delay(10000)  # 10ms delay
                # Second iteration - clear bit 5 for all controllers
                writeMasked(0x0903, 0x00, 0x20)  # Controller 0
                writeMasked(0x0943, 0x00, 0x20)  # Controller 1
                writeMasked(0x0983, 0x00, 0x20)  # Controller 2
                writeMasked(0x09C3, 0x00, 0x20)  # Controller 3

    def compile(self):
        self.seq_check_presence()
        self.seq_activate_link_reset()
        self.seq_release_link_reset()
        self.seq_setup_6gbps_gmsl2_link_a()
        self.seq_setup_6gbps_gmsl2_link_b()
        self.seq_setup_6gbps_gmsl2_link_c()
        self.seq_setup_6gbps_gmsl2_link_d()
        self.seq_setup_3gbps_gmsl2_link_a()
        self.seq_setup_3gbps_gmsl2_link_b()
        self.seq_setup_3gbps_gmsl2_link_c()
        self.seq_setup_3gbps_gmsl2_link_d()
        self.seq_set_i2c_mode()
        self.seq_set_i2c_port0()
        self.seq_set_i2c_port1()
        self.seq_set_i2c_port()
        self.seq_disable_all_pipelines()
        self.seq_check_csi_pll_lock_2x4()
        self.seq_check_csi_pll_lock_4x2()
        self.seq_enable_csi_out()
        self.seq_disable_csi_out()
        self.seq_check_link_lock_A()
        self.seq_check_link_lock_B()
        self.seq_check_link_lock_C()
        self.seq_check_link_lock_D()
        self.seq_set_errb_rx_en_A()
        self.seq_set_errb_rx_en_B()
        self.seq_set_errb_rx_en_C()
        self.seq_set_errb_rx_en_D()
        self.seq_unset_errb_rx_en_A()
        self.seq_unset_errb_rx_en_B()
        self.seq_unset_errb_rx_en_C()
        self.seq_unset_errb_rx_en_D()
        self.seq_set_ser_video_pipeline_mapping_A()
        self.seq_set_ser_video_pipeline_mapping_B()
        self.seq_set_ser_video_pipeline_mapping_C()
        self.seq_set_ser_video_pipeline_mapping_D()
        for link_idx in range(4):
            self._seq_set_dual_sensor_video_pipeline_mapping(link_idx)
        self.seq_set_undouble_8bit_emb_data()
        self.seq_video_pipeline_enable()
        self.seq_enable_double_pixel_mode()
        self.seq_enable_double_pixel_mode_ext_A()
        self.seq_enable_double_pixel_mode_ext_B()
        self.seq_enable_double_pixel_mode_ext_C()
        self.seq_enable_double_pixel_mode_ext_D()
        self.seq_set_serdes_gpio_forward()
        self.seq_set_gmsl2_external_fsync_link_A()
        self.seq_set_gmsl2_external_fsync_link_B()
        self.seq_set_gmsl2_external_fsync_link_C()
        self.seq_set_gmsl2_external_fsync_link_D()
        self.enable_link_A()
        self.enable_link_B()
        self.enable_link_C()
        self.enable_link_D()
        self.disable_link_A()
        self.disable_link_B()
        self.disable_link_C()
        self.disable_link_D()
        self.reset_links()
        self.seq_read_errb()
        self.seq_read_mipi_phy_registers()
        self.seq_set_mipi_c_phy()
        self.seq_set_mipi_d_phy()
        self.seq_setup_initial_deskew_all_controllers()
        self.seq_trigger_deskew_all_controllers()

        ph_logger().info(f'Common MAX967xx HSL sequences compiled successfully')
