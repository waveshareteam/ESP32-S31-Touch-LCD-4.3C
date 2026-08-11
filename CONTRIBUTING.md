# Contributing

Thank you for improving the ESP32-S31-Touch-LCD-4.3C repository.

## Before You Start

ESP32-S31 support is currently tied to a preview ESP-IDF toolchain. Do not
assume that a stock stable ESP-IDF release, container, or `esptool` package
supports the `esp32s31` target. Record the exact toolchain version or commit
used for every build result.

For changes that affect pins, clocks, buses, power control, display timing,
touch, audio, storage, USB, RS485, or CAN-FD, verify the change against the
local schematic and the board definition before changing source code or
documentation.

## Workflow

1. Keep the change scoped to the affected example, board definition,
   documentation, workflow, or support file.
2. Work from the relevant directory under `examples/esp-idf/`.
3. Regenerate that project's board configuration when its board YAML changes:

   ```text
   idf.py bmgr -b esp32_s31_touch_lcd_4_3c
   ```

4. Build with an ESP-IDF preview package or commit that explicitly supports
   `esp32s31`.
5. Test hardware-facing changes on an ESP32-S31-Touch-LCD-4.3C and record the
   board revision when known.
6. Submit a pull request with the exact validation performed and any remaining
   limitations.

## Repository Boundaries

- First-party source projects live under `examples/esp-idf/`.
- The repository currently contains 16 ESP-IDF projects and no Arduino
  sketches or bundled Arduino libraries.
- Each project keeps its board definition and required local components inside
  that project. Do not make one example depend on a sibling example's files.
- `components/gen_bmgr_codes` is generated for the selected example. Do not
  copy generated output between examples.
- The binary under `firmware/` is a test full-flash image. It is not a source
  project and must not be presented as a CI-generated example artifact.

## Validation Notes

A useful pull request report includes:

- affected example paths,
- exact ESP-IDF package, tag, or commit,
- `esp32s31` target confirmation,
- clean configure and build results,
- hardware revision and peripherals exercised,
- serial logs or screenshots for behavior changes,
- board YAML, generated-code, firmware-package, and documentation impact.

A successful compile validates source compatibility only. It does not validate
pin assignments, electrical behavior, timing, audio quality, display output,
touch alignment, or external-device compatibility.

## Public Text

Use repository-relative paths and reproducible commands. Do not include local
drive letters, network shares, usernames, credentials, or private tool paths in
issues, commits, pull requests, or documentation.
