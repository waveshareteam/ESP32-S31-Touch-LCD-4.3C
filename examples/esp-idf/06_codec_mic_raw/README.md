# Codec Microphone Raw Capture

[简体中文](README_ZH.md)

## What It Does

Captures 48 kHz, 16-bit stereo samples through the managed ES8389 audio ADC.

## Hardware Required

Board with its two onboard microphones and a USB data cable.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The serial monitor prints left/right peaks and the first sample pairs of each captured block.

## Troubleshooting

Compare quiet conditions with speech near each microphone. For the guarded ES8389 32fs clock correction used by the integrated test, see [example 13](../13_comprehensive_test/); the current Board Manager audio path uses esp_codec_dev 2.x APIs and cannot directly switch to the 1.x stable driver.
