# RS485

[简体中文](README_ZH.md)

## What It Does

Sends a counter string and logs incoming bytes through the managed half-duplex RS485 UART.

## Hardware Required

Board, USB data cable and an RS485 peer with matching wiring and baud rate.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

The peer receives increasing counter values; replies appear in the serial monitor.

## Troubleshooting

Match the board definition’s 115200 baud setting and connect a common reference ground. This example requires an external RS485 peer for receive testing.
