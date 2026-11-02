# ESP-IDF Examples

[简体中文](README_ZH.md)

This directory contains 16 standalone ESP32-S31-Touch-LCD-4.3C projects using the managed waveshare/waveshare_boards component.

## Build and Flash

Install and export the ESP-IDF version listed in the [product configuration](../../.github/product.json), then run these commands in the selected example directory. The board component is pinned to a published version containing this board definition.

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

The first bmgr invocation resolves dependencies and generates this project’s `components/gen_bmgr_codes`; regenerate after board YAML changes and keep generated output separate for each example. Replace `PORT` with the device port and press Ctrl+] to exit the monitor.

Pins and device settings come from the published [board definition](https://github.com/waveshareteam/waveshare_boards/tree/v0.1.5/boards/esp32_s31_touch_lcd_4_3c); check the [schematic](../../hardware/schematic/) before changing hardware-related code. Examples 02, 03, 04, 09, 10, 13 and 16 retain product-specific adapters, and example 15 retains application-specific audio and UI components.

GitLab is the source repository; submit merge requests there and let the mirror synchronize changes to GitHub.

## Examples

| Example | Purpose |
| --- | --- |
| [01_i2c_scan](01_i2c_scan/) | Scans the shared Board Manager I2C bus after applying the GT911 wake-up sequence. |
| [02_rtc_pcf85063atl](02_rtc_pcf85063atl/) | Reads the RTC and its 1 Hz timer interrupt using the managed I2C bus. |
| [03_imu_qmi8658](03_imu_qmi8658/) | Reads QMI8658 identity, temperature, accelerometer, gyroscope and interrupt status. |
| [04_bat_bq27220](04_bat_bq27220/) | Periodically reads battery voltage, current, temperature, capacity and charge/health status. |
| [05_tf_card](05_tf_card/) | Mounts a TF card over 4-bit SDMMC, writes and reads a test file, and lists the root directory. |
| [06_codec_mic_raw](06_codec_mic_raw/) | Captures 48 kHz, 16-bit stereo samples through the managed ES8389 audio ADC. |
| [07_codec_sd_wav_player](07_codec_sd_wav_player/) | Plays a 16-bit PCM WAV file from the TF card through the ES8389; mono audio is converted to stereo. |
| [08_rs485](08_rs485/) | Sends a counter string and logs incoming bytes through the managed half-duplex RS485 UART. |
| [09_canfd](09_canfd/) | Sends 16-byte CAN-FD counter frames with identifier 0x123 and logs received frames. |
| [10_usb_host](10_usb_host/) | Enumerates USB devices and logs connection, disconnection and descriptor information. |
| [11_rgb_lcd](11_rgb_lcd/) | Draws test patterns using two RGB framebuffers in PSRAM and full-frame swaps. |
| [12_touch](12_touch/) | Reads GT911 touch points and draws colored marks on the LCD. |
| [13_comprehensive_test](13_comprehensive_test/) | Runs the LVGL dashboard with automatic and manual tests for display, touch, sensors, storage, audio, Wi-Fi, RS485, CAN-FD and USB Host. |
| [14_lvgl_porting](14_lvgl_porting/) | Connects the managed LCD, touch and backlight to esp_lvgl_adapter and runs the bundled UI/media demonstration. |
| [15_bluetooth_speaker](15_bluetooth_speaker/) | Receives Classic Bluetooth A2DP audio, plays it through the ES8389 and shows AVRCP track information on an LVGL player. |
| [16_usb_camera](16_usb_camera/) | Receives MJPEG video from an external USB UVC camera, decodes it and draws the preview on the LCD. |
