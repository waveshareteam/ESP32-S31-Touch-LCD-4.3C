# Codec Microphone Raw Capture

[简体中文](README_ZH.md)

## What It Does

Captures 48 kHz, 16-bit stereo samples through the managed ES8389 audio ADC.

## Hardware Required

Board with its two onboard microphones and a USB data cable.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor prints left/right peaks and the first sample pairs of each captured block.

## Troubleshooting

Compare quiet conditions with speech near each microphone. For the guarded ES8389 32fs clock correction used by the integrated test, see [example 13](../13_comprehensive_test/).
