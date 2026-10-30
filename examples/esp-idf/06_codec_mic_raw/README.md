# Codec Microphone Raw Capture

Initializes the managed `audio_adc` device and opens its ES8389 codec handle
for 48 kHz, 16-bit stereo input. The original peak calculation and first-sample
logging behavior is retained.

I2C and I2S ownership stays inside Board Manager. Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory before
building.
