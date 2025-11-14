import numpy as np
import cv2
import os

# Create a dummy 640x480 Bayer pattern image (RGGB)
height, width = 480, 640
bayer_image = np.zeros((height, width), dtype=np.uint16)

# Simulate a Bayer pattern with a simple gradient
for y in range(height):
    for x in range(width):
        if (y % 2 == 0) and (x % 2 == 0):  # R
            bayer_image[y, x] = int(1023 * x / width)
        elif (y % 2 == 0) and (x % 2 == 1):  # G1
            bayer_image[y, x] = int(1023 * y / height)
        elif (y % 2 == 1) and (x % 2 == 0):  # G2
            bayer_image[y, x] = int(1023 * y / height)
        else:  # B
            bayer_image[y, x] = int(1023 * (x + y) / (width + height))

# Save as raw file to simulate CSI data
dummy_bayer_path = "./dummy_frame_bayer.raw"
bayer_image.tofile(dummy_bayer_path)

dummy_bayer_path
