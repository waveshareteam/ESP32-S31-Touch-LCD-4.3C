# Codec SD WAV Player

[简体中文](README_ZH.md)

## What It Does

Plays a 16-bit PCM WAV file from the TF card through the ES8389; mono audio is converted to stereo.

## Hardware Required

Board, USB data cable, FAT-formatted TF card and a speaker connected to the speaker output.

## How to Run

Prepare the environment using the [ESP-IDF instructions](../README.md#build-and-flash), then run these commands in this example directory and replace `PORT` with the device port.

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## Expected Result

The speaker repeatedly plays test.wav, or the first WAV file found in the card root.

## Troubleshooting

Copy the included test.wav to the card root before starting; its diagnostic tones and stereo sweeps are provided under [CC0-1.0](test.wav.LICENSE.txt) and can be reproduced with generate_test_wav.py. Compressed audio and non-16-bit PCM files are rejected.
