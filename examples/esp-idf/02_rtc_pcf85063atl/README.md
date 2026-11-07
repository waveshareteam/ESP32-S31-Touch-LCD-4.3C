# PCF85063ATL RTC

[简体中文](README_ZH.md)

## What It Does

Reads the RTC and its 1 Hz timer interrupt using the managed I2C bus.

## Hardware Required

Board and USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The serial monitor prints date, time, weekday, validity flag and interrupt count.

## Troubleshooting

If the RTC time is invalid, the example initializes it to 2026-01-01 12:00:00; this is a fallback value, not network time synchronization.
