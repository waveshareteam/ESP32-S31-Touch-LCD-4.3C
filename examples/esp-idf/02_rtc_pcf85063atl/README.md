# PCF85063ATL RTC

Acquires the custom Board Manager device `rtc_pcf85063` through
`s31_board_support`. It preserves time read/set behavior, the 1 Hz timer
interrupt and interrupt-status reporting while sharing the manager-owned I2C
bus. The interrupt pin is declared centrally as GPIO3.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
