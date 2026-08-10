# USB Camera

Uses managed LCD/backlight devices while retaining the USB Host, UVC and JPEG
preview flow. USB Host VBUS is supplied directly by the board 5V rail and has
no GPIO enable. Startup errors unwind UVC, USB tasks, USB Host, buffers, queues
and LCD resources in reverse order.

This is a USB camera example, not the board DVP camera device. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory before
building.
