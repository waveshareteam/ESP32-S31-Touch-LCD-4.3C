# USB Camera

[简体中文](README_ZH.md)

## What It Does

Receives MJPEG video from an external USB UVC camera, decodes it and draws the preview on the LCD.

## Hardware Required

Board, USB data cable and a USB UVC camera supporting MJPEG at a compatible resolution.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

A 640 × 480 camera image is centered on the 800 × 480 screen when that mode is available; serial logs report the selected stream and preview rate.

## Troubleshooting

If the preferred mode fails, the example tries additional frame rates at 640 × 480. The camera is external; the board has no DVP camera interface.
