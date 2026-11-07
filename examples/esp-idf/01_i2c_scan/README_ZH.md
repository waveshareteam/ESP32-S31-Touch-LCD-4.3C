# I2C Scan

[English](README.md)

## 功能

唤醒 GT911 后扫描 Board Manager 管理的共享 I2C 总线。

## 硬件需求

开发板及 USB 数据线，无需外接 I2C 器件。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

串口列出响应的 7 位地址，并输出总线电平和探测错误。

## 常见问题

常见地址为触摸 0x14/0x5D、RTC 0x51、IMU 0x6A/0x6B、电量计 0x55、音频编解码器 0x10；电量计需要连接电池供电。
