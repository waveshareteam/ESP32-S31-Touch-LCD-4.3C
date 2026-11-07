# RS485

[English](README.md)

## 功能

通过托管半双工 RS485 UART 发送计数字符串，并记录接收的数据。

## 硬件需求

开发板、USB 数据线及接线和波特率匹配的 RS485 对端。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

对端收到递增计数值；对端回复显示在开发板串口中。

## 常见问题

对端需匹配板级定义中的 115200 波特率并连接参考地；接收测试需要外接 RS485 对端。
