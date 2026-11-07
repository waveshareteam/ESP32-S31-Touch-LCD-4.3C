# QMI8658 IMU

[English](README.md)

## 功能

读取 QMI8658 的器件标识、温度、加速度、角速度及中断状态。

## 硬件需求

开发板及 USB 数据线。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

倾斜或转动开发板时，串口中的传感器读数随之变化。

## 常见问题

暂无。
