# QMI8658 IMU

[简体中文](README_ZH.md)

## What It Does

Reads QMI8658 identity, temperature, accelerometer, gyroscope and interrupt status.

## Hardware Required

Board and USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

Serial readings change as the board is tilted or rotated.

## Troubleshooting

None yet.
