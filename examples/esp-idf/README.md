# ESP-IDF examples for ESP32-S31-Touch-LCD-4.3C

This directory contains 16 standalone ESP-IDF projects for the Waveshare ESP32-S31-Touch-LCD-4.3C.

## Toolchain requirement

Use an ESP-IDF revision that supports the `esp32s31` target and satisfies the examples' `idf >=6.2` component constraints. ESP32-S31 is not available in the stable ESP-IDF 5.5 or 6.0 release lines. Check the official [ESP32-S31 support status](https://developer.espressif.com/hardware/esp32s31/) instead of assuming that an installed ESP-IDF supports this target.

From the selected project directory, run:

```text
idf.py set-target esp32s31
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

`idf.py set-target esp32s31` is required explicitly for `15_bluetooth_speaker`; the other projects also declare this target in their top-level `CMakeLists.txt`. The Board Manager command consumes the local board YAML files and generates `components/gen_bmgr_codes`.

## Example index

| Example | Board resources used |
| --- | --- |
| [`1_i2c_scan`](1_i2c_scan/) | I2C master and temporary touch-controller wake-up |
| [`2_rtc_pcf85063atl`](2_rtc_pcf85063atl/) | PCF85063ATL RTC |
| [`3_imu_qmi8658`](3_imu_qmi8658/) | QMI8658 IMU |
| [`4_bat_bq27220`](4_bat_bq27220/) | BQ27220 battery gauge |
| [`5_tf_card`](5_tf_card/) | TF card filesystem and active-low card power control |
| [`6_codec_mic_raw`](6_codec_mic_raw/) | ES8389 audio ADC |
| [`7_codec_sd_wav_player`](7_codec_sd_wav_player/) | TF card filesystem and ES8389 audio DAC |
| [`8_rs485`](8_rs485/) | UART1 half-duplex RS485 peripheral |
| [`9_canfd`](9_canfd/) | ESP32-S31 TWAI/CAN-FD controller |
| [`10_usb_host`](10_usb_host/) | USB Host stack; VBUS comes from the board 5 V rail |
| [`11_rgb_lcd`](11_rgb_lcd/) | 800 x 480 RGB LCD and backlight |
| [`12_touch`](12_touch/) | RGB LCD, GT911 touch, and backlight |
| [`13_comprehensive_test`](13_comprehensive_test/) | Display, touch, sensors, storage, audio, RS485, CAN-FD, and USB Host |
| [`14_lvgl_porting`](14_lvgl_porting/) | LCD, touch, backlight, and `esp_lvgl_adapter` |
| [`15_bluetooth_speaker`](15_bluetooth_speaker/) | Bluetooth A2DP, ES8389 audio, LCD, touch, and LVGL |
| [`16_usb_camera`](16_usb_camera/) | USB Host/UVC, LCD, and backlight |

## Board integration

Each project is self-contained and includes a local `components/esp_boards` snapshot. Select the `esp32_s31_touch_lcd_4_3c` board before building. Examples that require custom operations also include a small `components/s31_board_support` adapter.

Important board controls:

- GPIO56 enables TF-card power at a low level.
- GPIO57 enables the speaker amplifier at a high level.
- The board has no DVP camera. `16_usb_camera` requires an external USB UVC camera.

The examples and their Board Manager migration have been statically reviewed. They have not been claimed as built, flashed, or hardware-tested by this repository update because the locally available ESP-IDF does not expose the `esp32s31` target.
