# I2C Scan

[简体中文](README_ZH.md)

## What It Does

Scans the shared Board Manager I2C bus after applying the GT911 wake-up sequence.

## Hardware Required

Board and USB data cable; no external I2C device is required.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The serial monitor lists responding 7-bit addresses and reports bus levels and probe errors.

## Troubleshooting

Typical addresses are touch 0x14/0x5D, RTC 0x51, IMU 0x6A/0x6B, battery gauge 0x55 and codec 0x10. The battery gauge needs a connected battery.
