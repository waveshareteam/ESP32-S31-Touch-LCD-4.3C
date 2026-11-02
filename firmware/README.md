# Firmware

[English](README.md) | [简体中文](README_ZH.md)

Factory firmware tested on the ESP32-S31-Touch-LCD-4.3C board; flash the merged image at `0x0` to restore the out-of-box demo. Its source is not yet included in this repository.

| File | Purpose |
| --- | --- |
| [ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin](ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin) | Factory firmware, restores the out-of-box demo |

From this directory, use esptool with `esp32s31` support and replace `PORT` with the device port; back up device data before erasing and flashing.

```bash
python -m pip install --upgrade esptool
python -m esptool --chip esp32s31 --port PORT --no-stub erase-flash
python -m esptool --chip esp32s31 --port PORT --no-stub write-flash 0x0 ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin
```
