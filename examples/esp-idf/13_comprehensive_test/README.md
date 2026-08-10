# Comprehensive Test

Preserves the original LVGL dashboard and automatic/manual test flow while
using Board Manager for LCD, touch, RTC, IMU, battery, SD, audio, RS485, CAN-FD
resources, plus USB Host integration. Wi-Fi remains an ESP-IDF subsystem test.

The RS485/CAN-FD converter still uses the original 69-byte serial frame and
64-byte CAN-FD payload. SD power uses active-low GPIO56, and the speaker
amplifier uses active-high
GPIO57. USB VBUS is supplied directly by the board 5 V rail. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory first.
