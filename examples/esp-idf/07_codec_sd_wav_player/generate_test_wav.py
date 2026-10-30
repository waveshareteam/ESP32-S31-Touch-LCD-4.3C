#!/usr/bin/env python3
"""Generate an original stereo diagnostic signal for the WAV player example."""

from array import array
import math
from pathlib import Path
import sys
import wave


SAMPLE_RATE = 48_000
CHANNELS = 2
SAMPLE_WIDTH_BYTES = 2
DURATION_SECONDS = 6.0
FADE_SECONDS = 0.02


def envelope(position: float, duration: float) -> float:
    return min(1.0, position / FADE_SECONDS, (duration - position) / FADE_SECONDS)


def sine(frequency: float, position: float, duration: float, amplitude: float) -> float:
    return amplitude * envelope(position, duration) * math.sin(2.0 * math.pi * frequency * position)


def sweep(start_hz: float, end_hz: float, position: float, duration: float, amplitude: float) -> float:
    rate = (end_hz - start_hz) / duration
    phase = 2.0 * math.pi * (start_hz * position + 0.5 * rate * position * position)
    return amplitude * envelope(position, duration) * math.sin(phase)


def generate(output: Path) -> None:
    frames = array("h")
    frame_count = round(SAMPLE_RATE * DURATION_SECONDS)

    for frame in range(frame_count):
        time_s = frame / SAMPLE_RATE
        left = 0.0
        right = 0.0

        if 0.25 <= time_s < 1.25:
            left = sine(440.0, time_s - 0.25, 1.0, 0.28)
        elif 1.50 <= time_s < 2.50:
            right = sine(660.0, time_s - 1.50, 1.0, 0.28)
        elif 2.75 <= time_s < 5.75:
            position = time_s - 2.75
            left = sweep(300.0, 1_200.0, position, 3.0, 0.22)
            right = sweep(1_200.0, 300.0, position, 3.0, 0.22)

        frames.append(round(max(-1.0, min(1.0, left)) * 32767))
        frames.append(round(max(-1.0, min(1.0, right)) * 32767))

    if sys.byteorder != "little":
        frames.byteswap()

    with wave.open(str(output), "wb") as wav:
        wav.setnchannels(CHANNELS)
        wav.setsampwidth(SAMPLE_WIDTH_BYTES)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(frames.tobytes())


if __name__ == "__main__":
    destination = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("test.wav")
    generate(destination)
    print(destination)
