# Realsense Holoscan Sensor Bridge

This repository includes multiple examples for different RealSense camera products integrated with the Holoscan Sensor Bridge.

![alt text](resources/depth.png)

![alt text](resources/dual_stream.png)

---

## 🧱 Project Structure

| Folder                                              | Purpose                                                  |
| --------------------------------------------------- | -------------------------------------------------------- |
| `holoscan-sensor-bridge/src/hololink/operators/`    | C++ operator source files                                |
| `holoscan-sensor-bridge/python/hololink/operators/` | Python bindings using pybind11                           |
| `realsense-holoscan/d457`                           | RealSense Holoscan applications for D457 GMSL camera     |
| `realsense-holoscan/d555`                           | RealSense Holoscan applications for D555 Ethernet camera |

---

## References

* [Prerequisite](#prerequisite)
* [Host Setup](#host-setup-agx-orin)
* [Build Holoscan Applications](#build-holoscan-applications)
* [Run RealSense Holoscan Applications](#run-realsense-holoscan-applications)
* [Run TAO PeopleNet Example](#run-tao-peoplenet-example)
* [Known Issues](#known-issues)

---

## Prerequisite

If your current configuration is a Lattice CPNX100-ETH-SENSOR-BRIDGE device currently loaded with <b>2412</br> hololink program will reprogram it:

```bash
hololink --force program scripts/manifest.yaml
# or
hololink --hololink=192.168.0.2 --force program scripts/manifest.yaml
```

reference [Host Firmware Update (NVIDIA)](https://docs.nvidia.com/holoscan/sensor-bridge/2.0.0/sensor_bridge_firmware_setup.html)


AGX Orin systems running JP6.0 release 2. NOTE THAT JETPACK 6.1 AND NEWER ARE NOT YET SUPPORTED

<!-- ### Custom FW for D555:

connect the D555 camera via usb to host (disconnect the Ethernet cable)

```bash
# Start container
cd ~/src/realsense-holoscan/holoscan-sensor-bridge
./docker/demo.sh

# Inside container using realsense sdk
cd ../realsense-holoscan
rs-fw-update -f rvp-flash-dfu.img

# if device in recovery use
rs-fw-update -r -f rvp-flash-dfu.img

# help
rs-fw-update --help
``` -->

<!-- Once the update completes successfully, replug the usb cable and plug Ethernet cable as well. -->

## Host Setup AGX Orin

Run on Host: 
```bash
# Linux sockets require a larger network receiver buffer.
echo 'net.core.rmem_max = 31326208' | sudo tee /etc/sysctl.d/52-hololink-rmem_max.conf
sudo sysctl -p /etc/sysctl.d/52-hololink-rmem_max.conf

# D457 (GMSL camera)
# Configure eth0 for a static IP address of 192.168.0.101.
sudo nmcli con add con-name hololink-eth0 ifname eth0 type ethernet ip4 192.168.0.101/24
sudo nmcli connection up hololink-eth0


# D555 (Ethernet camera)
# Configure eth0 for a static IP address of 192.168.11.101 with matching subnet.
sudo nmcli con add con-name hololink-eth0 ifname eth0 type ethernet ip4 192.168.11.101/24
sudo nmcli connection up hololink-eth0

# Install other dependencies
sudo apt-get update
sudo apt-get install -y git-lfs
```

reference [Host Setup Guide (NVIDIA)](https://docs.nvidia.com/holoscan/sensor-bridge/2.0.0/setup.html#sd-tab-item-1)


---

## Build Holoscan Applications

Run on Host: 
```bash
# Clone repo
mkdir -p ~/src/ && cd ~/src
git clone git@github.com:IntelRealSense/realsense-holoscan.git
cd realsense-holoscan/
git submodule init && git submodule update

# Build container
cd holoscan-sensor-bridge
./docker/build.sh --igpu
```

---

## Run RealSense Holoscan Applications

Start Container
```bash
# Start container
cd ~/src/realsense-holoscan/holoscan-sensor-bridge
./docker/demo.sh
```

Check if the camera is detected:
``` bash
#using hololink cli inside docker
hololink enumerate


# output example inside docker
mac_id=98:4F:EE:1A:F6:85 cpnx_version=0X2412 clnx_version=0X0 ip_address=192.168.0.2
mac_id=98:4F:EE:1A:F6:85 cpnx_version=0X2412 clnx_version=0X0 ip_address=192.168.0.2
mac_id=98:4F:EE:1A:F6:85 cpnx_version=0X2412 clnx_version=0X0 ip_address=192.168.0.2
...
```

Parameters:
```
--headless            Run in headless mode
--fullscreen          Run in fullscreen mode
--frame-limit FRAME_LIMIT
                    Exit after receiving this many frames
--configuration CONFIGURATION
                    Configuration file
--hololink HOLOLINK   IP address of Hololink board

```


Run Apps:

```bash
xhost +local:docker

# Inside docker container
cd ../realsense-holoscan

# or from anywhere in container because volume is mapped
cd <Project path on host>/realsense-holoscan/realsense-holoscan/

# D457 MIPI/GMSL
python3 d457/linux_realsense_player.py --help
python3 d457/linux_realsense_player.py                   # 1280x720 30fps depth
python3 d457/linux_realsense_player.py --camera-mode 4   # 1280x720 30fps depth
python3 d457/linux_realsense_player.py --camera-mode 6   # 1280x720 30fps RGB

# D555 Ethernet
python3 d555/linux_realsense_player.py --help
python3 d555/linux_realsense_player.py
python3 d555/linux_realsense_player.py --camera-mode 4   # 1280x720 30 fps depth
python3 d555/linux_realsense_player.py --camera-mode 6   # 1280x720 30 fps depth

# D555 Dual stream
python3 d555/linux_realsense_dual_stream.py --help
python3 d555/linux_realsense_dual_stream.py                                           # 1280x720 30 fps for both rgb and depth
python3 d555/linux_realsense_dual_stream.py --camera-mode-depth 4 --camera-mode-rgb 6 # 1280x720 30 fps for both rgb and depth
```

---

## Run TAO PeopleNet Example

This example runs inference using [PeopleNet](https://docs.nvidia.com/tao/tao-toolkit/text/model_zoo/cv_models/peoplenet.html) to detect people, bags, and faces, with bounding box overlays in a live RealSense video stream.

### Download Model

```bash
# D457
cd realsense-holoscan/
wget --content-disposition 'https://api.ngc.nvidia.com/v2/models/org/nvidia/team/tao/peoplenet/pruned_quantized_decrypted_v2.3.3/files?redirect=true&path=resnet34_peoplenet_int8.onnx' -O ~/src/realsense-holoscan/realsense-holoscan/d457/resnet34_peoplenet_int8.onnx

# D555
cd realsense-holoscan/
wget --content-disposition 'https://api.ngc.nvidia.com/v2/models/org/nvidia/team/tao/peoplenet/pruned_quantized_decrypted_v2.3.3/files?redirect=true&path=resnet34_peoplenet_int8.onnx' -O ~/src/realsense-holoscan/realsense-holoscan/d555/resnet34_peoplenet_int8.onnx
```

### Run Inference

```bash
# AGX Orin - D457
python3 d555/linux_realsense_peoplenet.py --camera-mode 4 # 1280x720 30fps
python3 d457/linux_realsense_peoplenet.py --camera-mode <any-rgb-profile>

# AGX Orin - D555
python3 d555/linux_realsense_peoplenet.py
python3 d555/linux_realsense_peoplenet.py --camera-mode <any-rgb-profile>

# AGX Orin - D555 Aligned Depth to RGB
python3 d555/linux_realsense_peoplenet_dual_stream.py
```

This opens the Holoscan visualizer GUI and overlays red/green bounding boxes for detections.

More information: [applications.md#tao\_peoplenet](applications.md#tao_peoplenet)

## Known Issues

 1. Resolution Compatibility

All Holoscan applications mentioned above have been tested and verified using a resolution of **1280x720 30fps**.  
While other resolutions may function, changing to a different resolution **requires a full camera reboot** beforehand to ensure proper operation. Attempting to switch resolutions without a reboot may result in undefined behavior or application failure.

 2. Application Exit Hang

In some cases, when closing the application (e.g., by closing the Holoviz window), the process does **not exit cleanly** and becomes unresponsive.  
If this occurs:

- Pressing `Ctrl+C` in the terminal usually terminates the process.
- If the application still does not close properly, a **camera reboot** is recommended to restore functionality.

These issues are under investigation, and further improvements may be introduced in future updates.
