# ESP-IDF Firmware Packaging

`package_firmware.py` converts an existing ESP-IDF build directory into a flashable zip archive. It does not select, install, or validate an ESP-IDF release, so complete the framework target-support check before building ESP32-S31 firmware.

The repository contains 16 first-party ESP-IDF projects and no Arduino
sketches or bundled Arduino libraries. This packager covers source-built
ESP-IDF output only.

The target is always explicit. After building an example with an ESP-IDF revision that supports the board, package it with:

```bash
python3 releases/package_firmware.py \
  --project examples/esp-idf/11_rgb_lcd \
  --build-dir examples/esp-idf/11_rgb_lcd/build \
  --framework-version <exact-idf-tag-or-commit> \
  --target esp32s31 \
  --git-sha <git-commit>
```

The script verifies that `--target` agrees with the chip recorded in `flasher_args.json`. It then copies the required segments, creates a combined image starting at offset `0x0`, and writes the archive under `releases/dist/` by default.

Each archive contains:

```text
README.md
flash.sh
flash.bat
flash_args.txt
manifest.json
bin/*.bin
```

`manifest.json` records the framework version, target, project path, Git commit, UTC timestamp, original segment offsets, and the generated esptool command.

Activate the Python environment supplied with, or explicitly supported by, the
selected ESP32-S31 preview toolchain. Confirm that its esptool recognizes
`esp32s31`, then extract the archive and run the helper for the board serial
port:

```bash
python -m esptool --help
./flash.sh /dev/ttyUSB0
```

On Windows, run:

```bat
flash.bat COMx
```

Generated archives are ignored by `releases/.gitignore`. The checked-in
`firmware/ESP32-S31-Touch-LCD-4.3C-Test.bin` is a separate 16 MiB test
full-flash image. It is not established as factory or recovery firmware, is not
a source-built CI artifact, and this packaging script does not republish it.

The repository-check workflow exercises the packager with synthetic `flasher_args.json` data only. It does not upload firmware or imply that an ESP-IDF release supports ESP32-S31.
