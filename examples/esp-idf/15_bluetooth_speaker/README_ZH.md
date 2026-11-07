# Bluetooth Speaker

[English](README.md)

## 功能

接收经典蓝牙 A2DP 音频，通过 ES8389 播放，并在 LVGL 播放界面显示 AVRCP 曲目信息。

## 硬件需求

开发板、USB 数据线、扬声器及手机等 A2DP 音源。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

配对 ESP_SPEAKER 并播放音乐，扬声器输出音频，屏幕显示音源提供的曲目信息。

## 常见问题

默认内部解码器使用 SBC，接收端同时只连接一个音源；曲目信息取决于音源是否提供。
