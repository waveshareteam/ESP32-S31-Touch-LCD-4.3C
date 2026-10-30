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

All projects declare `esp32s31` in their top-level `CMakeLists.txt`. The Board Manager command consumes the managed board YAML files and generates `components/gen_bmgr_codes`. On a fresh checkout, first resolve dependencies and, if the package lacks this board, copy the board definition as described in [Component Strategy](../../docs/components.md#prepare-the-board-definition).

## Example index

| Example | Board resources used |
| --- | --- |
| [`01_i2c_scan`](01_i2c_scan/) | I2C master and temporary touch-controller wake-up |
| [`02_rtc_pcf85063atl`](02_rtc_pcf85063atl/) | PCF85063ATL RTC |
| [`03_imu_qmi8658`](03_imu_qmi8658/) | QMI8658 IMU |
| [`04_bat_bq27220`](04_bat_bq27220/) | BQ27220 battery gauge |
| [`05_tf_card`](05_tf_card/) | TF card filesystem and active-low card power control |
| [`06_codec_mic_raw`](06_codec_mic_raw/) | ES8389 audio ADC |
| [`07_codec_sd_wav_player`](07_codec_sd_wav_player/) | TF card filesystem and ES8389 audio DAC |
| [`08_rs485`](08_rs485/) | UART1 half-duplex RS485 peripheral |
| [`09_canfd`](09_canfd/) | ESP32-S31 TWAI/CAN-FD controller |
| [`10_usb_host`](10_usb_host/) | USB Host stack; VBUS comes from the board 5 V rail |
| [`11_rgb_lcd`](11_rgb_lcd/) | 800 x 480 RGB LCD and backlight |
| [`12_touch`](12_touch/) | RGB LCD, GT911 touch, and backlight |
| [`13_comprehensive_test`](13_comprehensive_test/) | Display, touch, sensors, storage, audio, RS485, CAN-FD, and USB Host |
| [`14_lvgl_porting`](14_lvgl_porting/) | LCD, touch, backlight, and `esp_lvgl_adapter` |
| [`15_bluetooth_speaker`](15_bluetooth_speaker/) | Bluetooth A2DP, ES8389 audio, LCD, touch, and LVGL |
| [`16_usb_camera`](16_usb_camera/) | USB Host/UVC, LCD, and backlight |

## Board integration

Each project declares `waveshare/waveshare_boards` in its component manifest. Select the `esp32_s31_touch_lcd_4_3c` board before building. Examples that require custom operations also include a small `components/s31_board_support` adapter.

Important board controls:

- GPIO56 enables TF-card power at a low level.
- GPIO57 enables the speaker amplifier at a high level.
- The board has no DVP camera. `16_usb_camera` requires an external USB UVC camera.

All 16 examples passed compilation with the configuration recorded in [Build Validation](../../docs/validation.md). Flashing and hardware behavior require separate validation on the board.
