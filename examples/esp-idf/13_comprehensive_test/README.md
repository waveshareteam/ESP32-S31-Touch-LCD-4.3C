# Comprehensive Test

[简体中文](README_ZH.md)

## What It Does

Runs the LVGL dashboard with automatic and manual tests for display, touch, sensors, storage, audio, Wi-Fi, RS485, CAN-FD and USB Host.

## Hardware Required

Board and USB data cable; add a battery, FAT-formatted TF card, speaker, USB device and compatible RS485/CAN-FD converter for the corresponding tests.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The LCD displays test results and both microphone meters; serial logs include the summary and MIC activity L/R. USB Host only mode is selectable in menuconfig.

## Troubleshooting

The microphone path checks and corrects ES8389 clock registers for 48 kHz, 16-bit stereo, 32fs BCLK (1.536 MHz) with esp_codec_dev 2.0.0-beta5; reassess this workaround when upgrading the driver. The bridge test expects the converter’s 69-byte serial framing and 64-byte CAN-FD payload, so a generic echo device is insufficient.
