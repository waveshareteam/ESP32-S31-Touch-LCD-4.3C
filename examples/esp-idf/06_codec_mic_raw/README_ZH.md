# Codec Microphone Raw Capture

[English](README.md)

## 功能

通过托管 ES8389 音频 ADC 采集 48 kHz、16 位立体声麦克风数据。

## 硬件需求

带双板载麦克风的开发板及 USB 数据线。

## 怎么运行

先按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备固定版本工具链、解析依赖并生成本示例的板级配置，然后编译烧录。

## 预期现象

串口输出左右声道峰值及每个采样块的前几组数据。

## 常见问题

对比安静环境与靠近各麦克风说话时的数据；综合测试中使用的 ES8389 32fs 时钟修正见 [13 示例](../13_comprehensive_test/README_ZH.md)。
