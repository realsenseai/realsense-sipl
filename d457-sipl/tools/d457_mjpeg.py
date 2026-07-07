#!/usr/bin/env python3
"""Serve the D457 SIPL capture as a live MJPEG stream over HTTP, with an FPS +
resolution overlay.

Reads the per-frame .raw files nvsipl_camera dumps into /tmp/live, converts the
newest COMPLETE frame to JPEG, overlays "<VIEW>  <WxH>  <fps>", and serves it as
multipart/x-mixed-replace so a browser <img> shows live video. Deletes consumed
frames to bound disk use. FPS is measured from the capture frame-index rate (the
true camera rate), not the serve rate, and is also printed to the console.

env:  D457_VIEW = rgb|ir|depth   D457_PORT = 8080   D457_DIR = /tmp/live
"""
import os, glob, io, re, time, threading, collections
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 1280, 720
DISP = (640, 360)
DIR  = os.environ.get('D457_DIR', '/tmp/live')
VIEW = os.environ.get('D457_VIEW', 'rgb')
PORT = int(os.environ.get('D457_PORT', '8080'))

_IDX = re.compile(r'frame_(\d+)\.raw$')
_hist = collections.deque(maxlen=120)   # (time, frame_index) samples
_last_print = [0.0]

try:
    FONT = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
except Exception:
    FONT = ImageFont.load_default()

def frame_index(path):
    m = _IDX.search(path)
    return int(m.group(1)) if m else None

def measure_fps(path):
    """Update history from the newest frame index and return fps over the window."""
    idx = frame_index(path)
    now = time.time()
    if idx is not None and (not _hist or idx != _hist[-1][1]):
        _hist.append((now, idx))
    # drop samples older than 2 s
    while len(_hist) >= 2 and now - _hist[0][0] > 2.0:
        _hist.popleft()
    if len(_hist) >= 2:
        dt = _hist[-1][0] - _hist[0][0]
        di = _hist[-1][1] - _hist[0][1]
        fps = di / dt if dt > 0 else 0.0
    else:
        fps = 0.0
    if now - _last_print[0] >= 1.0:
        print('%s  %dx%d  %5.1f fps' % (VIEW, W, H, fps), flush=True)
        _last_print[0] = now
    return fps

def janitor():
    """Continuously delete old frames so the capture always has tmpfs space — INDEPENDENT of
    whether a browser is connected. (If this only ran while streaming, the tmpfs would fill
    before the first client connects, stalling the capture -> frozen frame / 0 fps.)"""
    while True:
        fs = sorted(glob.glob(DIR + '/*.raw'), key=os.path.getmtime)
        for f in fs[:-4]:
            try: os.remove(f)
            except OSError: pass
        time.sleep(0.03)

def newest_complete():
    fs = glob.glob(DIR + '/*.raw')
    if not fs:
        return None
    fs.sort(key=lambda f: os.path.getmtime(f))
    return fs[-2] if len(fs) >= 2 else fs[-1]   # -2 = last fully-written file

def to_jpeg(path, fps):
    a = np.fromfile(path, dtype=np.uint8)
    if a.size < W * H * 2:
        return None
    a = a[:W * H * 2]
    if VIEW == 'rgb':                      # YUYV [Y0,U,Y1,V]
        b = a.reshape(H, W * 2).astype(np.float32)
        Y = b[:, 0::2]; U = b[:, 1::4]; V = b[:, 3::4]
        U = np.repeat(U, 2, 1) - 128.0; V = np.repeat(V, 2, 1) - 128.0
        R = Y + 1.402 * V; G = Y - 0.344 * U - 0.714 * V; B = Y + 1.772 * U
        img = np.clip(np.stack([R, G, B], -1), 0, 255).astype(np.uint8)
    elif VIEW == 'ir':                     # interleaved L/R Y8; show left view
        img = np.stack([a.reshape(H, W * 2)[:, 0::2]] * 3, -1)
    else:                                  # depth Z16 (LE) -> grayscale 0..4 m
        z = a.view('<u2').reshape(H, W).astype(np.float32)
        z = np.clip(z / 4000.0 * 255.0, 0, 255).astype(np.uint8)
        img = np.stack([z] * 3, -1)
    im = Image.fromarray(img).resize(DISP)
    d = ImageDraw.Draw(im)
    label = '%s  %dx%d  %.1f fps' % (VIEW.upper(), W, H, fps)
    d.rectangle([0, 0, 8 + 9 * len(label), 26], fill=(0, 0, 0))
    d.text((5, 4), label, fill=(0, 255, 0), font=FONT)
    buf = io.BytesIO(); im.save(buf, 'JPEG', quality=70)
    return buf.getvalue()

PAGE = ('<html><body style="margin:0;background:#111;text-align:center">'
        '<img src="/stream" style="max-width:100%;height:auto"></body></html>').encode()

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path != '/stream':
            self.send_response(200); self.send_header('Content-Type', 'text/html')
            self.end_headers(); self.wfile.write(PAGE); return
        self.send_response(200)
        self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
        self.end_headers()
        try:
            while True:
                p = newest_complete()
                if p:
                    fps = measure_fps(p)
                    try: j = to_jpeg(p, fps)
                    except Exception: j = None
                    if j:
                        self.wfile.write(b'--frame\r\nContent-Type: image/jpeg\r\n'
                                         b'Content-Length: %d\r\n\r\n' % len(j) + j + b'\r\n')
                time.sleep(0.04)
        except (BrokenPipeError, ConnectionResetError):
            pass

if __name__ == '__main__':
    srv = None
    for attempt in range(12):                 # tolerate a briefly-held port from a prior server
        try:
            srv = ThreadingHTTPServer(('0.0.0.0', PORT), Handler); break
        except OSError as e:
            if e.errno == 98 and attempt < 11:
                if attempt == 0:
                    print('port %d busy, waiting for it to free...' % PORT, flush=True)
                time.sleep(1); continue
            raise
    threading.Thread(target=janitor, daemon=True).start()   # keep tmpfs drained regardless of clients
    print('D457 MJPEG %s %dx%d on :%d  (open /  for the viewer)' % (VIEW, W, H, PORT), flush=True)
    srv.serve_forever()
