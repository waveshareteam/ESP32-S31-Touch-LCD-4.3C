# Touch

Initializes managed `display_lcd`, `lcd_touch` and `lcd_brightness`
devices. GT911 points draw colored marks on the LCD and the top-right block
clears the page. LCD width and height come from the generated device config.
The board definition uses 16-bit GT911 register commands, as required for
the product ID and touch status registers.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
