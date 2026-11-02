# Component Strategy

[简体中文](components_ZH.md)

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

Follow the [ESP-IDF instructions](../examples/esp-idf/README.md#build-and-flash)
for the shared dependency-resolution, board-generation and flashing procedure.
The published component must include esp32_s31_touch_lcd_4_3c; CI has no local
board-copy fallback. Publish the board component before tagging a release.

Regenerate components/gen_bmgr_codes after board YAML changes, separately for
each example. Managed components, lockfiles, build outputs and generated board
code are local build state.

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
electrical behavior require the schematic under `hardware/schematic/`, the
selected board YAML, generated configuration, and hardware tests.

There is no Arduino component surface in this repository.
