# RS485

References the generated `uart_rs485` peripheral and uses its UART number,
TX/RX pins, RTS/DE pin, baud rate and half-duplex mode. The application only
performs UART reads and writes; it does not install a second driver or toggle
the transceiver GPIO directly.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
