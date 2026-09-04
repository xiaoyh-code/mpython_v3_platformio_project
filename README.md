# mPython ESP32-S3 V3 TFT and Buzzer Lab

PlatformIO firmware for the mPython ESP32-S3 V3 board. The board connects to WiFi, serves a small web UI, receives images or webcam frames from a computer, displays them on the onboard ST7789 TFT, and exposes test controls for the onboard GPIO21 buzzer.

## Current Features

- ST7789 TFT output at 320 x 172 visible pixels.
- Browser UI served directly by the ESP32 at `http://<board-ip>/`.
- JPEG upload to the TFT.
- Webcam-to-TFT streaming from the browser or Python.
- 1-bit black-and-white webcam stream for faster frame rate.
- TFT color test pattern.
- Buzzer piano UI in the browser.
- Buzzer Do-Re-Mi and long-tone test endpoints.
- Experimental microphone pitch follower from a Mac to the buzzer.
- Experimental ES8388/I2S test code kept in the firmware for hardware investigation.

The GPIO21 buzzer is not a real speaker. It works well for beep/tone tests, but it cannot reproduce normal voice audio cleanly. The voice tool therefore has a `pitch` mode that tries to detect sung notes and make the buzzer play matching tones.

## Hardware

| Function | Board label | GPIO |
|---|---:|---:|
| Buzzer | P12 | 21 |
| Button B | P11 | 46 |
| I2C SDA | P20 | 44 |
| I2C SCL | P19 | 43 |
| LCD backlight | internal | 33 |
| LCD CS | internal | 34 |
| LCD RS / DC | internal | 35 |
| LCD SCK | internal | 36 |
| LCD SDA / MOSI | internal | 37 |
| ES8388 DOUT | internal | 38 |
| ES8388 MCLK | internal | 39 |
| ES8388 DIN | internal | 40 |
| ES8388 BCK | internal | 41 |
| ES8388 LRCK | internal | 42 |

More hardware notes are in [HARDWARE_LOG.md](HARDWARE_LOG.md).

## Requirements

- mPython ESP32-S3 V3 board.
- USB-C cable with data support.
- PlatformIO CLI or VS Code PlatformIO extension.
- Python 3 for helper scripts.
- Computer and board on the same WiFi network.

Install Python helper dependencies when needed:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install requests opencv-python numpy sounddevice
```

## Build and Upload

Check the serial device in `platformio.ini` first. The committed port is only the last port used on one Mac, not a universal value.

```bash
pio device list
```

Then change both lines in `platformio.ini` to your own board port:

```ini
upload_port = /dev/cu.usbmodemXXXX
monitor_port = /dev/cu.usbmodemXXXX
```

On macOS the port is usually `/dev/cu.usbmodem...`, and it can change after reconnecting the board.

```bash
pio run
pio run --target upload
pio device monitor
```

This board currently needs:

```ini
upload_flags = --no-stub
```

Do not remove that flag unless the upload behavior has been re-tested.

## WiFi Setup

WiFi credentials are currently hardcoded near the top of `src/main.cpp`:
WiFi credentials are loaded from a local file that is intentionally ignored by git:

```bash
cp src/wifi_config.example.h src/wifi_config.h
```

Then edit `src/wifi_config.h`:

```cpp
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

After upload, open the serial monitor and wait for the board to print its IP address.

## Web UI

Open this in a browser on the same network:

```text
http://<board-ip>/
```

The web UI includes:

- Static image upload.
- Webcam stream.
- TFT color test.
- Buzzer piano.
- Browser microphone voice-to-buzzer experiment.

Browser camera and microphone access may be blocked on plain HTTP IP addresses. If that happens, use the Python scripts in `tools/`.

## Useful Test Commands

Replace `10.1.14.249` with the board IP shown in the serial monitor.

```bash
.venv/bin/python tools/upload_test_pattern.py 10.1.14.249
.venv/bin/python tools/stream_webcam.py 10.1.14.249
.venv/bin/python tools/stream_webcam.py 10.1.14.249 --color
.venv/bin/python tools/test_buzzer_doremi.py 10.1.14.249
.venv/bin/python tools/test_buzzer_tone.py 10.1.14.249
.venv/bin/python tools/stream_voice_buzzer.py 10.1.14.249 --mode pitch --threshold 0.006 --min-freq 120 --max-freq 550
```

Detailed board test steps are in [docs/testing.md](docs/testing.md).

## HTTP and UDP Interfaces

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | Web UI |
| `/upload` | POST multipart | Upload a JPEG to the TFT |
| `/api/frame` | POST | Send one JPEG frame to the TFT |
| `/api/frame/bw` | POST multipart | Send one 1-bit black-and-white frame |
| `/test/colors` | GET | Draw TFT color bars |
| `/test/buzzer/tone` | GET | Play a long 440 Hz buzzer tone |
| `/test/buzzer/doremi` | GET | Play Do-Re-Mi on the buzzer |
| `/api/buzzer/tone?freq=440&duration=350` | GET | Play one buzzer note |
| `/api/buzzer/stop` | GET | Stop buzzer output |
| `/api/buzzer/audio` | GET | Return buzzer audio stream info |
| `/api/buzzer/audio` | POST | Experimental raw 8-bit PCM over HTTP |
| `/status` | GET | Board status JSON |

UDP port `4210` accepts experimental buzzer audio packets:

- `T` plus two big-endian bytes: play a tone frequency in Hz.
- `S`: stop buzzer.
- Raw unsigned 8-bit PCM packets: experimental only; this clicks on the buzzer and is not useful for real voice.

## Project Layout

```text
boards/                         custom PlatformIO board definition
partitions/                     ESP32-S3 flash partition tables
src/main.cpp                    firmware
src/web_page.h                  web UI embedded in flash
src/web/index.html              editable copy of the web UI
tools/                          Python upload, webcam, buzzer, and audio helpers
variants/mpython_V3_ESP32S3/    Arduino variant pin defaults
HARDWARE_LOG.md                 hardware notes and pin map
docs/testing.md                 physical-board test checklist
```

## Notes

- The repo intentionally ignores `.pio/`, `.venv/`, `.DS_Store`, and Python cache files.
- This firmware is for local hardware testing. It has no authentication and serves an open HTTP server on the local network.
- Local WiFi credentials belong in `src/wifi_config.h`, which is ignored by git. Do not commit real WiFi credentials.
