# USB Host

[简体中文](README_ZH.md)

## What It Does

Enumerates USB devices and logs connection, disconnection and descriptor information.

## Hardware Required

Board, USB data cable and a USB device connected to the board’s host interface.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

The serial monitor shows device speed, configuration, VID, PID and class after connection.

## Troubleshooting

Host VBUS comes directly from the board’s 5 V rail, so provide suitable board power. This example enumerates devices; it does not implement every USB class application.
