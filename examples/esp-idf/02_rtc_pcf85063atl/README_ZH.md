# PCF85063ATL RTC

[English](README.md)

## 功能

通过托管 I2C 总线读取 RTC 时间及其 1 Hz 定时中断。

## 硬件需求

开发板及 USB 数据线。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## 预期现象

串口输出日期、时间、星期、时间有效标志及中断计数。

## 常见问题

RTC 时间无效时，示例将其设为 2026-01-01 12:00:00；这是默认值，不是网络对时。
