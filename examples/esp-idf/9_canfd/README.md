# CAN-FD

Acquires the custom `canfd` Board Manager device through
`s31_board_support`, registers the receive callback and enables the managed
TWAI-FD node. The original 16-byte counter frame and receive logging are
retained. Unsupported targets remain in the explicit skip loop.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
