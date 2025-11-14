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

from pyhsl import *

def define_sample_coe_driver_sequences():
    """
    Defines HSL sequences for the Sample CoE Driver.
    """
    logger = ph_logger()
    logger.info(f'Defining Sample CoE Driver HSL sequences. Compilation parameters: {ph_params()}')

    # Define I2C Devices for a generic CoE Module
    # Control interface for CoE module, 16-bit offset, 8-bit data
    coe_module_ctrl_i2c = I2CDevice(0x7A, 16, 8, 'coe_module_ctrl_i2c')
    # Data interface for CoE module, 8-bit offset, 16-bit data, with auto-retry
    coe_module_data_i2c = I2CDevice(0x7B, 8, 16, 'coe_module_data_i2c', auto_retry=True)

    # Define GPIO Pins for a generic CoE Module
    coe_module_enable_gpio = GPIOPin(encodeGPIOPinAddress(1, 0, 5), name='coe_module_enable_gpio') # Example: ctrl=1, bank=0, pin=5
    coe_module_reset_gpio = GPIOPin(encodeGPIOPinAddress(1, 0, 6), name='coe_module_reset_gpio')   # Example: ctrl=1, bank=0, pin=6

    # --- Init Sequence for Sample CoE Driver ---
    with ph_sequence('Init') as seq:
        seq.annotate('Starting Sample CoE Driver Initialization')

        # Power on and basic setup for CoE Module
        coe_module_enable_gpio.write(True)
        seq.delay(120_000) # 120 ms delay

        with coe_module_ctrl_i2c:
            write(0x0100, 0xC1) # Write to main control register
            write(0x0102, 0xE3, noverify=True) # Write to secondary control register without verification
            # Poll a status register on the CoE module
            poll(0x01F0, expectedValue=0x01, mask=0x01, intervalInUsec=15_000, retries=6)

        with coe_module_data_i2c:
            # Stream write configuration parameters - provide as a list of bytes
            # For 16-bit data, each word is split into MSB and LSB (big-endian)
            write(0x20, [0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6])

        seq.annotate('Sample CoE Driver Initialization Complete')

    # --- Reset Sequence for Sample CoE Driver ---
    with ph_sequence('Reset') as seq:
        seq.annotate('Starting Sample CoE Driver Reset')

        # Assert reset pin for CoE Module
        coe_module_reset_gpio.write(True)
        seq.delay(60_000) # 60 ms delay
        coe_module_reset_gpio.write(False)
        seq.delay(120_000) # 120 ms delay for stabilization

        with coe_module_ctrl_i2c:
            # Masked write to a configuration register on the CoE module
            writeMasked(0x0100, data=0x70, mask=0xF0, reset_value=0xC1) # Modify upper nibble, keep lower

        with coe_module_data_i2c:
            # Read and verify a specific value (e.g., chip ID or default register)
            readVerify(0x00, expectedValue=0xBEEF, mask=0xFFFF)

        seq.annotate('Sample CoE Driver Reset Complete')

    # --- Deinitialize Sequence for Sample CoE Driver ---
    with ph_sequence('Deinit') as seq:
        seq.annotate('Starting Sample CoE Driver Deinitialization')

        with coe_module_ctrl_i2c:
            # Write to put CoE module in standby or low power mode
            write(0x010A, 0x00)

        # Read and discard a register from CoE module data interface
        coe_module_data_i2c.readDiscard(0xFF)

        # Power off CoE Module
        coe_module_enable_gpio.write(False)
        seq.delay(15_000) # 15 ms delay

        seq.annotate('Sample CoE Driver Deinitialization Complete')

    logger.info("Sample CoE Driver HSL sequences defined:")
    for s_name in Sequence.s_sequences:
        logger.info(f"  {s_name}")

def main():
    """Main function to define HSL sequences and configure pyhsl."""

    # Setup basic logging for pyhsl
    logging.basicConfig(level=logging.DEBUG) # Show debug messages from pyhsl
    pyhsl_logger = logging.getLogger("pyhsl")
    pyhsl_logger.setLevel(logging.INFO) # Set pyhsl's own logger to INFO or DEBUG

    define_sample_coe_driver_sequences()
    print("Successfully defined Sample CoE Driver HSL sequences.")

if __name__ == "__main__":
    main()
