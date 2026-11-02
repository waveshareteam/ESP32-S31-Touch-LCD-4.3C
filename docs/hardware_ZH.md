# ESP32-S31-Touch-LCD-4.3C 硬件参考

[English](hardware.md)

本文汇总了可由 ESP32-S31 原理图和托管组件中的 Board Manager 板级定义共同核对的信息。

## 板卡概览

| 项目 | 已核对信息 |
| --- | --- |
| 产品 | 微雪 ESP32-S31-Touch-LCD-4.3C |
| ESP-IDF 目标 | `esp32s31` |
| 原理图模块标注 | ESP32-S31-WROOM-3 |
| CPU 架构 | 双核 RISC-V，最高 320 MHz |
| 显示 | 4.3 英寸，800 x 480，RGB565 |
| 触摸 | GT911 电容触摸 |
| 音频编解码器 | ES8389 |
| 存储 | SDMMC 接口 TF 卡 |
| 板载器件 | PCF85063ATL RTC、QMI8658 IMU、BQ27220 电量计 |
| 有线接口 | 半双工 RS485、CAN-FD、USB Host |

电量计配置中包含 650 mAh 设计值，这只是当前软件参数，不能据此宣称开发板仅支持固定容量的电池。

## 显示与触摸

当前板级定义使用 16 位 RGB 总线驱动屏幕，并在 PSRAM 中配置双帧缓冲。

| 信号 | GPIO / 配置 |
| --- | --- |
| RGB D0-D15 | 8、9、10、11、12、13、14、15、16、17、18、19、33、34、35、36 |
| PCLK | GPIO40，26 MHz，下降沿有效 |
| DE | GPIO43 |
| HSYNC | GPIO44 |
| VSYNC | GPIO45 |
| 水平时序 | 脉宽 4、后肩 8、前肩 8 |
| 垂直时序 | 脉宽 4、后肩 8、前肩 8 |
| 背光 | GPIO47，LEDC 5 kHz，软件默认 60% |
| GT911 复位 | GPIO46 |
| GT911 中断 | GPIO2 |
| 触摸分辨率 | 800 x 480 |

## 共用 I2C 总线

I2C0 使用 SDA GPIO0、SCL GPIO1，当前速率最高 400 kHz。下表统一使用标准 **7 位地址**。

| 器件 | 7 位地址 | 中断 / 控制 |
| --- | --- | --- |
| GT911 触摸 | `0x5D` 或 `0x14` | INT GPIO2、RST GPIO46 |
| ES8389 编解码器 | `0x10` | 音频数据使用 I2S |
| PCF85063ATL RTC | `0x51` | INT GPIO3 |
| QMI8658 IMU | `0x6B`，备用 `0x6A` | INT GPIO54 |
| BQ27220 电量计 | `0x55` | 当前 YAML 未分配 GPIO |

Board Manager 设备 YAML 将 GT911 写成 `0xBA` / `0x28`、ES8389 写成 `0x20`。这些是包含 R/W 位的旧式 8 位表示，不能当作 7 位总线地址。

## 音频

| 信号 | GPIO / 配置 |
| --- | --- |
| I2S BCLK | GPIO37 |
| I2S LRCK / WS | GPIO38 |
| I2S DOUT | GPIO39 |
| I2S DIN | GPIO42 |
| 扬声器功放使能 | GPIO57，高有效 |
| 当前数据流默认值 | 48 kHz、16 位、立体声 |

当前软件将 I2S MCLK 设为 `-1`，并以无外部 MCLK 方式配置 ES8389；但原理图中仍有 `I2S_MCLK` 网络，因此不能据此宣称硬件没有 MCLK 连接。原理图还标出了 NS4150B 功放和双麦克风通道。

## TF 卡

| 信号 | GPIO / 配置 |
| --- | --- |
| SDMMC CLK | GPIO24 |
| SDMMC CMD | GPIO25 |
| D0 | GPIO20 |
| D1 | GPIO21 |
| D2 | GPIO22 |
| D3 | GPIO23 |
| 卡电源使能 | GPIO56，低有效 |

板级定义配置为 4 位 SDMMC，使用 D0-D3。TF 卡、SD 卡 WAV 播放和综合测试示例共用此配置。

## 工业接口与 USB

| 接口 | 信号 | 当前配置 |
| --- | --- | --- |
| CAN-FD | TX GPIO4、RX GPIO5 | MCP2562FD 收发器；仲裁 500 kbit/s、数据 2 Mbit/s |
| RS485 | UART1 TX GPIO6、RX GPIO7、RTS/DE GPIO61 | SIT3088EESA / SP3485EN 电路；115200 波特率、半双工 |
| USB Host | 芯片封装专用 USB D+ / D- 引脚 | VBUS 直接来自板载 5 V 电源 |
