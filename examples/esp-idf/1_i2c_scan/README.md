# I2C Scan

References the Board Manager-owned `i2c_master` bus and reads its generated
SDA/SCL configuration. The `lcd_touch` device is initialized and released once
to apply the GT911 reset, wake-up and address-selection sequence before scanning.

The loop reports bus idle levels, timeouts, other probe errors and responding
7-bit addresses without creating a second I2C bus. Expected devices may include:

- `0x14` or `0x5d` GT911 touch
- `0x51` PCF85063ATL RTC
- `0x6a` or `0x6b` QMI8658 IMU
- `0x55` BQ27220 battery gauge
- `0x10` or `0x11` ES8389 codec

The board definition is local to this project under
`components/esp_boards/esp32_s31_touch_lcd_4_3c`; the project does not use
`EXTRA_COMPONENT_DIRS` to reach repository-level board or support components.

## Prepare the board configuration

Run this command from the project directory:

```text
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
```

The command reads the local board YAML files and generates
`components/gen_bmgr_codes`. Do not copy the generated files from another
example because its enabled device set may differ.

## Build and flash

After the Board Manager command has generated the project component:

```text
idf.py -p PORT flash monitor
```
