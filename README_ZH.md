<div align="center">
  <h1>ESP32-S31-Touch-LCD-4.3C</h1>
  <p><strong>ESP32-S31 4.3 英寸 800 × 480 RGB 电容触摸开发板</strong></p>
  <p><a href="README.md">English</a> | <strong>简体中文</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-S31-Touch-LCD-4.3C/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-S31-Touch-LCD-4.3C/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-S31-Touch-LCD-4.3C/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-S31-Touch-LCD-4.3C"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-S31-Touch-LCD-4.3C"></a>
  </p>
  <p>
    <a href="https://www.waveshare.net/shop/ESP32-S31-Touch-LCD-4.3C.htm">🌐 产品</a> &middot;
    <a href="https://github.com/waveshareteam/ESP32-S31-Touch-LCD-4.3C/releases/latest">📦 固件</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="hardware/">📚 资料</a>
  </p>
  <p><img src="docs/images/Product-1.webp" alt="ESP32-S31-Touch-LCD-4.3C development board" width="500"></p>
</div>

---

## ✨ 概述

本开发板集成 4.3 英寸触摸屏、双麦克风音频、运动传感器、电池监测、TF 卡、RTC 及工业通信接口，适合人机交互与外设应用开发。本仓库提供 16 个 ESP-IDF 示例、固件及硬件资料。

| 功能 | 器件 / 接口 |
| --- | --- |
| 主控 | ESP32-S31，双核 RISC-V，最高 320 MHz |
| 存储器 | 16 MB Flash、16 MB PSRAM，模组 ESP32-S31-WROOM-3-N16R16 |
| 显示 | 4.3 英寸，800 × 480，16 位 RGB565 |
| 触摸 | GT911，I2C 电容触摸 |
| 音频 | ES8389、双麦克风及扬声器功放 |
| 传感器 | QMI8658 IMU、BQ27220 电量计、PCF85063ATL RTC |
| 外设 | 4-bit SDMMC TF 卡、RS485、CAN-FD、USB Host |
| 板级支持 | 托管组件 waveshare/waveshare_boards |

## 📦 快速开始

1. 从 [最新 Release](https://github.com/waveshareteam/ESP32-S31-Touch-LCD-4.3C/releases/latest) 下载所需示例的固件 ZIP。
2. 解压压缩包，准备支持 esp32s31 的 esptool 环境。
3. 通过 USB 连接开发板。
4. 在 Bash 中执行 `./flash.sh PORT`，或在 Windows PowerShell 中执行 `.\flash.bat PORT`，将 PORT 替换为设备端口。
5. 如未自动启动，复位开发板，并按对应示例说明连接外设。

随附的 [全 Flash 测试镜像](firmware/README_ZH.md) 位于 firmware 目录，烧录前请备份设备数据。

## 🧪 示例

### ESP-IDF

| 示例 | 功能 |
| --- | --- |
| [01_i2c_scan](examples/esp-idf/01_i2c_scan/README_ZH.md) | I2C 总线扫描 |
| [02_rtc_pcf85063atl](examples/esp-idf/02_rtc_pcf85063atl/README_ZH.md) | RTC 时间及中断 |
| [03_imu_qmi8658](examples/esp-idf/03_imu_qmi8658/README_ZH.md) | 加速度及角速度 |
| [04_bat_bq27220](examples/esp-idf/04_bat_bq27220/README_ZH.md) | 电池监测 |
| [05_tf_card](examples/esp-idf/05_tf_card/README_ZH.md) | 4-bit SDMMC 文件测试 |
| [06_codec_mic_raw](examples/esp-idf/06_codec_mic_raw/README_ZH.md) | 双声道麦克风采样 |
| [07_codec_sd_wav_player](examples/esp-idf/07_codec_sd_wav_player/README_ZH.md) | TF 卡 WAV 播放 |
| [08_rs485](examples/esp-idf/08_rs485/README_ZH.md) | 半双工 RS485 |
| [09_canfd](examples/esp-idf/09_canfd/README_ZH.md) | CAN-FD 收发 |
| [10_usb_host](examples/esp-idf/10_usb_host/README_ZH.md) | USB 设备枚举 |
| [11_rgb_lcd](examples/esp-idf/11_rgb_lcd/README_ZH.md) | RGB LCD 测试图案 |
| [12_touch](examples/esp-idf/12_touch/README_ZH.md) | GT911 触摸绘图 |
| [13_comprehensive_test](examples/esp-idf/13_comprehensive_test/README_ZH.md) | 综合外设测试仪表盘 |
| [14_lvgl_porting](examples/esp-idf/14_lvgl_porting/README_ZH.md) | LVGL 显示与触摸 |
| [15_bluetooth_speaker](examples/esp-idf/15_bluetooth_speaker/README_ZH.md) | 蓝牙 A2DP 扬声器 |
| [16_usb_camera](examples/esp-idf/16_usb_camera/README_ZH.md) | 外接 USB UVC 摄像头预览 |

## 🛠️ 开发环境

| 框架 | 版本 | 示例数 |
| --- | --- | ---: |
| ESP-IDF | 6.2.0-dev，提交 08e0d30a74ad0bfd5a34933142b80f45619ee410 | 16 |

源码构建步骤见 [ESP-IDF 使用说明](examples/esp-idf/README_ZH.md#构建与烧录)，首次构建需解析托管依赖并生成板级配置。

## 📚 资料

- [原理图](hardware/schematic/)
- [机械参考图纸及 3D 模型](hardware/mechanical/)（随附文件为 ESP32-S3 型号资料）
- [硬件参考](docs/hardware_ZH.md)
- [产品页](https://www.waveshare.net/shop/ESP32-S31-Touch-LCD-4.3C.htm)

## 🤝 支持

产品使用、硬件及订单问题请联系 [微雪技术支持](https://service.waveshare.com/)。

## 📄 许可证

除另有声明的第三方内容外，按 Apache License 2.0 提供，详见 [LICENSE](LICENSE)。
