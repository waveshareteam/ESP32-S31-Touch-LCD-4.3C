# LVGL Porting

[简体中文](README_ZH.md)

## What It Does

Connects the managed LCD, touch and backlight to esp_lvgl_adapter and runs the bundled UI/media demonstration.

## Hardware Required

Board with its LCD/touch assembly attached and a USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## Expected Result

The LCD displays the LVGL demonstration and responds to touch input.

## Troubleshooting

The manifest pins esp_lvgl_adapter to 0.6.1 for DMA API detection on ESP-IDF v6.1, with LVGL 9.5.0; check compatibility before changing these versions.
