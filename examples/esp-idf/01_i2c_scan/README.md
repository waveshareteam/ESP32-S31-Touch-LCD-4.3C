# I2C Scan

[简体中文](README_ZH.md)

## What It Does

Scans the shared Board Manager I2C bus after applying the GT911 wake-up sequence.

## Hardware Required

Board and USB data cable; no external I2C device is required.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor lists responding 7-bit addresses and reports bus levels and probe errors.

## Troubleshooting

Typical addresses are touch 0x14/0x5D, RTC 0x51, IMU 0x6A/0x6B, battery gauge 0x55 and codec 0x10. The battery gauge needs a connected battery.
