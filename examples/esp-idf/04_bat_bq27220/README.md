# BQ27220 Battery Gauge

[简体中文](README_ZH.md)

## What It Does

Periodically reads battery voltage, current, temperature, capacity and charge/health status.

## Hardware Required

Board, USB data cable and a compatible battery connected to the battery socket.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

The serial monitor displays battery measurements; initialization failures are retried every two seconds.

## Troubleshooting

The BQ27220 is powered by the battery, so USB power alone does not make it respond. The 650 mAh design value in the configuration does not limit the board to that battery capacity.
