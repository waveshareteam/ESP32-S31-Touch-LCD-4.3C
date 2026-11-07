# ESP-IDF 示例

[English](README.md)

本目录包含 16 个独立的 ESP32-S31-Touch-LCD-4.3C 工程，使用托管组件 waveshare/waveshare_boards。

## 构建与烧录

安装并导出 [产品配置](../../.github/product.json) 中列出的 ESP-IDF 版本，在所选示例目录执行以下命令。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

首次运行 bmgr 会解析依赖并生成本工程的 `components/gen_bmgr_codes`；修改板级 YAML 后需重新生成，不跨示例复制生成文件。将 `PORT` 替换为设备端口，按 Ctrl+] 退出监视器。

所有示例的 `waveshare/waveshare_boards` 均使用 `version: "*"`，依赖解析时允许选择最新兼容的已发布版本；已有依赖锁文件可能保留旧版本。

引脚及器件配置以实际解析的组件为准，可参考最新的 [板级定义](https://github.com/waveshareteam/waveshare_boards/tree/main/boards/esp32_s31_touch_lcd_4_3c)；修改硬件相关代码前，应对照 [原理图](../../hardware/schematic/) 核实。02、03、04、09、10、13、16 示例保留产品专用适配层，15 示例保留应用专用音频及界面组件。

本仓库以 GitLab 为源头，修改应向 GitLab 提交合并请求，再由镜像同步到 GitHub。

## 示例

| 示例 | 功能 |
| --- | --- |
| [01_i2c_scan](01_i2c_scan/README_ZH.md) | 唤醒 GT911 后扫描 Board Manager 管理的共享 I2C 总线。 |
| [02_rtc_pcf85063atl](02_rtc_pcf85063atl/README_ZH.md) | 通过托管 I2C 总线读取 RTC 时间及其 1 Hz 定时中断。 |
| [03_imu_qmi8658](03_imu_qmi8658/README_ZH.md) | 读取 QMI8658 的器件标识、温度、加速度、角速度及中断状态。 |
| [04_bat_bq27220](04_bat_bq27220/README_ZH.md) | 周期读取电池电压、电流、温度、容量、电量及健康状态。 |
| [05_tf_card](05_tf_card/README_ZH.md) | 通过 4-bit SDMMC 挂载 TF 卡，写入并读回测试文件，循环列出根目录。 |
| [06_codec_mic_raw](06_codec_mic_raw/README_ZH.md) | 通过托管 ES8389 音频 ADC 采集 48 kHz、16 位立体声麦克风数据。 |
| [07_codec_sd_wav_player](07_codec_sd_wav_player/README_ZH.md) | 从 TF 卡读取 16 位 PCM WAV 文件，通过 ES8389 播放，单声道会转换为立体声。 |
| [08_rs485](08_rs485/README_ZH.md) | 通过托管半双工 RS485 UART 发送计数字符串，并记录接收的数据。 |
| [09_canfd](09_canfd/README_ZH.md) | 发送标识符为 0x123 的 16 字节 CAN-FD 计数帧，并记录接收帧。 |
| [10_usb_host](10_usb_host/README_ZH.md) | 枚举 USB 设备，记录连接、断开及描述符信息。 |
| [11_rgb_lcd](11_rgb_lcd/README_ZH.md) | 通过 PSRAM 双 RGB 帧缓冲及整帧切换显示测试图案。 |
| [12_touch](12_touch/README_ZH.md) | 读取 GT911 触摸坐标，并在 LCD 上绘制彩色标记。 |
| [13_comprehensive_test](13_comprehensive_test/README_ZH.md) | 运行 LVGL 仪表盘，自动及手动测试显示、触摸、传感器、存储、音频、Wi-Fi、RS485、CAN-FD 和 USB Host。 |
| [14_lvgl_porting](14_lvgl_porting/README_ZH.md) | 将托管 LCD、触摸及背光接入 esp_lvgl_adapter，运行随附界面与媒体演示。 |
| [15_bluetooth_speaker](15_bluetooth_speaker/README_ZH.md) | 接收经典蓝牙 A2DP 音频，通过 ES8389 播放，并在 LVGL 播放界面显示 AVRCP 曲目信息。 |
| [16_usb_camera](16_usb_camera/README_ZH.md) | 接收外接 USB UVC 摄像头的 MJPEG 视频，解码后在 LCD 上显示预览。 |
