<!--
SPDX-FileCopyrightText: Copyright (c) 2025
NVIDIA CORPORATION & AFFILIATES. All rights
reserved.
SPDX-License-Identifier: LicenseRef-NvidiaProprietary

NVIDIA CORPORATION, its affiliates and licensors
retain all intellectual property and proprietary
rights in and to this material, related
documentation and any modifications thereto. Any
use, reproduction, disclosure or distribution of
this material and related documentation without an
express license agreement from NVIDIA CORPORATION
or its affiliates is strictly prohibited.
-->

# NVSIPL CoE Camera Sample

This sample application demonstrates how to use the NVIDIA SIPL Camera API to capture and process image data from Camera over Ethernet (CoE) cameras. It provides a comprehensive example of:

1. Camera system configuration using SIPL Query API
2. Multi-threaded image processing pipeline
3. Frame capture and output (RAW/YUV) to files
4. Event handling and error management
5. Buffer and sync object management
6. Auto control plugin integration

## Features

- **Multi-camera support**: Handle multiple CoE cameras simultaneously
- **Multiple output formats**: Capture RAW (ICP), ISP0, ISP1, and ISP2 outputs
- **File dumping**: Save captured frames to disk with configurable prefixes
- **Comprehensive logging**: Detailed debug, info, warning, and error messages
- **Event monitoring**: Real-time pipeline event notifications
- **Thread-safe operations**: Separate threads for image processing and event handling
- **NITO file support**: Auto control plugin integration with custom folder paths

## Requirements

- NVIDIA SIPL libraries (libnvsipl_camera, libnvsipl_query)
- NvSci libraries (libnvscibuf, libnvscisync)
- Network interface configured for CoE cameras
- Standard C++17 compiler with exception and RTTI support
- Camera configuration files or built-in database entries

## Code Structure

The application is organized into several key components:

### Core Classes

#### `SIPLCoeCamera`
Main test class that orchestrates the entire camera pipeline:
- Manages camera initialization and configuration
- Handles thread creation and synchronization
- Provides buffer and sync object management
- Implements event handling and error management

#### `CoeConsumerBase` and Derived Classes
Consumer framework for processing different output types:
- `CoeRawConsumer`: Handles ICP (raw) output processing
- `CoeYuvConsumer`: Handles ISP0/ISP1/ISP2 (YUV) output processing
- `CoeConsumerFactory`: Creates appropriate consumers based on thread type

#### `CoeBufObjManager`
Buffer object management for NvSci buffer allocation and registration:
- Allocates buffer objects for different output types
- Manages buffer lifecycle and cleanup
- Interfaces with SIPL camera for buffer registration

#### `CSyncObjManager`
Synchronization object management for frame synchronization:
- Allocates NvSciSync objects for different client types
- Manages sync object lifecycle
- Provides CPU wait context for fence operations

### Utility Classes

#### `CCmdLineParser`
Command-line argument parsing with comprehensive options support

#### `CLogger`
Centralized logging system with configurable levels and formatting

#### `CUtils`
Utility functions for NITO file loading, event name mapping, and system operations

### Source Files

- **`main.cpp`**: Main application entry point and test orchestration
- **`SIPLCoeCamera.hpp`**: Main test class definition and interfaces
- **`CCmdLineParser.hpp`**: Command-line argument parsing
- **`CSyncObjManager.hpp`**: Sync object management
- **`CUtils.hpp/.cpp`**: Utility functions and logging system
- **`Makefile.tmk`**: Build configuration

## Application Flow

### 1. Initialization Phase
```
main() -> SetUp() -> SetupCoeCamera() -> GetCoeSystemConfigFromQuery()
```
- Parse command-line arguments
- Initialize NvSci modules (buffer and sync)
- Create SIPL Query instance
- Load camera system configuration

### 2. Configuration Phase
```
CoeSetupAndInit() -> CoeSetPlatformCfg() -> CoeSetPipelineCfg() -> CoeInit()
```
- Configure platform with camera system config
- Set up pipeline configuration (ICP, ISP0/1/2 outputs)
- Initialize camera instance

### 3. Registration Phase
```
CoeRegisterImages() -> CoeRegisterAutoControlPlugin()
```
- Register buffer objects for each output type
- Load and register NITO files for auto control
- Allocate and register sync objects

### 4. Thread Configuration
```
CoeConfigAllThreads() -> Thread creation for each module/output
```
- Create image processing threads (ICP, ISP0, ISP1, ISP2)
- Create event notification threads
- Create CPU signaling threads

### 5. Streaming Phase
```
Start() -> Thread processing -> Stop()
```
- Start camera streaming
- Process frames in parallel threads
- Handle events and notifications
- Save frames to files (if enabled)

### 6. Cleanup Phase
```
CoeDeinit() -> TearDown()
```
- Stop all threads
- Release buffer and sync objects
- Clean up NvSci modules

## Thread Architecture

The application uses a multi-threaded architecture with separate threads for different functions:

### Image Processing Threads (per module)
- **ICP Thread**: Processes raw capture data
- **ISP0 Thread**: Processes ISP0 output (typically YUV)
- **ISP1 Thread**: Processes ISP1 output
- **ISP2 Thread**: Processes ISP2 output

### Control Threads (per module)
- **Event Thread**: Handles pipeline notifications and events
- **CPU Signal Thread**: Manages CPU-based signaling operations

## Usage

### Basic Usage
```bash
# Run with default settings (5 seconds, no file output)
./nvsipl_coe_camera -c VB1940_Camera

# Run with custom duration
./nvsipl_coe_camera -c VB1940_Camera -r 10

# Enable RAW output capture
./nvsipl_coe_camera -c VB1940_Camera -R -W 10 -f test_capture

# Enable multiple outputs with custom NITO path
./nvsipl_coe_camera -c VB1940_Camera -R -0 -1 -W 5 -f capture -N /custom/nito/path

# Use JSON configuration file
./nvsipl_coe_camera -t custom_config.json -0 -W 3 -v 2

# Use CoE override file for network configuration
./nvsipl_coe_camera -c VB1940_Camera --coeConfigOverridePath override.csv -R -W 5
```

## Command Line Options

| Option | Long Form | Argument | Description |
|--------|-----------|----------|-------------|
| `-h` | `--help` | None | Display usage information and available configurations |
| `-r` | `--runfor` | `<seconds>` | Exit application after n seconds (default: 5) |
| `-c` | `--platform-config` | `<name>` | Platform configuration name |
| `-t` | `--test-config-file` | `<file>` | Custom platform config JSON file |
| `-R` | `--enableRawOutput` | None | Enable RAW (ICP) output capture |
| `-0` | `--disableISP0Output` | None | disable ISP0 output capture |
| `-1` | `--disableISP1Output` | None | disable ISP1 output capture |
| `-2` | `--disableISP2Output` | None | disable ISP2 output capture |
| `-W` | `--writeFrames` | `<count>` | Number of frames to write to file (default: 0) |
| `-f` | `--filedump-prefix` | `<prefix>` | Filename prefix for dumped files |
| `-v` | `--verbosity` | `<level>` | Verbosity level (0-4, default: 1) |
| `-N` | `--nito` | `<folder>` | Path to folder containing NITO files |
| | `--coeConfigOverridePath` | `<file>` | Path to CoE config override file |

### Configuration Sources

The application accepts configuration from two sources:

#### 1. Built-in Platform Configurations (`-c` option)
Use predefined platform configurations from the SIPL database:
```bash
./nvsipl_coe_camera -c VB1940_Camera
```

To see available configurations:
```bash
./nvsipl_coe_camera -h
```

#### 2. JSON Configuration Files (`-t` option)
Use custom JSON configuration files:
```bash
./nvsipl_coe_camera -t /path/to/config.json
```

### CoE Override File

The application supports network configuration overrides for Camera over Ethernet (CoE) cameras through a CSV override file. This allows you to modify network interface settings, MAC addresses, and IP addresses without changing the configuration in database.

#### Usage
```bash
./nvsipl_coe_camera -c VB1940_Camera --coeConfigOverridePath /path/to/override.csv
```

#### CSV File Format
The override file uses CSV format with the following columns:
```
HSB_Id,id_number,Interface_Name,MAC_Address,IP_Address
```

Where:
- **HSB_id**: Holoscan sensor bridge id
- **Interface_Name**: Network interface name (e.g., mgbe0_0, mgbe0_1 for MGBE interfaces)
- **MAC_Address**: MAC address in format `XX:XX:XX:XX:XX:XX`
- **IP_Address**: IP address in dotted decimal format `XXX.XXX.XXX.XXX`

#### Example Override File
```csv
# hsb_id, id,Interface_Name,MAC_Address,IP_Address
hsb_id, 0,mgbe0_0,8c:1f:64:6d:70:03,192.168.1.2
```

#### Features
- **Comment support**: Lines starting with `#` are treated as comments
- **Empty line handling**: Empty lines are automatically skipped
- **Multiple overrides**: Support for multiple camera overrides in a single file
- **Validation**: MAC address and IP address format validation
- **Flexible MAC format**: Supports colon (`:`) separated MAC addresses
- **MGBE interface support**: Compatible with Multi-Gigabit Ethernet interfaces (mgbe0_0, mgbe0_1, etc.)

#### Error Handling
The application will log detailed information about override loading:
- File opening errors
- CSV parsing errors
- Invalid MAC address formats
- Invalid IP address formats
- Applied overrides with before/after values

## Output Files

When frame dumping is enabled (`-W` > 0), the application saves captured frames with the following naming convention:

### RAW Output Files (ICP)
```
<prefix>_sensor<N>_raw_frame_<X>.raw
```

### ISP Output Files
```
<prefix>_sensor<N>_ISP<Y>_frame_<X>.yuv
```

Where:
- `<prefix>`: User-specified prefix (`-f` option)
- `<N>`: Sensor/camera module number
- `<Y>`: ISP output number (0, 1, or 2)
- `<X>`: Frame number

### Example Output Files
```bash
# With: -f capture -W 3 -R -0
capture_sensor0_raw_frame_1.raw
capture_sensor0_raw_frame_2.raw
capture_sensor0_raw_frame_3.raw
capture_sensor0_ISP0_frame_1.yuv
capture_sensor0_ISP0_frame_2.yuv
capture_sensor0_ISP0_frame_3.yuv
```

## Sample Output

### Successful Execution
```
ubuntu@jetson:~$ ./nvsipl_coe_camera -c VB1940_Camera -r 3 -R -W 2 -f test
=== nvsipl_coe_camera ===
nvsipl_coe_camera: Running for 3 seconds
nvsipl_coe_camera: Raw output: enabled
nvsipl_coe_camera: ISP0 output: disabled
nvsipl_coe_camera: ISP1 output: disabled
nvsipl_coe_camera: ISP2 output: disabled
nvsipl_coe_camera: File dump prefix: test
nvsipl_coe_camera: Number of frames to write to file: 2

Camera[0]: Name: VB1940_Camera, Platform: Vb1940, Sensor ID: 0, Sensor Name: VB1940
Resolution: 2560x1984, Embedded lines: 1 top, 2 bottom, FPS: 30
Input Format: 8, Pixel Order (CFA): 0x24 (36)

=== coe setup and init ===
CoeSetPlatformCfg() completed
CoeSetPipelineCfg() completed
CoeInit() completed

=== image registration ===
Registering 4 images for sensor 0
RegisterImages completed in 45 ms

=== auto control plugin registration ===
NITO search path: "/var/nvidia/nvcam/settings/sipl/"
Module name lowercase: "vb1940"
Opened NITO file: "/var/nvidia/nvcam/settings/sipl/vb1940.nito" for module: "VB1940"
Auto control plugin registered successfully for sensor: 0

=== start streaming ===
Running COE streaming for 3 seconds...
Pipeline: 0, NOTIF_INFO_ICP_PROCESSING_DONE
[RAW_CONSUMER] Processing ICP buffer (frame 1, sensor 0)
Saved frame 1 -> test_sensor0_raw_frame_1.raw (6867072 bytes) [1/2]
Pipeline: 0, NOTIF_INFO_ICP_PROCESSING_DONE
[RAW_CONSUMER] Processing ICP buffer (frame 2, sensor 0)
Saved frame 2 -> test_sensor0_raw_frame_2.raw (6867072 bytes) [2/2]

=== stop streaming ===
COE Image Thread 0 (RAW consumer) for module 0 exited - processed 2 total frames

=== coe camera test completed ===
```

### CoE Override File Processing
```bash
ubuntu@jetson:~$ ./nvsipl_coe_camera -c VB1940_Camera --coeConfigOverridePath override.csv -r 3 -R -W 1
=== nvsipl_coe_camera ===
nvsipl_coe_camera: Running for 3 seconds
nvsipl_coe_camera: Raw output: enabled
nvsipl_coe_camera: CoE override path: override.csv

nvsipl_coe_camera: Opened CoE override file: "/home/ubuntu/coe_config_override.csv"
nvsipl_coe_camera: Loaded CoE override: HSB=hsb_id, HSB id=0, Interface=mgbe0_0
nvsipl_coe_camera: MAC: 8c:1f:64:6d:70:03
nvsipl_coe_camera: IP: 192.168.1.2
nvsipl_coe_camera: Loaded 1 CoE override entries from file: /home/ubuntu/coe_config_override.csv.file
nvsipl_coe_camera: CoE override file loaded: /home/ubuntu/ankurp/coe_config_override.csv
nvsipl_coe_camera: CoE Override MAC: 8c:1f:64:6d:70:03
nvsipl_coe_camera: CoE Override IP: 192.168.1.2
nvsipl_coe_camera: CoE Override: HSB=hsb_id, Interface=mgbe0_0
nvsipl_coe_camera: Applying 1 CoE overrides to camera system configuration
nvsipl_coe_camera: Applied interface override: HsbTransport -> mgbe0_0
nvsipl_coe_camera: Applied IP override: 192.168.1.2
nvsipl_coe_camera: CoE overrides applied successfully

...
```

### Error Scenarios
```bash
# Invalid configuration name
ubuntu@jetson:~$ ./nvsipl_coe_camera -c InvalidConfig
ERROR: GetCameraConfig failed for InvalidConfig : 2

# Missing JSON file
ubuntu@jetson:~$ ./nvsipl_coe_camera -t missing.json
ERROR: ParseJsonFile failed

# Invalid override file
ubuntu@jetson:~$ ./nvsipl_coe_camera -c VB1940_Camera --coeConfigOverridePath missing.csv
ERROR: File "missing.csv" not found
```

## Verbosity Levels

The application supports 5 verbosity levels:

| Level | Description | Output |
|-------|-------------|---------|
| 0 | No logging | none |
| 1 | Error only | Error messages (default) |
| 2 | Warning | Error and warning messages |
| 3 | Info | Error, warning, and info messages |
| 4 | Debug | All messages including detailed debug info |

Example with debug output:
```bash
./nvsipl_coe_camera -c VB1940_Camera -v 4
```

## Error Handling

The application includes comprehensive error handling for:

- **Configuration errors**: Invalid platform configs or JSON files
- **Camera initialization failures**: Hardware or driver issues
- **Buffer allocation errors**: Memory or resource constraints
- **Sync object failures**: Synchronization setup problems
- **Thread creation issues**: System resource limitations
- **File I/O errors**: Disk space or permission issues
- **Pipeline notifications**: Frame drops, capture timeouts, authentication failures

## Performance Considerations

- **Buffer counts**: Default configuration uses 4 ICP buffers and 64 ISP buffers per output
- **Thread affinity**: Consider CPU affinity for optimal performance
- **File I/O**: Large frame dumps can impact real-time performance
- **Memory usage**: Monitor system memory when capturing multiple high-resolution streams

## Troubleshooting

### Common Issues

1. **"Camera instance is null"**: Check camera configuration and hardware connection
2. **"RegisterImages failed"**: Verify sufficient memory and correct configuration
3. **"NITO file not found"**: Check NITO file path and permissions
4. **"Fence wait timeout"**: May indicate processing bottleneck or hardware issue
5. **"CoE override file not found"**: Verify override file path and read permissions
6. **"Invalid MAC address format"**: Ensure MAC addresses use XX:XX:XX:XX:XX:XX format
7. **"Invalid IP address format"**: Verify IP addresses are in XXX.XXX.XXX.XXX dotted decimal format

### Debug Steps

1. Increase verbosity level (`-v 4`) for detailed logging
2. Check network connectivity for CoE cameras
3. Verify NITO files exist in specified path
4. Check available network interfaces with `ip link show` or `ifconfig`
5. Verify HSB id matches the HSB id in transport settings and cameraConfig sensor

## Dependencies

### Build Dependencies
- NVIDIA SIPL Camera libraries
- NVIDIA SIPL Query libraries
- NvSci Buffer and Sync libraries
- JsonCpp library
- Standard C++17 libraries
