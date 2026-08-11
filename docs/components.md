# Component Strategy

The examples currently keep board support local so the preview `esp32s31`
target and board configuration travel with each independently buildable
project.

## Board Manager Layout

Every one of the 16 projects under `examples/esp-idf/` contains a local
`components/esp_boards` copy. Its board catalog includes
`esp32_s31_touch_lcd_4_3c`, whose YAML files describe the board devices,
peripherals, pins, buses, and initialization hooks used to generate code.

Run this command from the selected project after changing board YAML:

```text
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
```

The resulting `components/gen_bmgr_codes` belongs to that project. A generated
directory from one example may have a different enabled-device set and must
not be reused by another example.

## Local S31 Adapters

The following examples also contain `components/s31_board_support` for custom
operations that are not expressed solely by the generic Board Manager device
API:

- `2_rtc_pcf85063atl`
- `3_imu_qmi8658`
- `4_bat_bq27220`
- `9_canfd`
- `10_usb_host`
- `13_comprehensive_test`
- `16_usb_camera`

Keep this adapter project-specific until its APIs and preview-target support
are stable enough for a shared component. Do not replace it with an ESP32-S3
BSP or copy hardware constants from another board.

`13_comprehensive_test` additionally carries a local
`espressif__esp_board_manager` source tree. `15_bluetooth_speaker` contains
application-specific A2DP and UI components. These trees should not be pulled
into unrelated examples simply because they are present in the repository.

## Managed Dependencies

Example-specific managed dependencies are declared in each project's
`main/idf_component.yml`. Treat those manifests as the dependency source of
truth. Do not add repository-wide version claims that are not valid for every
example or for the selected ESP32-S31 preview toolchain.

A successful dependency resolution or compile does not validate the board's
pins or electrical behavior. Hardware-facing updates must also be checked
against:

- `hardware/schematics/ESP32-S31-Touch-LCD-4.3C-Schematics.pdf`,
- the selected project's board YAML,
- generated Board Manager configuration,
- project `sdkconfig.defaults`,
- the corresponding example source and README.

## Future Consolidation

The repeated `esp_boards` trees are candidates for consolidation only after a
shared component supports `esp32s31`, preserves the product board definition,
and works with the repository's selected preview toolchain. Until then, keep
changes synchronized across affected copies and record any intentional
difference in the example README.

There is no Arduino component surface in this repository. Bundled Arduino
libraries or BSPs from the ESP32-S3 reference product are out of scope.
