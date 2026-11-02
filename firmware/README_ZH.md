# 固件

[English](README.md) | [简体中文](README_ZH.md)

在 ESP32-S31-Touch-LCD-4.3C 开发板上实测过的出厂固件，从 `0x0` 烧录合并镜像即可恢复开箱演示。源码暂未包含在本仓库中。

| 文件 | 用途 |
| --- | --- |
| [ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin](ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin) | 出厂固件，恢复开箱演示 |

在本目录使用支持 `esp32s31` 的 esptool；将 `PORT` 替换为设备端口，先备份数据，再擦除并烧录。

```bash
python -m pip install --upgrade esptool
python -m esptool --chip esp32s31 --port PORT --no-stub erase-flash
python -m esptool --chip esp32s31 --port PORT --no-stub write-flash 0x0 ESP32-S31-Touch-LCD-4.3C_FactoryOnly_20261030.bin
```
