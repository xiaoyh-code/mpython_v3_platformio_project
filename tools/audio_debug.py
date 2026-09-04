#!/usr/bin/env python3
"""Read ES8388 debug registers from the ESP32 firmware.

Usage:
    python3 tools/audio_debug.py [HOST]

Default HOST is 10.1.14.249.
"""
import json
import sys

import requests

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.1.14.249"
URL = f"http://{HOST}/test/audio/debug"

print(f"Requesting {URL}")
resp = requests.get(URL, timeout=10)
print(resp.status_code)
try:
    data = resp.json()
    print(json.dumps(data, indent=2, sort_keys=True))
except ValueError:
    print(resp.text)
