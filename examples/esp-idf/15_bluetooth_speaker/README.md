# Bluetooth Speaker

[简体中文](README_ZH.md)

## What It Does

Receives Classic Bluetooth A2DP audio, plays it through the ES8389 and shows AVRCP track information on an LVGL player.

## Hardware Required

Board, USB data cable, speaker and a phone or other A2DP source.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

Pair with ESP_SPEAKER and play music; the speaker outputs audio and the display shows available track information.

## Troubleshooting

The default internal decoder uses SBC, and the sink accepts one source connection at a time. Track metadata depends on the connected source.
