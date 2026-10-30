# TF Card

Initializes the managed `fs_sdcard` device, which mounts SDMMC FAT storage and
controls its declared `sdcard_power_ctrl` dependency. The demo writes and
reads back a test file and repeatedly lists the root directory.

The board definition uses active-low GPIO56 for SD enable and 1-bit SDMMC
mode. The narrower bus avoids the observed 4-bit SD Status Register timeout
while retaining enough bandwidth for this filesystem test. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example
directory before building.
