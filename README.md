# Waveshare ESP32-S31-Touch-LCD-4.3C

[简体中文](README_ZH.md)

The **ESP32-S31-Touch-LCD-4.3C** is a 4.3-inch human-machine interface development board built around the ESP32-S31. It combines an 800 x 480 capacitive touch display with audio, motion sensing, battery monitoring, TF card storage, RTC, RS485, CAN-FD, and USB host connectivity.

This repository contains ESP-IDF examples, hardware drawings, and a full-flash test image. The examples load their board definitions from managed components.

![ESP32-S31-Touch-LCD-4.3C](docs/images/Product-1.webp)

## Hardware at a glance

| Function | Hardware |
| --- | --- |
| MCU | ESP32-S31, dual-core RISC-V, up to 320 MHz |
| Display | 4.3-inch, 800 x 480, 16-bit RGB565 interface |
| Touch | GT911 capacitive touch controller |
| Audio | ES8389 codec, microphone input, and speaker amplifier |
| Storage | TF card over SDMMC |
| Sensors | QMI8658 IMU and BQ27220 battery gauge |
| Timekeeping | PCF85063ATL RTC |
| Industrial interfaces | Half-duplex RS485 and CAN-FD |
| Expansion | USB host and exposed board connectors |

See [Hardware Reference](HARDWARE_REFERENCE.md) for the verified pin map, I2C addresses, and source notes. The source PDFs and CAD files are under [`hardware/`](hardware/).

## Software support

The repository currently contains **16 ESP-IDF projects** and no Arduino examples. ESP32-S31 framework support is still evolving: use an ESP-IDF revision that actually lists `esp32s31` as a supported target. The component manifests in these examples currently require ESP-IDF `>=6.2`.

Before choosing a framework revision, check Espressif's [ESP32-S31 support status](https://developer.espressif.com/hardware/esp32s31/). Stable ESP-IDF 5.5 and 6.0 releases, and the current stable Arduino-ESP32 release, do not provide an ESP32-S31 build target, so this repository does not claim support for them.

### Build an example

Install and export a compatible ESP-IDF environment, then run the following from an example directory:

Before the first build, follow [Component Strategy](docs/components.md#prepare-the-board-definition) to resolve managed dependencies and generate the board configuration. The published board package must include `esp32_s31_touch_lcd_4_3c`.

```text
cd examples/esp-idf/01_i2c_scan
idf.py set-target esp32s31
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the board's serial port. `idf.py bmgr` reads the managed `esp32_s31_touch_lcd_4_3c` board definition and generates the Board Manager initialization component. See the selected example's README for its hardware-specific behavior.

## Examples

| # | Example | Purpose |
| ---: | --- | --- |
| 1 | [`01_i2c_scan`](examples/esp-idf/01_i2c_scan/) | Scan the on-board I2C bus |
| 2 | [`02_rtc_pcf85063atl`](examples/esp-idf/02_rtc_pcf85063atl/) | Read and set the RTC |
| 3 | [`03_imu_qmi8658`](examples/esp-idf/03_imu_qmi8658/) | Read accelerometer and gyroscope data |
| 4 | [`04_bat_bq27220`](examples/esp-idf/04_bat_bq27220/) | Read battery-gauge information |
| 5 | [`05_tf_card`](examples/esp-idf/05_tf_card/) | Mount and access a TF card |
| 6 | [`06_codec_mic_raw`](examples/esp-idf/06_codec_mic_raw/) | Capture raw microphone samples through ES8389 |
| 7 | [`07_codec_sd_wav_player`](examples/esp-idf/07_codec_sd_wav_player/) | Play a WAV file from a TF card |
| 8 | [`08_rs485`](examples/esp-idf/08_rs485/) | Exercise the half-duplex RS485 interface |
| 9 | [`09_canfd`](examples/esp-idf/09_canfd/) | Send and receive CAN-FD frames |
| 10 | [`10_usb_host`](examples/esp-idf/10_usb_host/) | Enumerate an external USB device |
| 11 | [`11_rgb_lcd`](examples/esp-idf/11_rgb_lcd/) | Drive the 800 x 480 RGB LCD |
| 12 | [`12_touch`](examples/esp-idf/12_touch/) | Read GT911 touch input |
| 13 | [`13_comprehensive_test`](examples/esp-idf/13_comprehensive_test/) | Exercise the board's integrated peripherals |
| 14 | [`14_lvgl_porting`](examples/esp-idf/14_lvgl_porting/) | Run an LVGL display and touch demo |
| 15 | [`15_bluetooth_speaker`](examples/esp-idf/15_bluetooth_speaker/) | Receive Bluetooth A2DP audio and show an LVGL player UI |
| 16 | [`16_usb_camera`](examples/esp-idf/16_usb_camera/) | Display video from an external USB UVC camera |

The USB camera example does not imply an on-board camera; this board has no DVP camera interface. For implementation details and migration notes, see the [ESP-IDF example index](examples/esp-idf/README.md).

## Test firmware

[`firmware/ESP32-S31-Touch-LCD-4.3C-Test.bin`](firmware/ESP32-S31-Touch-LCD-4.3C-Test.bin) is a 16 MB full-flash **test image** supplied with this repository. It is kept separate from source-built example artifacts and is not described as factory or recovery firmware. Read [`firmware/README.md`](firmware/README.md) before flashing it because writing a full-flash image replaces the contents of the target flash.

## Versioned firmware releases

Pushing an annotated `vMAJOR.MINOR.PATCH` tag to GitHub builds all 16 examples,
checks their firmware packages, and publishes a Release with 16 ZIP files and
`SHA256SUMS`. ESP32-S31 builds use the pinned preview toolchain in
[`releases/config.json`](releases/config.json), so these Releases are marked
as prereleases. Branch and pull-request builds upload Actions artifacts;
manual runs can select one example and do not publish a Release.

See [Release instructions](releases/README.md#automatic-github-releases).

## Repository layout

```text
docs/                Repository, component, CI, and firmware notes
docs/images/         Product images
examples/esp-idf/    Standalone ESP-IDF projects
firmware/            Supplied full-flash test image
hardware/            Schematics and mechanical files
releases/            Source-built firmware packaging tools
scripts/             Repository discovery and validation helpers
```

Additional documentation:

- [Repository structure](docs/repository-structure.md)
- [Component layout](docs/components.md)
- [Firmware policy](docs/firmware.md)
- [CI scope](docs/ci.md)
- [Build validation](docs/validation.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Security policy](SECURITY.md)

## License

Unless a subdirectory or third-party component states otherwise, this repository is distributed under the [Apache License 2.0](LICENSE). Bundled third-party components and media retain their own license notices.
