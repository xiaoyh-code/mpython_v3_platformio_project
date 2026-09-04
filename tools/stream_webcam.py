#!/usr/bin/env python3
"""
Stream the MacBook webcam to the ESP32-S3 TFT.

By default this sends a tiny 1-bit black-and-white bitmap to /api/frame/bw
for the fastest, most stable stream. Add --color to send a JPEG to /api/frame
instead (slower, but in color).

Usage:
    python3 tools/stream_webcam.py <esp-ip> [--color]

Example:
    python3 tools/stream_webcam.py 10.1.14.249
    python3 tools/stream_webcam.py 10.1.14.249 --color

Requires:
    pip install opencv-python requests numpy
"""

import sys
import time

import cv2
import numpy as np
import requests

ESP_IP = sys.argv[1] if len(sys.argv) > 1 else "10.1.14.249"
USE_COLOR = "--color" in sys.argv

URL = f"http://{ESP_IP}/api/frame" + ("/bw" if not USE_COLOR else "")
TIMEOUT_SECONDS = 5

# 1-bit stream dimensions (must match BW_SRC_WIDTH / BW_SRC_HEIGHT in src/main.cpp)
BW_WIDTH = 160
BW_HEIGHT = 86
BW_THRESHOLD = 128

# Color stream settings
COLOR_WIDTH = 320
COLOR_HEIGHT = 172
COLOR_JPEG_QUALITY = 40

SEND_INTERVAL_SECONDS = 0.05


def encode_bw(frame):
    resized = cv2.resize(frame, (BW_WIDTH, BW_HEIGHT))
    gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY)
    _, binary = cv2.threshold(gray, BW_THRESHOLD, 1, cv2.THRESH_BINARY)
    bits = np.packbits(binary, axis=1)
    return bits.tobytes()


def upscale_preview(frame):
    return cv2.resize(frame, (320, 172), interpolation=cv2.INTER_NEAREST)


def make_bw_multipart(payload):
    boundary = "----BWFrameBoundary"
    body = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="frame"; filename="frame.bin"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("utf-8")
    body += payload
    body += f"\r\n--{boundary}--\r\n".encode("utf-8")
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body)),
    }
    return body, headers


def encode_color_jpeg(frame):
    resized = cv2.resize(frame, (COLOR_WIDTH, COLOR_HEIGHT))
    _, encoded = cv2.imencode(
        ".jpg", resized, [int(cv2.IMWRITE_JPEG_QUALITY), COLOR_JPEG_QUALITY]
    )
    return encoded.tobytes()


def main():
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("Error: cannot open the MacBook camera.")
        sys.exit(1)

    print(f"Streaming to {URL}")
    print(f"Mode: {'color JPEG' if USE_COLOR else '1-bit black & white'}")
    print("Press 'q' in the preview window or Ctrl+C to stop")

    frame_count = 0
    last_fps_time = time.time()
    encode = encode_color_jpeg if USE_COLOR else encode_bw
    session = requests.Session()

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                continue

            payload = encode(frame)

            try:
                if USE_COLOR:
                    response = session.post(
                        URL,
                        data=payload,
                        headers={"Content-Type": "image/jpeg"},
                        timeout=TIMEOUT_SECONDS,
                    )
                else:
                    body, headers = make_bw_multipart(payload)
                    response = session.post(
                        URL, data=body, headers=headers, timeout=TIMEOUT_SECONDS
                    )
                if response.status_code == 200 and response.json().get("ok"):
                    frame_count += 1
            except requests.RequestException as e:
                print(f"POST error: {e}")
                time.sleep(0.5)

            if USE_COLOR:
                preview = cv2.resize(frame, (320, 172))
            else:
                small = cv2.resize(frame, (BW_WIDTH, BW_HEIGHT))
                gray = cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)
                _, preview = cv2.threshold(gray, BW_THRESHOLD, 255, cv2.THRESH_BINARY)
                preview = upscale_preview(preview)
            cv2.imshow("stream preview", preview)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

            now = time.time()
            if now - last_fps_time >= 1.0:
                print(f"FPS: {frame_count}")
                frame_count = 0
                last_fps_time = now

            time.sleep(SEND_INTERVAL_SECONDS)
    except KeyboardInterrupt:
        print("\nStopping stream")
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
