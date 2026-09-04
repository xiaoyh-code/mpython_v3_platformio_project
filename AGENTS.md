# Agent Notes for mPython V3 PlatformIO Project

This file is written for AI coding agents who need to work on this project. The project contains no `pyproject.toml`, `package.json`, or `Cargo.toml`; it is a PlatformIO embedded project for the mPython ESP32-S3 V3 board.

## Project Overview

- **Name**: mPython V3 PlatformIO Project
- **Target board**: custom `mpython_esp32s3_r8n16` (ESP32-S3R8N16, 16 MB flash, 8 MB PSRAM)
- **Framework**: Arduino (Espressif ESP32 core)
- **Build system**: PlatformIO
- **Main source**: `src/main.cpp`
- **Purpose**: a minimal webcam display firmware. The ESP32-S3 connects to WiFi, serves a single-page web UI, receives JPEG uploads or webcam frames from a MacBook, and displays them on the ST7789 TFT.

## Project Structure

```
.
├── .pio/                         # PlatformIO build artifacts and downloaded libraries
├── .vscode/                      # VS Code workspace configuration (see notes below)
├── boards/
│   └── mpython_esp32s3_r8n16.json  # Custom board definition
├── partitions/
│   ├── mpython_esp32S3_16MBapp.csv            # 16 MB single app partition (active)
│   ├── mpython_esp32S3_2MBapp_2MBota_12MBspiffs.csv
│   ├── mpython_esp32S3_4MBapp_4MBota_7MBspiffs.csv
│   └── mpython_esp32S3_8MBapp_8MBota.csv
├── src/
│   ├── main.cpp                  # Single firmware source file
│   ├── web_page.h                # Minified HTML UI for the HTTP upload/stream interface
│   └── web/index.html            # Editable version of the same HTML UI
├── variants/
│   └── mpython_V3_ESP32S3/
│       └── pins_arduino.h        # Custom Arduino variant header
├── .gitignore
├── HARDWARE_LOG.md               # Hardware discovery notes and pin map
├── README.md
└── platformio.ini                # PlatformIO project configuration
```

### Key Configuration Files

- `platformio.ini` — environment, dependencies, upload/monitor/debug settings.
- `boards/mpython_esp32s3_r8n16.json` — custom board definition (MCU, clock, flash, USB VID/PID, variant name).
- `variants/mpython_V3_ESP32S3/pins_arduino.h` — minimal Arduino variant header defining USB descriptors and default `TX`/`RX`/`SDA`/`SCL` pins.
- `partitions/*.csv` — flash partition tables. The active table is selected in `platformio.ini`.

## Technology Stack

- **Platform**: `espressif32`
- **Board**: `mpython_esp32s3_r8n16` (custom)
- **Framework**: `arduino`
- **Language**: C/C++ (Arduino APIs)
- **IDE**: Visual Studio Code with the PlatformIO IDE extension

### Declared Library Dependencies (`platformio.ini`)

| Library | Purpose |
|---|---|
| `moononournation/GFX Library for Arduino @ 1.6.0` | ST7789 LCD driver (Arduino_GFX) |
| `bodmer/TJpg_Decoder @ ^1.1.0` | JPEG decode for uploaded / streamed images |
| Built-in `WebServer` | HTTP server for the upload/stream web UI |
| Built-in `WiFi` | STA connection |

These libraries are downloaded by PlatformIO into `.pio/libdeps/`.

## Build, Upload, Monitor, and Debug Commands

Use the PlatformIO CLI (`pio`) or the VS Code PlatformIO extension.

```bash
# Build the firmware
pio run

# Upload firmware to the board
pio run --target upload

# Open serial monitor
pio device monitor

# Build and upload in one command
pio run --target upload --target monitor

# Clean build artifacts
pio run --target clean

# Start debugging with the ESP32-S3 built-in USB-JTAG debugger
pio debug --interface=gdb
```

### Important Upload Settings

- `upload_port = /dev/cu.usbmodem11401`
- `upload_speed = 115200`
- `upload_flags = --no-stub` — required because this board disconnects from the ESP32 upload stub after `Stub running...`. Do not remove this flag.

If macOS re-enumerates the board to a different `/dev/cu.usbmodem...` device, update `upload_port` and `monitor_port` in `platformio.ini`.

### Serial Monitor

- `monitor_speed = 115200`
- `monitor_filters = esp32_exception_decoder` — decodes ESP32 exception backtraces automatically.

### Debugging

- `debug_tool = esp-builtin`
- `debug_init_break = tbreak setup`
- VS Code `launch.json` contains three generated PlatformIO debug configurations. They assume the firmware has already been built (`.pio/build/mpython_esp32s3_r8n16/firmware.elf`).

## Hardware Architecture

The firmware targets the mPython ESP32-S3 V3 board. The pin constants below come from `src/main.cpp` and `HARDWARE_LOG.md`.

### GPIO Pin Map

| Function | Board label | GPIO | Notes |
|---|---:|---:|---|
| Button A | P5 | 0 | Active low |
| Button B | P11 | 46 | Active low |
| RGB LED data | P7 | 8 | 3x WS2812B |
| Buzzer | P12 | 21 | Driven with `tone()` |
| Microphone ADC | P10 | 6 | 12-bit ADC input |
| I2C SDA | P20 | 44 | Shared sensor bus |
| I2C SCL | P19 | 43 | Shared sensor bus |
| Touch P | P | 9 | Capacitive touch |
| Touch Y | Y | 10 | Capacitive touch |
| Touch T | T | 11 | Capacitive touch |
| Touch H | H | 12 | Capacitive touch |
| Touch O | O | 13 | Capacitive touch |
| Touch N | N | 14 | Capacitive touch |
| LCD BL | - | 33 | Backlight enable, HIGH = on |
| LCD CS | - | 34 | ST7789 chip select |
| LCD RS | - | 35 | Register select / data-command |
| LCD SCK | - | 36 | SPI clock |
| LCD SDA | - | 37 | SPI serial data, equivalent to MOSI |

### I2C Sensor Bus

- SDA: GPIO 44
- SCL: GPIO 43
- Speed: 400 kHz

Detected devices:

| Address | Device |
|---|---|
| `0x30` | MMC5603NJ magnetometer |
| `0x53` | LTR-308ALS light sensor |
| `0x6B` | QMI8658C accelerometer + gyroscope |

### LCD Configuration

- Controller: ST7789
- Visible resolution: 320 x 172
- Panel size passed to driver: 172 x 320
- Rotation: 5
- Offset X: 34, Offset Y: 0
- Bus: ESP32 hardware SPI through Arduino_GFX (`Arduino_ESP32SPI`)
- MISO: not used

## Code Organization

All firmware logic is in a single file, `src/main.cpp`. The file is organized as follows:

1. **Pin and display constants** — `static constexpr` definitions for LCD GPIOs, geometry, and timing.
2. **Global driver objects** — `Arduino_GFX` for the ST7789 LCD, `WebServer` for HTTP, `TJpgDec` for JPEG decode.
3. **Helper functions** — WiFi connection, JPEG decode callback, HTTP route handlers.
4. **LCD rendering** — `drawStandbyScreen()` and JPEG rendering via `TJpg_Decoder`.
5. **HTTP interface** — `WebServer` routes for `/`, `/upload`, `/api/frame`, and `/status`; HTML UI served from PROGMEM.
6. **Initialization** — LCD backlight, LCD driver, WiFi, and HTTP server setup.
7. **Arduino entry points** — `setup()` and `loop()`.

The current firmware does not use the I2C bus, buttons, RGB LEDs, buzzer, microphone, touch pads, or on-board sensors. Those peripherals are documented in `HARDWARE_LOG.md` for reference but are not active in this webcam/stream version.

## Development Conventions

- Use `static constexpr` for compile-time constants such as pin numbers and timing values.
- Use `uint8_t`, `uint16_t`, `int16_t`, and `uint32_t` from `<stdint.h>` for hardware-related values.
- Global objects are declared near the top of `main.cpp`; avoid introducing hidden state in new modules unless the project is refactored into multiple files.
- Keep the LCD standby screen minimal; the TFT is primarily used for uploaded/streamed images.
- Pin numbers and display geometry are duplicated between `src/main.cpp` and `HARDWARE_LOG.md`. When changing hardware constants, update both files and this `AGENTS.md` if needed.

## Testing Instructions

1. Connect the board and confirm the serial port matches `upload_port`/`monitor_port` in `platformio.ini`.
2. Build and upload: `pio run --target upload`.
3. Open the serial monitor: `pio device monitor`.
4. Wait for the serial output to print the ESP32's IP address (e.g., `http://192.168.1.123/`).
5. Open that URL in a browser on the same WiFi network.
6. Test static upload: choose a JPEG/PNG/WebP file, click *Upload to TFT*, and verify it appears on the TFT.
7. Test webcam stream:
   - Browser path: click *Start Camera* and grant permission. This only works on secure origins (`localhost` or HTTPS). On an IP like `172.20.10.2` the browser will block camera access.
   - Python path (recommended): install `opencv-python` and `requests`, then run `python3 tools/stream_webcam.py <esp-ip>`.
8. After 5 seconds with no new frame/upload, the TFT returns to the standby screen showing the IP.

There are no automated unit tests in this project; validation is done on the physical board.

## Partition Tables

Four partition layouts are provided under `partitions/`:

| File | Layout |
|---|---|
| `mpython_esp32S3_16MBapp.csv` | Single ~16 MB app, no OTA, no filesystem (active) |
| `mpython_esp32S3_8MBapp_8MBota.csv` | Two 8 MB app slots, no filesystem |
| `mpython_esp32S3_4MBapp_4MBota_7MBspiffs.csv` | Two 4.5 MB app slots + 6.9 MB SPIFFS |
| `mpython_esp32S3_2MBapp_2MBota_12MBspiffs.csv` | Two 2 MB app slots + 11.9 MB SPIFFS |

The active table is set in `platformio.ini` via `board_build.partitions`. The default `boards/mpython_esp32s3_r8n16.json` also references `default_16MB.csv`, but `platformio.ini` overrides it.

## Security Considerations

- This is an embedded test firmware; it does not implement authentication, encryption, or secure boot.
- The firmware runs an open HTTP server on port 80 and connects to WiFi with credentials hardcoded in `src/main.cpp`. Do not deploy this on untrusted networks or commit real credentials.
- The serial port is used for plaintext debug output; do not log sensitive information.
- The `--no-stub` upload flag is required for this board but disables the ESP32 upload stub; ensure you understand the trade-offs if you change upload tooling.

## Files to Keep in Sync

When modifying hardware parameters or the test firmware, update these files together:

- `src/main.cpp` — source of truth for pin numbers and display geometry.
- `HARDWARE_LOG.md` — human-readable hardware log and pin map.
- `platformio.ini` — upload/monitor port and partition table selection.
- `boards/mpython_esp32s3_r8n16.json` — board-level constants.
- `variants/mpython_V3_ESP32S3/pins_arduino.h` — Arduino variant defaults.
- `AGENTS.md` — this file.
