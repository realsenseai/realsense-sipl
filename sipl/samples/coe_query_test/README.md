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

# NVSIPL CoE Query Test

This sample demonstrates how to use the NVIDIA SIPL Camera Query API to access camera configurations for Camera over Ethernet (CoE) cameras. It shows how to:

1. Parse the built-in database containing camera configurations
2. Parse additional JSON configuration files (optional)
3. Query specific camera configurations by name
4. List all available camera configurations
5. Display detailed configuration information in a compact format

## Features

- **Multiple operation modes**: List all configs, query specific config, or test with JSON files
- **Comprehensive output**: Displays camera, sensor, and transport information for CoE

## Requirements

- NVIDIA SIPL libraries (libnvsipl_query)
- Network interface with valid MAC address (for CoE cameras)
- Standard C++ libraries and networking headers

## Usage

The application supports three command-line options:

### List All Available Configurations
```bash
# List all camera configurations in the database
./nvsipl_coe_query_test -l
```

### Query Specific Configuration
```bash
# Get details for a specific camera configuration
./nvsipl_coe_query_test -c <configName>
```

### Test with JSON Configuration File
```bash
# Parse and test with an additional JSON configuration file
./nvsipl_coe_query_test -t <jsonConfigFile>
```

### Display Usage Information
```bash
# Show usage information
./nvsipl_coe_query_test
```

## Sample Output

### Listing All Configurations
```

ubuntu@jetson:~$ ./nvsipl_coe_query_test -l
List all configs
--- NVSIPL COE QUERY TEST ---
Initializing Query API...
------------
Camera: VB1940_Camera (ID: 0)
Platform: Vb1940 | Config: Thor_VB1940 | EEPROM: No
MIPI: DPHY 1140kbps 1140ksps
CoE: MAC=8c:1f:64:6d:70:03 | IP=192.168.1.2 | HSB=0

Sensor:
Name: VB1940 | ID: 0 | I2C: 0x10
Resolution: 2560x1984 @ 30fps | CFA: 36
Input Format: 8 | Embedded Data: No
Embedded Lines: Top=1 | Bottom=2
CoE Transport | HSB ID: 0 | Interface: eth0 | IP: 192.168.1.2 | VLAN: No | Sync: Yes
------------
Camera: VB1940_Camera_2sensor (ID: 0)
Platform: Vb1940 | Config: Thor_VB1940 | EEPROM: No
MIPI: DPHY 1140kbps 1140ksps
CoE: MAC=8c:1f:64:6d:70:03 | IP=192.168.1.2 | HSB=0

Sensor:
Name: VB1940 | ID: 0 | I2C: 0x10
Resolution: 2560x1984 @ 30fps | CFA: 36
Input Format: 8 | Embedded Data: No
Embedded Lines: Top=1 | Bottom=2
CoE Transport | HSB ID: 0 | Interface: eth0 | IP: 192.168.1.2 | VLAN: No | Sync: Yes
------------
Camera: VB1940_Camera_2sensor (ID: 1)
Platform: Vb1940 | Config: Thor_VB1940 | EEPROM: No
MIPI: DPHY 1140kbps 1140ksps
CoE: MAC=8c:1f:64:6d:70:03 | IP=192.168.1.2 | HSB=0

Sensor:
Name: VB1940 | ID: 0 | I2C: 0x10
Resolution: 2560x1984 @ 30fps | CFA: 36
Input Format: 8 | Embedded Data: No
Embedded Lines: Top=1 | Bottom=2
CoE Transport | HSB ID: 0 | Interface: eth0 | IP: 192.168.1.2 | VLAN: No | Sync: Yes
------------

### Testing with camera configuration name
```
ubuntu@jetson:~$ ./nvsipl_coe_query_test -c VB1940_Camera
Config name: VB1940_Camera
--- NVSIPL COE QUERY TEST ---
Initializing Query API...
------------
Parsing config name: VB1940_Camera
Platform: Vb1940 | Config: Thor_VB1940 | EEPROM: No
MIPI: DPHY 1140kbps 1140ksps
CoE: MAC=8c:1f:64:6d:70:03 | IP=192.168.1.2 | HSB=0

Sensor:
Name: VB1940 | ID: 0 | I2C: 0x10
Resolution: 2560x1984 @ 30fps | CFA: 36
Input Format: 8 | Embedded Data: No
Embedded Lines: Top=1 | Bottom=2
CoE Transport | HSB ID: 0 | Interface: eth0 | IP: 192.168.1.2 | VLAN: No | Sync: Yes
```

### Testing with JSON Configuration
```
ubuntu@jetson:~$ ./nvsipl_coe_query_test -t vb1940_1sensor_config.json
Test config file: vb1940_1sensor_config.json
--- NVSIPL COE QUERY TEST ---
Initializing Query API...
Successfully parsed JSON file: vb1940_1sensor_config.json
------------
Camera: VB1940_Camera (ID: 0)
Platform: Vb1940 | Config: Thor_VB1940 | EEPROM: No
MIPI: DPHY 1140kbps 1140ksps
CoE: MAC=8c:1f:64:6d:70:03 | IP=192.168.1.2 | HSB=0

Sensor:
Name: VB1940 | ID: 0 | I2C: 0x10
Resolution: 2560x1984 @ 30fps | CFA: 36
Input Format: 8 | Embedded Data: No
Embedded Lines: Top=1 | Bottom=2
CoE Transport | HSB ID: 0 | Interface: eth0 | IP: 192.168.1.2 | VLAN: No | Sync: Yes
```

## Output Format

The application provides information in several sections:

### Camera Configuration
- **Platform Information**: Platform name, configuration, and EEPROM support
- **MIPI Settings**: PHY mode (DPHY/CPHY) and data rates (kbps for DPHY, ksps for CPHY)
- **Camera Type**: 
  - **CoE**: MAC address, IP address, HSB ID

### Sensor Information
- **Basic Info**: Name, ID, I2C address
- **Resolution**: Width x height @ frame rate
- **Format Info**: CFA pattern, input format
- **Embedded Data**: Support status and line counts (top/bottom)
- **EEPROM**: Name and I2C address (if available)

### Transport Configuration
- **CoE Transport**: HSB ID, network interface, IP address, VLAN settings, synchronization

## Error Handling

The application includes comprehensive error handling for:
- Invalid command-line arguments with usage display
- Query API initialization failures
- Database parsing errors
- JSON file parsing errors (when specified)
- Configuration retrieval failures for specific camera names
- Missing device information

## Configuration Sources

The application reads configurations from:

1. **Built-in Database**: Standard SIPL driver configurations in `/usr/lib/nvsipl_drv`
2. **JSON Files**: Additional configuration files specified via the `-t` option

## Command Line Options

| Option | Argument | Description |
|--------|----------|-------------|
| `-l` | None | List all available camera configurations |
| `-c` | `<configName>` | Query a specific camera configuration by name |
| `-t` | `<jsonConfigFile>` | Parse and test with an additional JSON configuration file |
| (no args) | None | Display usage information |