# Comprehensive Test

Preserves the original LVGL dashboard and automatic/manual test flow while
using Board Manager for LCD, touch, RTC, IMU, battery, SD, audio, RS485, CAN-FD
resources, plus USB Host integration. Wi-Fi remains an ESP-IDF subsystem test.

The RS485/CAN-FD converter still uses the original 69-byte serial frame and
64-byte CAN-FD payload. SD power uses active-low GPIO56, and the speaker
amplifier uses active-high
GPIO57. USB VBUS is supplied directly by the board 5 V rail. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory first.
On a fresh checkout, follow [Component Strategy](../../../docs/components.md#prepare-the-board-definition)
to download dependencies and copy the board definition before generation.

Microphone capture uses 48 kHz, 16-bit stereo with DAC reference disabled and
BCLK as the ES8389 clock source. This requires 32 BCLK cycles per frame
(1.536 MHz). `esp_codec_dev` 2.0.0-beta5 instead configures its ES8389 clock
coefficients for four channels (64 cycles). After opening the ADC, `test_audio.c`
checks the bus format and clock source, applies the driver's 48 kHz / 32fs
coefficient row, and verifies the four changed registers by reading them back.
This example-level workaround leaves the managed component intact. Reassess it
when upgrading to a driver that configures ES8389 clocks from the actual bus width.

The startup log should include `MIC clock: 48000 Hz, 16-bit stereo,
BCLK=1536000 Hz (32fs)`. The subsequent `MIC activity` values are measured before
the display noise gate; compare quiet conditions with speech on both microphones.
Successful register verification establishes the configuration, while microphone
response must be checked on the board.
