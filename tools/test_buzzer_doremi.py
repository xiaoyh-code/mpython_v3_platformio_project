#!/usr/bin/env python3
"""Trigger the GPIO21 buzzer Do-Re-Mi test."""
import sys

import requests

HOST = sys.argv[1] if len(sys.argv) > 1 else "10.1.14.249"
URL = f"http://{HOST}/test/buzzer/doremi"

print(f"Requesting {URL}")
resp = requests.get(URL, timeout=10)
print(resp.status_code, resp.text)
