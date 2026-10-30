# TF Card

Initializes the managed `fs_sdcard` device, which mounts SDMMC FAT storage and
controls its declared `sdcard_power_ctrl` dependency. The demo writes and
reads back a test file and repeatedly lists the root directory.

The board definition uses active-low GPIO56 for SD enable and 4-bit SDMMC
mode, with D0-D3 on GPIO20-GPIO23, CLK on GPIO24 and CMD on GPIO25. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example
directory before building.
