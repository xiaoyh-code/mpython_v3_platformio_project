# mPython ESP32-S3 V3 Board Test Guide

This guide verifies the current firmware on the physical mPython ESP32-S3 V3 board.

## 1. Prepare the Project

Install PlatformIO and open this folder:

```bash
cd /Users/yuhuaxiao/Documents/PlatformIO/Projects/mpython_v3_platformio_project
```

Check the board serial port:

```bash
pio device list
```

Important: the port committed in `platformio.ini` is only an example from the last tested Mac. Before uploading, update both `upload_port` and `monitor_port` to your own board port from `pio device list`.

```ini
upload_port = /dev/cu.usbmodemXXXX
monitor_port = /dev/cu.usbmodemXXXX
```

If the board appears as a different `/dev/cu.usbmodem...` path later, update both lines again.

Create the local WiFi config before building:

```bash
cp src/wifi_config.example.h src/wifi_config.h
```

Then edit `src/wifi_config.h` with the WiFi SSID and password for the test network. This file is ignored by git and should not be uploaded to GitHub.

## 2. Build and Upload

```bash
pio run
pio run --target upload
pio device monitor
```

Expected result:

- Build succeeds.
- Upload succeeds with `--no-stub`.
- Serial monitor prints the ESP32 IP address.
- TFT shows the standby screen with the board URL.

## 3. Open the Web UI

Open:

```text
http://<board-ip>/
```

For the current local test network, the board has often been:

```text
http://10.1.14.249/
```

Expected result:

- The page loads.
- It shows sections for static upload, webcam stream, buzzer piano, and voice-to-buzzer.

## 4. TFT Color Test

Use the web UI `TFT Color Test` button, or run:

```bash
curl http://<board-ip>/test/colors
```

Expected color order on the TFT:

```text
white, red, green, blue, black, gray
```

If red and blue are swapped, check the ST7789 BGR MADCTL setting and `TJpgDec.setSwapBytes(false)` in `src/main.cpp`.

## 5. Static JPEG Upload

From the web UI:

1. Choose a JPEG file.
2. Click `Upload to TFT`.

Expected result:

- The image is resized to 320 x 172 in the browser.
- The TFT displays the image with natural colors.

Python pattern test:

```bash
.venv/bin/python tools/upload_test_pattern.py <board-ip>
```

Expected pattern:

```text
white, red, green, blue, black, gray
```

The grayscale gradient should look neutral, not tinted.

## 6. Webcam Stream

Fast black-and-white stream:

```bash
.venv/bin/python tools/stream_webcam.py <board-ip>
```

Color JPEG stream:

```bash
.venv/bin/python tools/stream_webcam.py <board-ip> --color
```

Expected result:

- Black-and-white mode is faster and stable.
- Color mode is slower but should have natural color.

Browser camera streaming may fail on `http://<ip>/` because browsers restrict camera access on insecure origins. Use the Python script when that happens.

## 7. Buzzer Tests

Web UI:

- Press piano keys in the `Buzzer Piano` section.
- Press `Do Re Mi`.

Python helpers:

```bash
.venv/bin/python tools/test_buzzer_tone.py <board-ip>
.venv/bin/python tools/test_buzzer_doremi.py <board-ip>
```

Expected result:

- GPIO21 buzzer plays clear beeps.
- Do-Re-Mi is audible as three different pitches.

## 8. Voice-To-Buzzer Experiment

Pitch-following mode:

```bash
.venv/bin/python tools/stream_voice_buzzer.py <board-ip> --mode pitch --threshold 0.006 --min-freq 120 --max-freq 550
```

How to test:

1. Put the board farther away from the Mac microphone to reduce feedback.
2. Put your mouth closer to the Mac microphone.
3. Sing long notes, for example `Do -- Re -- Mi --`.
4. Watch the terminal for lines such as `262 Hz C4`, `294 Hz D4`, or `330 Hz E4`.

Expected result:

- The terminal shows detected pitch values when it hears a stable sung note.
- The buzzer responds with short beeps near the detected pitch.

Known limitation:

- GPIO21 is a buzzer, not a speaker. Raw voice streaming will sound like clicks or harsh electronic noise.
- For real voice playback, use the ES8388/speaker path or an external I2S amplifier and speaker.

Raw PCM mode is kept only for experiments:

```bash
.venv/bin/python tools/stream_voice_buzzer.py <board-ip> --mode pcm
```

## 9. ES8388 / I2S Debug Tests

These endpoints are experimental and were used to investigate the onboard ES8388 codec:

```bash
.venv/bin/python tools/audio_debug.py <board-ip>
.venv/bin/python tools/test_tone.py <board-ip>
.venv/bin/python tools/test_doremi.py <board-ip>
.venv/bin/python tools/test_tone_lj.py <board-ip>
.venv/bin/python tools/test_tone_square.py <board-ip>
```

Observed state:

- ES8388 I2C register writes can succeed.
- Audible output through the codec path was tiny/noisy during testing.
- The confirmed working audible output is the GPIO21 buzzer.

## 10. Button B

When an image or stream is active, press Button B.

Expected result:

- The current TFT image clears.
- The standby screen returns.

## 11. Troubleshooting

No upload port:

- Reconnect USB.
- Run `pio device list`.
- Update `upload_port` and `monitor_port`.

Cannot open web page:

- Confirm the board and computer are on the same WiFi.
- Check the serial monitor for the current IP.
- Try `ping <board-ip>`.

Wrong TFT colors:

- Run `/test/colors` first.
- Then run `tools/upload_test_pattern.py`.
- Confirm JPEG decoder uses `TJpgDec.setSwapBytes(false)`.

Voice only clicks:

- This is expected for raw PCM on the buzzer.
- Use `--mode pitch` for note-following.
- Use a real speaker path for voice playback.
