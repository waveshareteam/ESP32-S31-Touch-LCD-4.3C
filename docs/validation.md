# Build Validation

[简体中文](validation_ZH.md)

Historical validation date: 2026-10-30 (Asia/Shanghai). These records do not
establish CI success for the current branch commit.

All 16 ESP-IDF examples built successfully for `esp32s31` with ESP-IDF 6.2.0,
commit `08e0d30a74ad0bfd5a34933142b80f45619ee410`, using the prepared managed board
definition described in [Component Strategy](components.md).

## Resolved Dependencies

All examples resolved `waveshare/waveshare_boards` 0.1.3 and managed
`espressif/esp_board_manager` 0.7.5. The three adapter examples pin
`esp_lvgl_adapter` to 0.6.0. All four LVGL examples pin LVGL to 9.5.0.
Codec remains on the 2.x prerelease API required by Board Manager.

| Example | Build | LVGL adapter | Codec |
| --- | --- | --- | --- |
| `01_i2c_scan` | Passed | — | 2.0.0-beta5 |
| `02_rtc_pcf85063atl` | Passed | — | 2.0.0-beta5 |
| `03_imu_qmi8658` | Passed | — | 2.0.0-beta5 |
| `04_bat_bq27220` | Passed | — | 2.0.0-beta5 |
| `05_tf_card` | Passed | — | 2.0.0-beta5 |
| `06_codec_mic_raw` | Passed | — | 2.0.0-beta5 |
| `07_codec_sd_wav_player` | Passed | — | 2.0.0-beta5 |
| `08_rs485` | Passed | — | 2.0.0-beta5 |
| `09_canfd` | Passed | — | 2.0.0-beta5 |
| `10_usb_host` | Passed | — | 2.0.0-beta5 |
| `11_rgb_lcd` | Passed | — | 2.0.0-beta5 |
| `12_touch` | Passed | — | 2.0.0-beta5 |
| `13_comprehensive_test` | Passed | 0.6.0 | 2.0.0-beta5 |
| `14_lvgl_porting` | Passed | 0.6.0 | 2.0.0-beta5 |
| `15_bluetooth_speaker` | Passed | 0.6.0 | 2.0.0-beta5 |
| `16_usb_camera` | Passed | — | 2.0.0-beta5 |

## Repository Checks

- All example directories use two-digit numbering and explicitly declare `esp32s31`.
- All adapter manifests and resolved packages use 0.6.0.
- Relative documentation links resolve, and example discovery finds all 16 projects.
- Python tooling passes syntax compilation and the Git diff passes whitespace checks.
- The checked-in test image matches the SHA256 recorded in the firmware documentation.

## Hardware Scope

These results cover compilation and repository consistency; no firmware was
flashed during validation.
Display output, touch alignment, audio behavior, and external devices require
measurements on hardware. The original 16-example build used 1-bit SDMMC; the
board definition now selects 4-bit mode. Mechanical
files named ESP32-S3 remain S3 reference material rather than S31 dimensions.

The integrated test compiled successfully with the ES8389 microphone clock
workaround for `esp_codec_dev` 2.0.0-beta5: the two 16-bit slots use a 32fs BCLK.
The application checks the bus geometry and clock settings before updating and
reading back registers 0x05, 0x06, 0x07 and 0x09. Microphone response requires
verification on hardware.

The header and footer padding fix also compiled successfully. Rendering with
LVGL 9.5 confirmed that both microphone channel meters, the footer log and the
header summary fit on the 800 x 480 screen.

To reproduce a build after preparing the board definition, run from an example
directory in a compatible ESP-IDF environment:

```bash
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
```

## Release automation

The release build entry point compiled all 16 examples in an isolated validation
checkout with the toolchain recorded in `releases/config.json`. All 16 resulting
firmware archives passed the complete matrix validator, including image headers,
combined-image offsets and padding, helper arguments, checksums, dependency
versions and board-definition hashes. The ESP32-S31 `--no-stub` flash setting is
retained. No board was flashed.

Because the registry package does not yet include this board, these local builds
used the existing board definition as a validation fixture. Production CI has no
fallback board copy and requires a published `waveshare/waveshare_boards` version
containing the board before a product tag is pushed.

Release-tool tests cover malformed and moved tags, default-branch reachability,
corrupted or incomplete artifacts, dependency drift, draft retry, remote digest
failure and refusal to change a public Release. Both workflows passed actionlint.
The existing generic packaging smoke test also passed. GitHub upload/publication
was exercised with a mocked API, not an actual remote Release.

## SDMMC 4-bit follow-up

The shared board definition now sets `fs_sdcard.config.sub_config.bus_width`
to 4, with D0-D3 on GPIO20-GPIO23, CLK on GPIO24 and CMD on GPIO25.
Examples `05_tf_card`, `07_codec_sd_wav_player` and `13_comprehensive_test`
were rebuilt successfully with this updated board fixture and the same pinned
toolchain. Each generated `gen_board_device_config.c` was checked for
`.bus_width = 4`. The builds were isolated from the product checkout.
The updated board definition must be included in the published
`waveshare/waveshare_boards` package used by these examples.
The user subsequently confirmed successful hardware testing, including the
4-bit SDMMC change. This confirmation is separate from the automated build
validation recorded above.
