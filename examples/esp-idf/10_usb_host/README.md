# USB Host

Installs the USB Host stack and retains device connect/disconnect, speed,
configuration, VID, PID and class logging. USB Host VBUS is supplied directly
by the board 5V rail and has no GPIO enable. Startup failures release client
tasks, host tasks and the USB stack in reverse order.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
