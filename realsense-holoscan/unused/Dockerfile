FROM nvcr.io/nvidia/clara-holoscan/holoscan:v3.2.0-dgpu

ENV HTTPS_PROXY=http://proxy-iil.intel.com:912
ENV https_proxy=http://proxy-iil.intel.com:912
ENV http_proxy=http://proxy-iil.intel.com:911
ENV HTTP_PROXY=http://proxy-iil.intel.com:911
ENV NO_PROXY=localhost,127.0.0.1,localaddress,intel.com

RUN apt-get update && apt-get install -y libusb-1.0-0
RUN pip install opencv-python-headless && pip install pyrealsense2

CMD ["bash"]