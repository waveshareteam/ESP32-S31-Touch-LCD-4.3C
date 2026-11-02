# Repository Structure

[简体中文](repository-structure_ZH.md)

This repository contains board-specific ESP-IDF examples and support material
for the ESP32-S31-Touch-LCD-4.3C.

## Top-Level Layout

| Path | Purpose |
| --- | --- |
| `examples/esp-idf/` | First-party ESP-IDF source projects for `esp32s31` |
| `hardware/` | Schematics and mechanical reference files |
| `firmware/` | Checked-in test full-flash image and its usage notes |
| `releases/` | Helpers and documentation for packaging source-built firmware artifacts |
| `docs/` | Repository, component, firmware, and CI maintenance notes |
| `docs/images/` | Product images used by repository documentation |
| `scripts/` | Target-independent project discovery and repository helpers |
| `.github/` | Repository checks and collaboration templates |

The repository has no Arduino source tree. Do not create an empty
`examples/arduino/` directory or include Arduino examples from another product.

## ESP-IDF Projects

All 16 first-party projects are direct children of `examples/esp-idf/` and
contain a project `CMakeLists.txt`. All projects explicitly set
`IDF_TARGET esp32s31`.

| Project | Purpose |
| --- | --- |
| `01_i2c_scan` | Scan the Board Manager-owned I2C bus |
| `02_rtc_pcf85063atl` | PCF85063ATL real-time clock |
| `03_imu_qmi8658` | QMI8658 motion sensor |
| `04_bat_bq27220` | BQ27220 battery gauge |
| `05_tf_card` | SDMMC TF-card mount, file test, and directory listing |
| `06_codec_mic_raw` | ES8389 microphone capture and sample diagnostics |
| `07_codec_sd_wav_player` | SD-card WAV playback through ES8389 |
| `08_rs485` | Half-duplex RS485 communication |
| `09_canfd` | TWAI/CAN-FD transmit and receive |
| `10_usb_host` | USB Host device discovery and descriptor logging |
| `11_rgb_lcd` | 800 x 480 RGB LCD framebuffer and pattern tests |
| `12_touch` | GT911 touch input and LCD drawing |
| `13_comprehensive_test` | Integrated board test dashboard and automatic/manual tests |
| `14_lvgl_porting` | LCD, touch, backlight, and LVGL adapter integration |
| `15_bluetooth_speaker` | Bluetooth A2DP speaker reference application |
| `16_usb_camera` | External USB UVC camera preview on the LCD |

Each project owns its local components, `sdkconfig.defaults`, dependencies, and
generated Board Manager output. This avoids hidden cross-project include paths
and allows an example to be configured independently.

## Board Configuration

Board definitions come from the managed `waveshare/waveshare_boards` package.
Follow [Component Strategy](components.md#prepare-the-board-definition) to
resolve dependencies; the published package must contain the selected board.
The board identifier is `esp32_s31_touch_lcd_4_3c`. Run the following command
from the selected project before building, especially after changing its board
YAML files:

```text
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
```

The generated `components/gen_bmgr_codes` directory belongs only to that
project. Do not copy it from another example because enabled devices and
generated dependencies may differ.

## Toolchain Boundary

Builds and CI use the ESP-IDF commit recorded in
[`releases/config.json`](../releases/config.json). The build script verifies
the active framework commit and records its exact identity in each package.

## Firmware Boundary

Source-maintained firmware consists of the projects under `examples/esp-idf/`.
The checked-in `firmware/ESP32-S31-Touch-LCD-4.3C-Test.bin` is a separate 16 MiB
test full-flash image. It has no source project under `firmware/`, is not built
by example CI, and must not be described as a source-built release artifact.
