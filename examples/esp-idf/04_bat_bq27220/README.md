# BQ27220 Battery Gauge

Acquires the custom Board Manager device `bat_bq27220` through
`s31_board_support` and periodically reports voltage, current, temperature,
capacity, state of charge, state of health and status values over the shared
manager-owned I2C bus.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.

The BQ27220 is powered directly from the connected battery. If the gauge is
not powered or is not fitted, the example reports the initialization error and
retries every two seconds instead of aborting. Use `01_i2c_scan` to confirm that
the configured battery-gauge address responds on the shared I2C bus.
