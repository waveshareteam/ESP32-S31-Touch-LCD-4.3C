# Continuous Integration

The repository currently runs target-independent repository checks. It does not run an ESP-IDF build matrix.

ESP32-S31 support is still described by Espressif as preview support. The current status and recommended development branch are maintained on the [official ESP32-S31 status page](https://developer.espressif.com/hardware/esp32s31/). Stable ESP-IDF build jobs must not be added until the selected release explicitly supports `esp32s31`. A prerelease or development-branch job requires an explicit maintainer decision and must be pinned and labeled experimental.

## Repository Checks

`.github/workflows/repository-checks.yml` performs lightweight checks that do not install ESP-IDF or compile firmware:

- compiles the Python tooling to catch syntax errors;
- requires exactly 16 first-party projects under `examples/esp-idf/*/CMakeLists.txt`;
- validates both example-name and repository-relative path selectors;
- checks explicit project target declarations against `esp32s31`;
- checks archive-name sanitization; and
- creates and inspects a firmware archive from synthetic ESP-IDF build metadata.

The packaging smoke test does not validate application code, managed components, hardware behavior, or framework compatibility.

## Discovering Projects

The discovery command always requires an explicit target:

```bash
python3 scripts/discover_examples.py --target esp32s31 --selector all
```

`--selector` accepts `all`, an example directory name, or a repository-relative example path. For example:

```bash
python3 scripts/discover_examples.py \
  --target esp32s31 \
  --selector examples/esp-idf/11_rgb_lcd \
  --require-match
```

The script discovers ESP-IDF projects only. This repository has no first-party Arduino sketches, and the stable Arduino-ESP32 package does not provide an ESP32-S31 board FQBN, so Arduino discovery and CI are intentionally absent.

## Adding Build CI Later

Before adding build jobs, re-run the framework support gate against current official releases. Include only stable ESP-IDF releases that contain the `esp32s31` target. Keep any explicitly approved preview job separate from required checks, pin it to an exact tag or commit, and pass `--target esp32s31` to discovery and packaging commands.

Source-built firmware may be packaged with `releases/package_firmware.py` after a successful build. Prebuilt test binaries under `firmware/` are not CI build outputs and must not be republished as generated artifacts.
