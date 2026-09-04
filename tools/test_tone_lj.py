#!/usr/bin/env python3
"""Trigger the ESP32 left-justified audio tone test."""
import sys

import requests

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.1.14.249"
URL = f"http://{HOST}/test/tone/lj"

print(f"Requesting {URL}")
resp = requests.get(URL, timeout=10)
print(resp.status_code, resp.text)
