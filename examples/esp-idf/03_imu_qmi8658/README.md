# QMI8658 IMU

Acquires the custom Board Manager device `imu_qmi8658` through
`s31_board_support`. It reports identity, data-ready state, interrupt status,
GPIO level, temperature, acceleration and gyroscope values.

The generated board configuration owns I2C and the GPIO54 interrupt mapping.
Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
