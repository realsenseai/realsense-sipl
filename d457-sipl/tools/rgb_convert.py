#!/usr/bin/env python3
# Convert captured RAW16-container RGB frames (DS5 native YUV422-8) to viewable PNG/GIF.
import numpy as np, glob, os
from PIL import Image
W, H = 1280, 720
OUT = '/tmp/rgb_view'
os.makedirs(OUT, exist_ok=True)
files = sorted(glob.glob('/tmp/rgb.raw_cam_0_out_0_frame_*.raw'))
print('frames:', len(files))

def to_rgb(a, order):
    a = a.reshape(H, W * 2).astype(np.float32)
    if order == 'YUYV':            # bytes [Y0,U,Y1,V]
        Y = a[:, 0::2]
        U = a[:, 1::4]; V = a[:, 3::4]
    else:                          # UYVY: bytes [U,Y0,V,Y1]
        Y = a[:, 1::2]
        U = a[:, 0::4]; V = a[:, 2::4]
    U = np.repeat(U, 2, axis=1); V = np.repeat(V, 2, axis=1)
    Uf = U - 128.0; Vf = V - 128.0
    R = Y + 1.402 * Vf
    G = Y - 0.344 * Uf - 0.714 * Vf
    B = Y + 1.772 * Uf
    return np.clip(np.stack([R, G, B], -1), 0, 255).astype(np.uint8)

mid = files[len(files) // 2]
a = np.fromfile(mid, dtype=np.uint8)
Image.fromarray(to_rgb(a, 'YUYV')).save(f'{OUT}/frame_yuyv.png')
Image.fromarray(to_rgb(a, 'UYVY')).save(f'{OUT}/frame_uyvy.png')
print('wrote frame_yuyv.png, frame_uyvy.png from', os.path.basename(mid))

# short animated GIF (YUYV), downscaled, every 3rd frame
seq = files[::3][:40]
imgs = [Image.fromarray(to_rgb(np.fromfile(f, np.uint8), 'YUYV')).resize((640, 360)) for f in seq]
imgs[0].save(f'{OUT}/rgb_stream.gif', save_all=True, append_images=imgs[1:], duration=90, loop=0)
print('wrote rgb_stream.gif with', len(imgs), 'frames')
