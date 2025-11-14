# FUSA PDK Package

This package contains HSL (Hardware Security Layer), UDDF (Unified Device Driver Framework), Hololink, UDDF-CDD, and Fmtlib components for camera and functional safety applications.

## Contents

- `hsl/`: Hardware Sequencing Language components
  - Header files for bytecode interfaces and definitions
  - Python tools for HSL compilation and encoding
  - Prebuilt libraries for decoder and encoder functionality
  - CMake integration rules

- `uddf/`: Unified Device Driver Framework components
  - Core UDDF interfaces and types
  - Driver and hardware access APIs
  - Sample drivers and libraries (CoE, HSB, Eagle)

- `uddf/samples/drivers/eagleAIO/`: Eagle AIO driver implementation
  - `hololink/`: Hololink core library files for camera communication
    - Networking, serialization, and data channel management
    - Traditional I2C/SPI interfaces
    - JESD and CSI controller support
  - `drivers/`: UDDF-CDD driver implementations
    - HSB (High Speed Bridge) transport drivers with hololink stubs
    - VB1940 sensor module and camera drivers with Python HSL integration
  - `libraries/`: Eagle-specific library implementations
  - `fmt/`: Fmtlib header-only formatting library

## Prerequisites for Building

Before building the package, ensure you have the following prerequisites installed:

- **Python 3.12 or higher**: Required for HSL Python components and VB1940 drivers
- **CMake**: Required for building C++ components
- **build-essential**: Required for compilation (includes gcc, make, etc.)
- **python3.12-dev**: Required for Python development headers

### Installation on Ubuntu/Debian:
```bash
sudo apt update
sudo apt install python3.12 python3.12-dev cmake build-essential
```

## Building

### Option 1: Build All Sample Drivers
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build all sample drivers
make samples

# Or build everything
make -j$(nproc)

# Install libraries (optional)
sudo make install
```

### Option 2: Build Individual Sample Drivers

#### Build CoE Sample Driver Only
```bash
mkdir build && cd build
cmake ..
make sample_coe_driver
```

#### Build EagleAIO Sample Driver Only
```bash
mkdir build && cd build
cmake ..
make nvuddf_eagle_driver
```

### Option 3: Build with Custom Configuration
```bash
mkdir build && cd build

# Configure with specific options
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=ON

# Build all samples
make samples
```

### Build Output
After successful build, you'll find:
- **Sample Driver Libraries**:
  - `build/uddf/samples/drivers/coe/libsample_coe_driver.so` (CoE sample driver)
  - `build/uddf/samples/drivers/eagleAIO/libnvuddf_eagle_driver.so` (EagleAIO sample driver)
- **HSL Libraries**: `build/hsl/lib-target/` (if building HSL components)
- **Headers**: `build/include/` or `build/hsl/include/`
- **Generated HSL files**:
  - `build/uddf/samples/drivers/coe/hsl_gen/` (CoE driver HSL files)
  - `build/uddf/samples/drivers/eagleAIO/hsl_gen/` (EagleAIO driver HSL files)

### Available Build Targets
- `samples`: Build all sample drivers
- `sample_coe_driver`: Build CoE sample driver only
- `nvuddf_eagle_driver`: Build EagleAIO sample driver only
- `nvsipl_coe_camera`: Build CoE camera sample application
- `nvsipl_coe_query_test`: Build CoE query test sample application
- `install`: Install libraries to `/usr/lib/nvsipl_uddf/` and apps to `/usr/bin/`

## Cross-Platform Build Instructions

### Building on Host and Deploying to Target Device

If you need to build the package on a host machine and deploy it to a target device (e.g., NVIDIA Jetson), follow these steps:

#### On Host Machine
```bash
# Navigate to the camera-public directory
cd $TEGRA_TOP/camera-public/

# Create a tarball of the SIPL package
tar -cjvf sipl-public.tar.bz2 sipl

# Transfer the tarball to the target device
scp sipl-public.tar.bz2 ubuntu@<target ip>:/home/ubuntu/
```

#### On Target Device
```bash
# Install required packages
sudo apt-get update && sudo apt-get install build-essential
sudo apt-get install cmake

# Navigate to home directory and extract the package
cd /home/ubuntu
tar -xf sipl-public.tar.bz2

# Build the package
cd sipl/
mkdir build && cd build
cmake ../
Use cmake -DCMAKE_INSTALL_PREFIX=/usr .. to install in particular dir(in this case its /usr)
make
sudo make install
```

### Prerequisites for Target Device
Before building on the target device, ensure the following packages are installed:
- `build-essential`: Provides essential build tools (gcc, make, etc.)
- `cmake`: Required for the build system
- Additional NVIDIA libraries may be required depending on your target platform

## Documentation

See the README.md files in each component directory for detailed documentation and usage examples.
