# LVGL Porting

[简体中文](README_ZH.md)

## What It Does

Connects the managed LCD, touch and backlight to esp_lvgl_adapter and runs the bundled UI/media demonstration.

## Hardware Required

Board with its LCD/touch assembly attached and a USB data cable.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The LCD displays the LVGL demonstration and responds to touch input.

## Troubleshooting

The manifest pins esp_lvgl_adapter to 0.6.0 and LVGL to 9.5.0; upgrade them together only after checking compatibility.
