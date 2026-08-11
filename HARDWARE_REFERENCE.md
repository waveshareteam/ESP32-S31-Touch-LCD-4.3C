# ESP32-S31-Touch-LCD-4.3C Hardware Reference

[简体中文](HARDWARE_REFERENCE_ZH.md)

This reference consolidates the board settings that can be verified from the ESP32-S31 schematic and the local Board Manager definition. Values copied from similarly named ESP32-S3 products are not valid evidence for this board.

## Board overview

| Item | Verified value |
| --- | --- |
| Product | Waveshare ESP32-S31-Touch-LCD-4.3C |
| ESP-IDF target | `esp32s31` |
| Module label in schematic | ESP32-S31-WROOM-3 |
| CPU family | Dual-core RISC-V, up to 320 MHz |
| Display | 4.3-inch, 800 x 480, RGB565 |
| Touch | GT911 capacitive touch |
| Audio codec | ES8389 |
| Storage | TF card over SDMMC |
| On-board devices | PCF85063ATL RTC, QMI8658 IMU, BQ27220 battery gauge |
| Wired interfaces | Half-duplex RS485, CAN-FD, USB Host |

The battery-gauge configuration contains a 650 mAh design value. This is a software configuration value, not a declaration that the board supports only one fixed battery capacity.

## Display and touch

The current board definition drives the panel over a 16-bit RGB bus with two frame buffers in PSRAM.

| Signal | GPIO / setting |
| --- | --- |
| RGB D0-D15 | 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 33, 34, 35, 36 |
| PCLK | GPIO40, 26 MHz, active on falling edge |
| DE | GPIO43 |
| HSYNC | GPIO44 |
| VSYNC | GPIO45 |
| Horizontal timing | pulse 4, back porch 8, front porch 8 |
| Vertical timing | pulse 4, back porch 8, front porch 8 |
| Backlight | GPIO47, LEDC 5 kHz, software default 60% |
| GT911 reset | GPIO46 |
| GT911 interrupt | GPIO2 |
| Touch resolution | 800 x 480 |

The schematic contains an `LCD_RST` net, while the current generic RGB YAML does not identify a display-reset GPIO. Do not infer an LCD reset pin from another product.

## Shared I2C bus

I2C0 uses SDA GPIO0 and SCL GPIO1 at up to 400 kHz. Addresses below use the conventional **7-bit** form.

| Device | 7-bit address | Interrupt / control |
| --- | --- | --- |
| GT911 touch | `0x5D` or `0x14` | INT GPIO2, RST GPIO46 |
| ES8389 codec | `0x10` | Audio data uses I2S |
| PCF85063ATL RTC | `0x51` | INT GPIO3 |
| QMI8658 IMU | `0x6B`, fallback `0x6A` | INT GPIO54 |
| BQ27220 battery gauge | `0x55` | No GPIO assigned by current YAML |

The Board Manager device YAML stores GT911 as `0xBA` / `0x28` and ES8389 as `0x20`. Those are legacy 8-bit address forms that include the R/W bit; do not present them as 7-bit bus addresses.

## Audio

| Signal | GPIO / setting |
| --- | --- |
| I2S BCLK | GPIO37 |
| I2S LRCK / WS | GPIO38 |
| I2S DOUT | GPIO39 |
| I2S DIN | GPIO42 |
| Speaker amplifier enable | GPIO57, active high |
| Current stream default | 48 kHz, 16-bit, stereo |

The current software sets I2S MCLK to `-1` and configures ES8389 for no external MCLK. The schematic nevertheless contains an `I2S_MCLK` net, so this must not be described as proof that the hardware has no MCLK connection. The schematic also identifies an NS4150B amplifier and two microphone channels.

## TF card

| Signal | GPIO / setting |
| --- | --- |
| SDMMC CLK | GPIO24 |
| SDMMC CMD | GPIO25 |
| D0 | GPIO20 |
| D1 | GPIO21 |
| D2 | GPIO22 |
| D3 | GPIO23 |
| Card power enable | GPIO56, active low |

The current examples configure a 1-bit SDMMC bus even though D1-D3 are assigned in the board definition. Treat 4-bit operation as unverified until the software and hardware are tested together.

## Industrial and USB interfaces

| Interface | Signals | Current configuration |
| --- | --- | --- |
| CAN-FD | TX GPIO4, RX GPIO5 | MCP2562FD transceiver; 500 kbit/s arbitration, 2 Mbit/s data |
| RS485 | UART1 TX GPIO6, RX GPIO7, RTS/DE GPIO61 | SIT3088EESA / SP3485EN circuit; 115200 baud, half-duplex |
| USB Host | Dedicated USB D+ / D- package pins | VBUS is supplied directly from the board 5 V rail |

The USB VBUS rail has no Board Manager GPIO-enable device. Do not document it as software-controlled. The board has no DVP camera; the USB camera example requires an external UVC camera.

## Source files

The primary repository sources for this reference are:

- [ESP32-S31 schematic](hardware/schematics/ESP32-S31-Touch-LCD-4.3C-Schematics.pdf)
- [Comprehensive-test board information](examples/esp-idf/13_comprehensive_test/components/esp_boards/esp32_s31_touch_lcd_4_3c/board_info.yaml)
- [Comprehensive-test device definition](examples/esp-idf/13_comprehensive_test/components/esp_boards/esp32_s31_touch_lcd_4_3c/board_devices.yaml)
- [Comprehensive-test peripheral definition](examples/esp-idf/13_comprehensive_test/components/esp_boards/esp32_s31_touch_lcd_4_3c/board_peripherals.yaml)

The files under [`hardware/dimensions/`](hardware/dimensions/) still use `ESP32-S3-Touch-LCD-4.3C` in their names and metadata. They are retained as supplied, but their approximately 75.10 x 112.40 mm drawing must not be treated as authoritative S31 mechanical evidence until Waveshare confirms the revision relationship.
