import time
import logging

def write_clnx_spi(data, addr, hololink):
    cmd = (0x01 << 24) + (data << 16) + (addr << 8) + 0x01
    hololink.write_uint32(0x03000008, 0x0000002F)
    hololink.write_uint32(0x03000004, 0x00000003)
    hololink.write_uint32(0x03000010, cmd)
    hololink.write_uint32(0x03000000, 0x00000001)
    time.sleep(0.1)

def write_lmmi_register(camera_idx, data, addr, hololink):
    time.sleep(0.1)
    write_clnx_spi(data, 0x0D, hololink)
    write_clnx_spi(addr, 0x0F, hololink)
    write_clnx_spi((camera_idx << 4) + 0x1, 0x0C, hololink)

def reconfigurable_mipi(camera_idx, lane_prog, clock_lane_settle_cycle, data_lane_settle_cycle, hololink):
    logging.debug("Config MIPI with: lane_prog=%d, clock_lane=%d, data_lane=%d",
                  lane_prog, clock_lane_settle_cycle, data_lane_settle_cycle)

    reg0A = ((clock_lane_settle_cycle & 0x1) << 3) + (lane_prog << 1)
    reg0B = clock_lane_settle_cycle >> 1
    reg0C = (clock_lane_settle_cycle >> 2) & 0x80
    reg0F = (data_lane_settle_cycle & 0x3) << 2
    reg10 = (data_lane_settle_cycle >> 2) & 0xF

    write_lmmi_register(camera_idx, reg0A, 0x0A, hololink)
    write_lmmi_register(camera_idx, reg0B, 0x0B, hololink)
    write_lmmi_register(camera_idx, reg0C, 0x0C, hololink)
    write_lmmi_register(camera_idx, reg0F, 0x0F, hololink)
    write_lmmi_register(camera_idx, reg10, 0x10, hololink)

def program_mipi_phy(camera_idx, num_lanes, clock_freq, hololink):
    lane_prog = num_lanes - 1
    if clock_freq >= 1000:
        data_lane_settle_cycle = 6
    elif clock_freq >= 350:
        data_lane_settle_cycle = 7
    elif clock_freq >= 200:
        data_lane_settle_cycle = 8
    elif clock_freq >= 150:
        data_lane_settle_cycle = 9
    else:
        data_lane_settle_cycle = 11

    clock_lane_settle_cycle = 9

    reconfigurable_mipi(
        camera_idx, lane_prog, clock_lane_settle_cycle, data_lane_settle_cycle, hololink
    )
