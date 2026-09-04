#!/usr/bin/env python3
import argparse
import socket
import sys
import time

import requests

try:
    import numpy as np
    import sounddevice as sd
except ImportError:
    np = None
    sd = None


SAMPLE_RATE = 8000
CHUNK_SAMPLES = 320
UDP_PORT = 4210
PITCH_WINDOW = 1024
NOTE_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


def pcm8_from_float32(samples, gain: float) -> bytes:
    mono = np.asarray(samples, dtype=np.float32).reshape(-1)
    mono = np.clip(mono * gain, -1.0, 1.0)
    return np.rint((mono * 0.5 + 0.5) * 255.0).astype(np.uint8).tobytes()


def note_name(freq: float) -> str:
    midi = int(round(69 + 12 * np.log2(freq / 440.0)))
    return f"{NOTE_NAMES[midi % 12]}{midi // 12 - 1}"


def snap_to_semitone(freq: float) -> float:
    midi = round(69 + 12 * np.log2(freq / 440.0))
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def estimate_pitch(samples, sample_rate: int, min_freq: float, max_freq: float, threshold: float):
    mono = np.asarray(samples, dtype=np.float32).reshape(-1)
    mono = mono - float(np.mean(mono))
    rms = float(np.sqrt(np.mean(mono * mono)))
    if rms < threshold:
        return None, rms, 0.0

    min_lag = max(1, int(sample_rate / max_freq))
    max_lag = min(len(mono) // 2, int(sample_rate / min_freq))
    if max_lag <= min_lag:
        return None, rms, 0.0

    diff = np.zeros(max_lag + 1, dtype=np.float32)
    for lag in range(1, max_lag + 1):
        delta = mono[:-lag] - mono[lag:]
        diff[lag] = float(np.dot(delta, delta))

    cumulative = np.cumsum(diff[1:])
    cmnd = np.ones(max_lag + 1, dtype=np.float32)
    lags = np.arange(1, max_lag + 1, dtype=np.float32)
    cmnd[1:] = diff[1:] * lags / np.maximum(cumulative, 1e-9)

    lag = None
    yin_threshold = 0.32
    for candidate in range(min_lag, max_lag - 1):
        if cmnd[candidate] < yin_threshold and cmnd[candidate] <= cmnd[candidate + 1]:
            lag = candidate
            break
    if lag is None:
        lag = int(np.argmin(cmnd[min_lag:max_lag])) + min_lag

    confidence = float(1.0 - cmnd[lag])
    if confidence < 0.45:
        return None, rms, confidence

    return sample_rate / lag, rms, confidence


def tone_packet(freq: float) -> bytes:
    hz = int(max(80, min(3000, round(freq))))
    return b"T" + bytes([(hz >> 8) & 0xFF, hz & 0xFF])


def main() -> int:
    parser = argparse.ArgumentParser(description="Stream Mac microphone audio to the GPIO21 buzzer.")
    parser.add_argument("host", help="ESP32 IP address, for example 10.1.14.249")
    parser.add_argument("--gain", type=float, default=2.4, help="microphone gain before 8-bit conversion")
    parser.add_argument("--device", help="optional sounddevice input device name or index")
    parser.add_argument("--mode", choices=("pitch", "pcm", "http"), default="pitch", help="pitch follows sung notes; pcm/http try raw voice")
    parser.add_argument("--threshold", type=float, default=0.018, help="mic RMS threshold for pitch mode")
    parser.add_argument("--min-freq", type=float, default=120.0, help="lowest detected voice pitch in Hz")
    parser.add_argument("--max-freq", type=float, default=900.0, help="highest detected voice pitch in Hz")
    parser.add_argument("--no-snap", action="store_true", help="do not snap detected pitch to nearest piano note")
    parser.add_argument("--continuous", action="store_true", help="hold the buzzer tone continuously in pitch mode")
    parser.add_argument("--pulse-ms", type=float, default=85.0, help="short buzzer response length in pitch mode")
    parser.add_argument("--gap-ms", type=float, default=140.0, help="quiet gap after each pitch response")
    parser.add_argument("--http", action="store_true", help="use the older HTTP streaming endpoint instead of UDP")
    args = parser.parse_args()
    if args.http:
        args.mode = "http"

    if sd is None:
        print("Missing dependency: sounddevice")
        print("Install it with: python3 -m pip install sounddevice numpy requests")
        return 1

    url = f"http://{args.host}/api/buzzer/audio"
    udp_addr = (args.host, UDP_PORT)
    target = "HTTP " + url if args.mode == "http" else "UDP " + args.host + ":" + str(UDP_PORT)
    print(f"Streaming microphone to {target} in {args.mode} mode")
    print("Press Ctrl+C to stop.")

    session = requests.Session()
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    last_print = time.monotonic()
    chunks = 0
    latest_pitch = None
    latest_note = ""
    voiced = 0
    beeps = 0
    tone_off_at = 0.0
    listen_again_at = 0.0
    pitch_buffer = np.zeros(PITCH_WINDOW, dtype=np.float32)

    try:
        with sd.InputStream(
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype="float32",
            blocksize=CHUNK_SAMPLES,
            device=args.device,
        ) as stream:
            while True:
                samples, overflowed = stream.read(CHUNK_SAMPLES)
                now = time.monotonic()
                flat_samples = np.asarray(samples, dtype=np.float32).reshape(-1)
                pitch_buffer = np.roll(pitch_buffer, -len(flat_samples))
                pitch_buffer[-len(flat_samples) :] = flat_samples
                if tone_off_at and now >= tone_off_at:
                    udp.sendto(b"S", udp_addr)
                    tone_off_at = 0.0
                    listen_again_at = now + args.gap_ms / 1000.0
                    pitch_buffer.fill(0.0)

                if args.mode == "pitch":
                    if not args.continuous and now < listen_again_at:
                        pitch = None
                    else:
                        pitch, _rms, _confidence = estimate_pitch(
                            pitch_buffer,
                            SAMPLE_RATE,
                            args.min_freq,
                            args.max_freq,
                            args.threshold,
                        )
                    if pitch is None:
                        if args.continuous:
                            udp.sendto(b"S", udp_addr)
                        latest_pitch = None
                        latest_note = ""
                    else:
                        if not args.no_snap:
                            pitch = snap_to_semitone(pitch)
                        latest_pitch = pitch
                        latest_note = note_name(pitch)
                        udp.sendto(tone_packet(pitch), udp_addr)
                        voiced += 1
                        if not args.continuous:
                            beeps += 1
                            tone_off_at = time.monotonic() + args.pulse_ms / 1000.0
                            listen_again_at = tone_off_at + args.gap_ms / 1000.0
                else:
                    body = pcm8_from_float32(samples, args.gain)
                    if args.mode == "http":
                        response = session.post(
                            url,
                            data=body,
                            headers={"Content-Type": "application/octet-stream"},
                            timeout=2,
                        )
                        response.raise_for_status()
                    else:
                        udp.sendto(body, udp_addr)
                chunks += 1
                if now - last_print >= 1.0:
                    suffix = " input overflow" if overflowed else ""
                    if latest_pitch is None:
                        pitch_text = "no pitch"
                    else:
                        pitch_text = f"{latest_pitch:.0f} Hz {latest_note}"
                    print(f"{chunks} chunks sent, {voiced} voiced, {beeps} beeps, {pitch_text}{suffix}")
                    chunks = 0
                    voiced = 0
                    beeps = 0
                    last_print = now
    except KeyboardInterrupt:
        try:
            session.get(f"http://{args.host}/api/buzzer/stop", timeout=1)
        except requests.RequestException:
            pass
        print("\nStopped")
        return 0
    except Exception as exc:
        print(f"Voice stream failed: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
