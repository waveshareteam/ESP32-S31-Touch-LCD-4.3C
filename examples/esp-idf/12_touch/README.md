# Touch

[简体中文](README_ZH.md)

## What It Does

Reads GT911 touch points and draws colored marks on the LCD.

## Hardware Required

Board with its LCD/touch assembly attached and a USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

Touching the screen draws marks; touching the top-right block clears the page.

## Troubleshooting

None yet.
