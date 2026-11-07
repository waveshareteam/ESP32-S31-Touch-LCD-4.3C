# TF Card

[English](README.md)

## 功能

通过 4-bit SDMMC 挂载 TF 卡，写入并读回测试文件，循环列出根目录。

## 硬件需求

开发板、USB 数据线及 FAT 格式的 TF 卡。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

串口输出文件读回内容及根目录列表。

## 常见问题

请使用有可写空间的卡，并在运行写入测试前备份重要文件；卡供电及 4-bit 总线由托管板级定义配置。
