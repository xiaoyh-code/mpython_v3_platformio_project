#!/usr/bin/env python3
"""Upload a 320x172 color test pattern to the ESP32 TFT.

Usage:
    python3 tools/upload_test_pattern.py [HOST]

Default HOST is 10.1.14.249.
"""
import sys

import cv2
import numpy as np
import requests

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.1.14.249"
URL = f"http://{HOST}/upload"

W, H = 320, 172

# Build vertical bars: white, red, green, blue, black, gray
bar_w = W // 6
colors = [
    (255, 255, 255),  # white
    (255, 0, 0),      # red
    (0, 255, 0),      # green
    (0, 0, 255),      # blue
    (0, 0, 0),        # black
    (128, 128, 128),  # gray
]

img_rgb = np.zeros((H, W, 3), dtype=np.uint8)
for i, color in enumerate(colors):
    x0 = i * bar_w
    x1 = W if i == len(colors) - 1 else (i + 1) * bar_w
    img_rgb[:, x0:x1] = color

# Add a fine gradient at the bottom to check banding
for x in range(W):
    v = int(255 * x / (W - 1))
    img_rgb[H - 20:, x] = (v, v, v)

img_bgr = cv2.cvtColor(img_rgb, cv2.COLOR_RGB2BGR)
ok, encoded = cv2.imencode(".jpg", img_bgr, [int(cv2.IMWRITE_JPEG_QUALITY), 95])
if not ok:
    print("JPEG encode failed")
    sys.exit(1)

jpeg_bytes = encoded.tobytes()
print(f"Uploading {len(jpeg_bytes)} bytes to {URL}")
resp = requests.post(URL, files={"file": ("test_pattern.jpg", jpeg_bytes, "image/jpeg")})
print(resp.status_code, resp.text)
