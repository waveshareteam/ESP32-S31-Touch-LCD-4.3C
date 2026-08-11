# RGB LCD

Initializes managed `display_lcd` and `lcd_brightness` devices, obtains both
RGB framebuffers and cycles the original solid, eight-color bar, checkerboard,
gradient and geometry patterns using the generated LCD dimensions. Each pattern
is drawn into the inactive framebuffer, synchronized from the CPU cache to
PSRAM, and presented as a full-frame swap. This avoids modifying the buffer
currently scanned by the RGB peripheral or presenting stale cached pixels.

The framebuffer is configured in PSRAM. The board definition uses the original
BSP's 26 MHz pixel clock and 128-byte DMA burst. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory before
building.
