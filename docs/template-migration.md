# Template Migration Notes

[简体中文](template-migration_ZH.md)

## Adopted Rules

This migration follows waveshareteam/esp32-repo-template v1 at commit c576a0a2d2d21c76fe05d84bb1ce1c3261bf4c44. It normalizes hardware paths, homepages and bilingual documentation, the five-section example instructions, build workflow naming and ignore rules. Source, CMake, manifests, sdkconfig, partitions and local components in all 16 examples remain unchanged.

## Shared CI Prerequisites

The existing build and release tools are retained. No product.json claiming an incompatible framework was added, and the working workflow was not replaced with one that cannot build these projects. Template maintainer Wulu needs to address:

1. **ESP-IDF version selection:** examples require >=6.2 and builds pin the commit in [config.json](../releases/config.json). The template accepts only release tags and uses matching espressif/idf container tags, which cannot express this commit and toolchain.
2. **Board Manager generation:** a fresh checkout must resolve managed dependencies, run bmgr to generate gen_bmgr_codes, then build. The template currently runs only set-target and build.
3. **S31 flashing arguments:** the existing packager preserves stub=false from flasher_args.json as --no-stub; the template packager does not propagate it.
4. **Release behavior:** the existing workflow automatically publishes a regular Latest release after remote verification; the template creates a draft for manual publication. Agree on the intended behavior before switching.
5. **ESP-IDF-only documentation checks:** the specification permits products without Arduino, but the default homepage audit and contract require an Arduino quick link. This repository adds neither an empty framework directory nor a broken link.

Once supported, configure product.json with the real toolchain, switch to shared CI and validate all 16 examples at the new commit. L1/L2 compliance is not claimed before that verification.

## Retained Exceptions

- releases, scripts, tests and associated maintenance documentation remain until shared CI can preserve the existing build, packaging and release checks.
- The GitHub repository is currently private, so public organization community-file inheritance does not apply. Local collaboration templates and contribution, support and security documents are retained and paired in both languages.
- docs/hardware preserves the previous pin and schematic cross-checks. Runtime pin configuration remains owned by the managed BSP.
- The supplied test image has no established source, build date or factory identity. Its filename and bytes are preserved; no FactoryOnly name or date is invented, and it is not uploaded as a Release asset.
- S3 mechanical files only change directory; their product identity is preserved and no verified S31 dimensions are claimed.

## Links and Repository Metadata

The Chinese product page confirms this product and its N16R16 specification. The candidate Chinese documentation URL returned a generic homepage, English-site requests were restricted, and Gitee issues could not be confirmed in a browser. Unconfirmed Wiki/Gitee links are omitted; support uses the official Waveshare service portal.

GitHub About was not modified. Suggested values, subject to authorization:

- Description: Waveshare ESP32-S31 4.3-inch 800 x 480 RGB capacitive touch development board: ESP-IDF examples, firmware and hardware resources
- Website: https://www.waveshare.net/shop/ESP32-S31-Touch-LCD-4.3C.htm
- Topics: waveshare, esp32, esp32-s31, esp32-s31-touch-lcd-4-3c, esp-idf

GitLab is the primary repository. Push branches and tags there and let the mirror synchronize GitHub; do not push source directly to GitHub.

## Verification Performed

- All 24 release-tool tests passed and all 16 ESP32-S31 projects were discovered.
- SHA256 comparisons against the baseline matched for 142 source, configuration, firmware and hardware files; hardware resources only moved directories. All 16 example README pairs have the required five sections.
- Default Markdown auditing of 64 pages found only the two missing Arduino quick links on the homepages. A strict audit using a temporary configuration that removes this inapplicable requirement reported zero errors and warnings; template defaults were not changed.
- The same 41 template tests ran through unittest: 38 passed and three errored because product.json is absent. The prescribed pytest command could not start because pytest is not installed.
- Git whitespace checks passed. No local firmware compilation, flashing or remote CI for the new commit was performed; historical results are not treated as current-commit build evidence.
