# PCF85063ATL RTC

[简体中文](README_ZH.md)

## What It Does

Reads the RTC and its 1 Hz timer interrupt using the managed I2C bus.

## Hardware Required

Board and USB data cable.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor prints date, time, weekday, validity flag and interrupt count.

## Troubleshooting

If the RTC time is invalid, the example initializes it to 2026-01-01 12:00:00; this is a fallback value, not network time synchronization.
