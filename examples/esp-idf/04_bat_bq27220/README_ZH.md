# BQ27220 Battery Gauge

[English](README.md)

## 功能

周期读取电池电压、电流、温度、容量、电量及健康状态。

## 硬件需求

开发板、USB 数据线及接入电池接口的适配电池。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

串口显示电池测量数据；初始化失败时每两秒重试。

## 常见问题

BQ27220 由电池供电，仅连接 USB 时可能无法响应；配置中的 650 mAh 设计值不代表开发板只能使用该容量的电池。
