# LVGL Porting

Initializes managed LCD, touch and backlight devices, then registers their
generated handles with `esp_lvgl_adapter`. Dynamic LCD dimensions,
double-direct tear avoidance, PSRAM buffers and the optional PPA profile setting
are retained with the original media/UI behavior.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory
before building.
