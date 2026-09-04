# mPython ESP32-S3 V3 Hardware Log

This file records the current working hardware parameters for the PlatformIO project in this folder.

## Current firmware files

- Main firmware: `src/main.cpp`
- HTML UI: `src/web_page.h` (minified) and `src/web/index.html` (editable)
- Project config: `platformio.ini`

> Historical files `src/i2c_scanner.cpp` and `google_sheets_click_logger.gs` were removed when the project was simplified to the webcam/stream firmware.

## Board and environment

- Board env: `mpython_esp32s3_r8n16`
- Platform: `espressif32`
- Framework: `arduino`
- Configured upload port in `platformio.ini`: `/dev/cu.usbmodem21301`
- Configured monitor port in `platformio.ini`: `/dev/cu.usbmodem21301`
- Last detected / used upload port: `/dev/cu.usbmodem21301`
- Upload mode: `--no-stub`

## GPIO pin map

| Function | Board label | GPIO | Notes |
|---|---:|---:|---|
| Button A | P5 | 0 | Active low |
| Button B | P11 | 46 | Active low |
| RGB LED data | P7 | 8 | 3x WS2812B |
| Buzzer | P12 | 21 | Confirmed working with `tone()`; used by web piano and buzzer tests |
| Microphone ADC | P10 | 6 | Analog input |
| I2C SDA | P20 | 44 | Shared I2C sensor bus |
| I2C SCL | P19 | 43 | Shared I2C sensor bus |
| Touch P | P | 9 | Capacitive touch |
| Touch Y | Y | 10 | Capacitive touch |
| Touch T | T | 11 | Capacitive touch |
| Touch H | H | 12 | Capacitive touch |
| Touch O | O | 13 | Capacitive touch |
| Touch N | N | 14 | Capacitive touch |
| LCD BL | - | 33 | ST7789 backlight enable |
| LCD CS | - | 34 | ST7789 chip select |
| LCD RS | - | 35 | ST7789 register select / data-command |
| LCD SCK | - | 36 | ST7789 SPI clock |
| LCD SDA | - | 37 | ST7789 SPI serial data, equivalent to MOSI |

## Pin conflict audit

Checked against `src/main.cpp`, `src/i2c_scanner.cpp`, `variants/mpython_V3_ESP32S3/pins_arduino.h`, and this hardware log.

### Working hardware map

No GPIO is intentionally shared by two active hardware functions in the known board pin map:

- `GPIO0`: Button A
- `GPIO6`: Microphone ADC
- `GPIO8`: WS2812B RGB LED data
- `GPIO9` to `GPIO14`: capacitive touch pads
- `GPIO21`: buzzer
- `GPIO33` to `GPIO37`: ST7789 LCD SPI/backlight pins
- `GPIO43` / `GPIO44`: external I2C sensor bus
- `GPIO46`: Button B

### Important caveats

- The ST7789 LCD has no I2C address. Its official pin table is `BL=GPIO33`, `CS=GPIO34`, `RS=GPIO35`, `SCK=GPIO36`, `SDA=GPIO37`; this LCD `SDA` is SPI serial data, equivalent to MOSI.
- The SSD1306 OLED at `0x3C`, if connected, shares only the I2C bus on `GPIO44`/`GPIO43`. It does not conflict with the ST7789 SPI LCD.
- `GPIO0` is also an ESP32-S3 boot strapping pin. Button A works as a button, but holding it during reset can affect boot mode.
- `variants/mpython_V3_ESP32S3/pins_arduino.h` now defines default `SDA=44` and `SCL=43`, matching the confirmed board I2C bus.
- The same variant file still defines `TX=43` and `RX=44`, which overlap the hardware I2C bus pins. The project uses USB CDC serial, so this is not an active conflict unless hardware UART is used on those default aliases.

## I2C devices found

Bus used by code:

- SDA: `GPIO44`
- SCL: `GPIO43`
- Speed: `400000`

Detected / mapped addresses:

| I2C address | Device | Status |
|---|---|---|
| `0x10` | Possible ES8388 audio codec control | Detected, still needs driver-level confirmation |
| `0x30` | MMC5603NJ magnetometer | Confirmed |
| `0x3C` | SSD1306 0.96 inch OLED, 128 x 64 | Confirmed working with Adafruit SSD1306 in the current display sketch |
| `0x53` | LTR-308ALS light sensor | Confirmed |
| `0x6B` | QMI8658C accelerometer + gyroscope | Confirmed |
| `0x7E` | Reserved address | Ignore unless a driver proves otherwise |

Possible codec control addresses mentioned in code, not confirmed by current scan:

- `0x11`
- `0x20`
- `0x22`

Latest user scan after adding the OLED:

```text
0x10  possible ES8388 audio codec control
0x30  MMC5603NJ magnetometer
0x3C  SSD1306 128x64 OLED
0x53  LTR-308ALS light sensor
0x6B  QMI8658C accel/gyro
0x7E  reserved address; ignore unless a driver proves it
Found 6 device(s)
```

## OLED / I2C details

Display setup currently confirmed working:

- Controller: `SSD1306`
- Address: `0x3C`
- Resolution: `128 x 64`
- Bus: I2C on `SDA=GPIO44`, `SCL=GPIO43`
- Driver class: `Adafruit_SSD1306`
- Graphics base: `Adafruit_GFX`
- Reset pin: not connected, passed as `-1`
- Power mode used by driver: `SSD1306_SWITCHCAPVCC`

Working constructor and init used in `src/main.cpp`:

```cpp
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
```

Notes:

- U8G2 detected/used the bus but did not produce visible output in this project state.
- Adafruit SSD1306 produced visible OLED output and is the confirmed working OLED library for this module.
- The TFT status area checks `0x3C` every 2 seconds and reports `seen` / `missing` plus SSD1306 init status.

## LCD / SPI details

Display setup currently used by the display test firmware:

- Controller: `ST7789`
- Visible resolution: `320 x 172`
- Panel size passed to driver: `172 x 320`
- Rotation: `5`
- Offset X: `34`
- Offset Y: `0`
- Driver class: `Arduino_ST7789`
- Bus class: `Arduino_ESP32SPI`
- SPI mode: handled by the library, ST7789 driver sets `SPI_MODE3` on ESP32
- MISO: not used

Current constructor used in `src/main.cpp`:

```cpp
Arduino_DataBus *lcdBus = new Arduino_ESP32SPI(
    PIN_LCD_RS, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_SDA, GFX_NOT_DEFINED);

Arduino_GFX *lcd = new Arduino_ST7789(
    lcdBus, GFX_NOT_DEFINED, LCD_ROTATION, true, LCD_PANEL_WIDTH,
    LCD_PANEL_HEIGHT, LCD_OFFSET_X, LCD_OFFSET_Y);
```

## Component libraries

| Component / use | Library | GitHub |
|---|---|---|
| LCD graphics | GFX Library for Arduino | https://github.com/moononournation/Arduino_GFX |
| WS2812B RGB LEDs | Adafruit NeoPixel | https://github.com/adafruit/Adafruit_NeoPixel |
| Button handling | OneButton | https://github.com/mathertel/OneButton |
| Light sensor LTR-308ALS | LTR308 library | https://github.com/dantudose/LTR308-library |
| IMU QMI8658C | QMI8658 | https://github.com/lahavg/QMI8658 |
| Magnetometer MMC5603NJ | Adafruit MMC56x3 | https://github.com/adafruit/Adafruit_MMC56x3 |
| OLED graphics | Adafruit SSD1306 | https://github.com/adafruit/Adafruit_SSD1306 |
| OLED graphics, previous attempt | U8G2 | https://github.com/olikraus/u8g2 |

## What the current firmware covers

- ST7789 SPI TFT initialization
- HTTP web server on port 80 serving a single-page UI from flash
- Static JPEG upload from a laptop, decoded and displayed on the TFT
- MacBook webcam streaming to the TFT via HTTP POST frames
- TFT direct color test pattern
- GPIO21 buzzer piano UI and Do-Re-Mi tests
- Experimental Mac microphone pitch follower over UDP port 4210
- Experimental ES8388/I2S debug endpoints
- WiFi STA connection

## Historical: Google Sheets click logger

> This feature was removed when the project was simplified. Kept here for reference only.

Previous firmware behavior:

- Button: Button A, `GPIO0`, active low, debounced with `OneButton`.
- Local click count increments immediately on each click.
- The board sends one HTTPS JSON POST per click to a Google Apps Script Web App.
- Wi-Fi SSID/password, Apps Script Web App URL, and shared token are currently direct constants in `src/main.cpp` for fast testing.
- Upload attempts are rate-limited to once every 2 minutes to avoid sending excessive requests to Google.
- Failed uploads do not queue or retry in this version; the display shows the local count and failure/status message.

Expected Apps Script row shape:

| Column | Meaning |
|---|---|
| `server_timestamp` | Google server timestamp from Apps Script |
| `device_id` | Firmware device ID, currently `mpython-v3` |
| `button` | Button name, currently `A` |
| `local_count` | Local Button A click count |

Payload sent by firmware:

```json
{
  "token": "CHANGE_ME_SHARED_TOKEN",
  "device_id": "mpython-v3",
  "button": "A",
  "local_count": 1
}
```

## HTTP image upload / webcam stream

The firmware now runs a built-in web server on port 80 after WiFi connects. The UI is served from flash as a single minified HTML page (`src/web_page.h`).

### Endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | Web UI: file upload, webcam stream, buzzer piano, voice controls |
| `/upload` | POST | Receive a static JPEG, decode and display it on the TFT |
| `/api/frame` | POST | Receive a JPEG frame from the webcam stream and display it |
| `/api/frame/bw` | POST | Receive a packed 1-bit black-and-white frame |
| `/test/colors` | GET | Draw direct TFT color bars |
| `/test/buzzer/tone` | GET | Play a long 440 Hz tone on GPIO21 buzzer |
| `/test/buzzer/doremi` | GET | Play Do-Re-Mi on GPIO21 buzzer |
| `/api/buzzer/tone` | GET | Play a buzzer note using `freq` and `duration` query parameters |
| `/api/buzzer/stop` | GET | Stop buzzer output |
| `/api/buzzer/audio` | GET/POST | Report audio stream parameters or receive experimental raw PCM |
| `/status` | GET | JSON with IP, streamed frame count, and image-active flag |

### How it works

- The MacBook resizes the image/canvas to 320×172 before sending, so the ESP32 only decodes correctly sized JPEGs.
- The ESP32 uses `TJpg_Decoder` with an `Arduino_GFX` callback (`lcd->draw16bitRGBBitmap`) to draw tiles.
- While a stream or recent upload is active, the standby screen is hidden so the image is not overwritten.
- **Browser webcam streaming is blocked on HTTP** (except for `localhost`). The page falls back to showing a message with the Python script command. For reliable streaming, use `tools/stream_webcam.py`.

### Buzzer / voice notes

- GPIO21 is a buzzer, not a full-range speaker. It is useful for tone, piano, and Do-Re-Mi tests.
- Raw voice PCM sent to the buzzer sounds like clicks/noise and is kept only as an experiment.
- `tools/stream_voice_buzzer.py --mode pitch` detects sung pitch on the Mac and sends tone commands to UDP port `4210`.
- For real voice playback, use a confirmed ES8388 speaker output path or an external I2S amplifier and speaker.

### Libraries added

| Library | Purpose |
|---|---|
| `bodmer/TJpg_Decoder` | JPEG decode for streamed/uploaded images |
| Built-in `WebServer` | Serves the web UI and receives images |

### Source files

- `src/main.cpp` — web server routes, JPEG decoder integration, status pause logic
- `src/web_page.h` — minified HTML UI served from PROGMEM
- `src/web/index.html` — readable version of the same UI for editing

## Notes

- The LCD corners are filleted, so the extreme corner pixels are not fully visible on the physical panel.
- The current LCD orientation was verified after switching from `Adafruit_ST7789` to `Arduino_GFX`.
- macOS can re-enumerate the board as a different `/dev/cu.usbmodem...` device. The most recent successful upload used `/dev/cu.usbmodem21301`, while `platformio.ini` still has `/dev/cu.usbmodem11401`.
