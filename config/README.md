# Shared Configuration

This directory is reserved for ESP-IDF configuration overlays that are truly
shared by more than one first-party project.

All 16 current projects keep their own `sdkconfig.defaults` under
`examples/esp-idf/<project>/`. Those project-local files remain the source of
truth for feature selection, memory layout, and peripheral configuration.

Add a file here only when:

- at least two projects consume the same settings,
- the workflow or build command applies the overlay explicitly,
- the settings are valid for the pinned ESP32-S31 preview toolchain,
- applying the overlay does not hide a project-specific hardware requirement.

Do not copy shared overlays from an ESP32-S3 product. A stable ESP-IDF setting
or target option is not assumed to exist for the preview `esp32s31` target.
