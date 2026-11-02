# USB Camera

[简体中文](README_ZH.md)

## What It Does

Receives MJPEG video from an external USB UVC camera, decodes it and draws the preview on the LCD.

## Hardware Required

Board, USB data cable and a USB UVC camera supporting MJPEG at a compatible resolution.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

A 640 × 480 camera image is centered on the 800 × 480 screen when that mode is available; serial logs report the selected stream and preview rate.

## Troubleshooting

If the preferred mode fails, the example tries additional frame rates at 640 × 480. The camera is external; the board has no DVP camera interface.
