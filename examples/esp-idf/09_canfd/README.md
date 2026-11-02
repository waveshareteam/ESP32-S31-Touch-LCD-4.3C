# CAN-FD

[简体中文](README_ZH.md)

## What It Does

Sends 16-byte CAN-FD counter frames with identifier 0x123 and logs received frames.

## Hardware Required

Board, USB data cable and an active CAN-FD peer on a correctly terminated bus.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

The serial monitor prints transmit counters and received frame identifiers, lengths and payloads.

## Troubleshooting

Match the managed board definition: 500 kbit/s arbitration and 2 Mbit/s data phase. A compatible active peer is needed to acknowledge frames.
