# Component Strategy

All 16 projects under `examples/esp-idf/` declare the managed board package in
`main/idf_component.yml`:

```yaml
dependencies:
  waveshare/waveshare_boards:
    version: "*"
```

The main component requires `waveshare_boards`, which exposes ESP Board Manager
as a public dependency. Projects no longer carry `components/esp_boards` copies.

## Prepare the Board Definition

Use an ESP-IDF installation that supports `esp32s31` and satisfies the project's
manifest. From the selected example directory, first run:

```bash
idf.py -DIDF_TARGET=esp32s31 build
```

On a fresh checkout this resolves managed components and may fail because
`gen_bmgr_codes` has not been generated. The published
[`waveshare_boards`](https://github.com/waveshareteam/waveshare_boards) component
must include `esp32_s31_touch_lcd_4_3c`. Then run:

```bash
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
```

Release CI uses `scripts/build_example.py` to perform dependency resolution,
board generation and compilation automatically. It deliberately fails when
the published component lacks the board; there is no local board-copy fallback.
Publish the board component before tagging a product release. Managed components,
lockfiles, build outputs and generated board code are local build state.

Run `idf.py bmgr -b esp32_s31_touch_lcd_4_3c` again after changing board YAML.
The resulting `components/gen_bmgr_codes` belongs to that project. Generate it
independently for each example rather than copying another example's output.

## Local S31 Adapters

These examples retain `components/s31_board_support` for custom operations not
expressed solely by the generic Board Manager device API:

- `02_rtc_pcf85063atl`
- `03_imu_qmi8658`
- `04_bat_bq27220`
- `09_canfd`
- `10_usb_host`
- `13_comprehensive_test`
- `16_usb_camera`

All examples resolve Board Manager through the managed board package.
`15_bluetooth_speaker` retains application-specific A2DP and UI components.

## Display Dependency Versions

The three examples that use `esp_lvgl_adapter` (`13_comprehensive_test`,
`14_lvgl_porting`, and `15_bluetooth_speaker`) pin it to `0.6.0`.
All examples that use LVGL pin `lvgl/lvgl` to `9.5.0`, including the USB camera
example, which drives LVGL directly without the adapter.

Board Manager currently requires the `esp_codec_dev` 2.x prerelease API when
audio support is enabled. A codec 1.x stable release cannot be substituted
without a compatible Board Manager implementation.

## Validation Scope

Example-specific dependencies remain in each project's `main/idf_component.yml`.
A successful compile validates integration with the selected toolchain; pins and
electrical behavior require the schematic under `hardware/schematics/`, the
selected board YAML, generated configuration, and hardware tests.

There is no Arduino component surface in this repository.
