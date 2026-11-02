# CAN-FD

[English](README.md)

## 功能

发送标识符为 0x123 的 16 字节 CAN-FD 计数帧，并记录接收帧。

## 硬件需求

开发板、USB 数据线及接在正确终端匹配总线上的活动 CAN-FD 对端。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## 预期现象

串口输出发送计数及接收帧的标识符、长度和数据。

## 常见问题

对端需匹配托管板级定义：仲裁段 500 kbit/s、数据段 2 Mbit/s，并能正常应答帧。
