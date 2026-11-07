# USB Host

[English](README.md)

## 功能

枚举 USB 设备，记录连接、断开及描述符信息。

## 硬件需求

开发板、USB 数据线及接入开发板 Host 接口的 USB 设备。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

连接后串口显示设备速度、配置、VID、PID 和类别。

## 常见问题

Host VBUS 直接来自开发板 5 V 电源，请确保供电正常；本示例进行设备枚举，不包含所有 USB 类应用。
