# Codec SD WAV Player

This example initializes the managed `fs_sdcard` and `audio_dac` devices,
finds `/sdcard/test.wav` or the first WAV file in the card root, validates a
16-bit PCM mono/stereo stream, converts mono samples to stereo and repeatedly
plays it through the managed ES8389 codec.

Board Manager owns the complete hardware lifecycle:

- GPIO56 is driven low to enable the SD card before the FAT filesystem is mounted.
- GPIO57 enables the active-high speaker power amplifier with `audio_dac`.
- I2S uses BCLK GPIO37, LRCK GPIO38 and DOUT GPIO39.
- I2C configures the ES8389 at address `0x20`; its internal DAC reference is
  enabled to match the product BSP and avoid the invalid/noisy output path.
- SDMMC runs in 4-bit mode, using D0-D3 on GPIO20-GPIO23, CLK on GPIO24
  and CMD on GPIO25.

The included `test.wav` is an original six-second diagnostic signal: first a
left-channel tone, then a right-channel tone, followed by opposing stereo
sweeps. It contains no third-party recordings and is provided under CC0-1.0;
`generate_test_wav.py` reproduces it deterministically. Copy `test.wav` to
the root of a FAT-formatted card.

Run
`idf.py bmgr -b esp32_s31_touch_lcd_4_3c` from this example directory after
changing the board YAML and before building so `components/gen_bmgr_codes`
is regenerated.
