# RGB LCD

[简体中文](README_ZH.md)

## What It Does

Draws test patterns using two RGB framebuffers in PSRAM and full-frame swaps.

## Hardware Required

Board with its LCD attached and a USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The 800 × 480 screen cycles through solid colors, color bars, checkerboards, gradients and geometry.

## Troubleshooting

None yet.
