# Comprehensive Test

[English](README.md)

## 功能

运行 LVGL 仪表盘，自动及手动测试显示、触摸、传感器、存储、音频、Wi-Fi、RS485、CAN-FD 和 USB Host。

## 硬件需求

开发板及 USB 数据线；相应测试还需电池、FAT 格式 TF 卡、扬声器、USB 设备及兼容的 RS485/CAN-FD 转换器。

## 怎么运行

先按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备固定版本工具链、解析依赖并生成本示例的板级配置，然后编译烧录。

## 预期现象

LCD 显示测试结果及左右麦克风电平，串口输出汇总及 MIC activity L/R；可在 menuconfig 选择仅 USB Host 模式。

## 常见问题

麦克风路径针对 esp_codec_dev 2.0.0-beta5 检查并修正 ES8389 时钟寄存器，采用 48 kHz、16 位立体声及 32fs BCLK（1.536 MHz），升级驱动时需重新评估该修正；桥接测试要求转换器使用 69 字节串口帧和 64 字节 CAN-FD 载荷，普通回显设备不能替代。
