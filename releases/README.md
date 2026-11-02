# ESP-IDF Firmware Packaging

[简体中文](README_ZH.md)

## Automatic GitHub Releases

The firmware workflow builds all 16 examples on a version tag and publishes
16 firmware ZIPs plus `SHA256SUMS`. Each ZIP includes its original segments,
a combined image, checksums, a manifest and flash helpers. GitHub's automatic
Source code downloads are separate from the flashable firmware ZIPs.

Before the first release:

1. Publish a `waveshare/waveshare_boards` component version containing
   `boards/esp32_s31_touch_lcd_4_3c`. Adding the board only to its Git repository
   is not sufficient; the ESP Component Registry package must include it.
2. Merge reviewed product changes into the GitLab default branch and let the configured mirror synchronize them to GitHub.
3. Enable GitHub Actions. The workflow uses `github.token` and gives only the
   release job `contents: write`; no personal access token is needed.

Create a new annotated SemVer tag on the reviewed default-branch commit.
Bash (Linux) (also valid in Windows PowerShell):

```bash
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

Here `origin` is the primary GitLab repository; its GitHub mirror must sync
branches and tags. A tag that exists only on GitLab does not trigger GitHub Actions. After
the tag reaches GitHub, open **Actions → Build** to follow the build, then download your example ZIP from
**Releases**. Do not create a public Release manually before the workflow runs.

Only annotated `vMAJOR.MINOR.PATCH` tags on commits reachable from the default
branch are accepted. Every example must build, and the exact matrix must pass
binary-layout, target, source-commit and SHA256 validation. Uploads go to a
draft first; remote asset names, sizes and digests are checked before publishing.
A failed draft can be retried using **Re-run failed jobs**. An existing public
Release cannot be replaced; publish a new patch tag instead.

ESP32-S31 currently uses the pinned ESP-IDF framework in
[`config.json`](config.json). Product versions are published as regular
Releases with **Latest** enabled. Version tags, framework commits and resolved
component versions are recorded separately in manifests.
The board package keeps the projects' `"*"` version policy; all matrix jobs must
resolve the same component versions and board-definition hashes. Other release
pins are recorded in `config.json`.

Branch and pull-request builds upload Actions artifacts only. A manual
**Run workflow** can select `all`, an example name or a repository-relative
example path; it does not create a Release.

For local CI-equivalent builds, activate the configured ESP-IDF environment
and run from the repository root (Bash (Linux)):

```bash
python -m pip install "idf-component-manager==3.1.2"
python scripts/build_example.py 01_i2c_scan
python releases/package_release.py 01_i2c_scan
```

The release packager requires committed source. Archives go to the ignored
`releases/dist/` directory. The checked-in full-flash test image is retained
separately and is not uploaded by this workflow.

## Manual packaging

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
SHA256SUMS
bin/*.bin
```

`manifest.json` records the framework version, target, project path, Git commit, UTC timestamp, original segment offsets, and the generated esptool command.

Activate the Python environment supplied with, or explicitly supported by, the
selected ESP32-S31 toolchain. Confirm that its esptool recognizes
`esp32s31`, then extract the archive and run the helper for the board serial
port:

```bash
python -m esptool --help
./flash.sh PORT
```

On Windows, run:

```bat
flash.bat PORT
```

Generated archives are ignored by the root `.gitignore`. The checked-in
`firmware/ESP32-S31-Touch-LCD-4.3C-Test.bin` is a separate 16 MiB test
full-flash image. It is not established as factory or recovery firmware, is not
a source-built CI artifact, and this packaging script does not republish it.

The repository-check workflow exercises this generic packaging interface with
synthetic data. Automatic release builds use `package_release.py` to add and
validate the exact release identity and build context.
