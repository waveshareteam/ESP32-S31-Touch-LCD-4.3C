# Continuous Integration

## Repository checks

`.github/workflows/repository-checks.yml` checks Python syntax, example discovery,
release-tool tests and packaging with synthetic firmware data. These checks do
not require ESP-IDF or connected hardware.

## Firmware builds and Releases

`.github/workflows/firmware.yml` builds the 16 first-party ESP-IDF projects:

- Firmware, tooling or workflow changes on `main`, `master` and pull requests
  build all projects and upload firmware ZIPs as Actions artifacts.
- A manual run accepts `all`, an example name or a repository-relative example
  path. It builds and uploads the selection without publishing a Release.
- Pushing an annotated `vMAJOR.MINOR.PATCH` tag builds all 16 projects, verifies
  the complete artifact set, uploads a draft Release, checks the remote asset
  names, sizes and SHA256 digests, then publishes it.

The release commit must be on the default branch. Every build and validation
job must succeed. A rerun may replace an unpublished draft's assets, but refuses
to modify an already published version. A fresh public asset download is also
checked after publication. Releases contain 16 firmware ZIPs and `SHA256SUMS`;
the checked-in full-flash test image is not republished as generated firmware.

## Pinned experimental toolchain

[`releases/config.json`](../releases/config.json) records the exact ESP-IDF
commit, Component Manager version, board identifier and component constraints.
The build script checks the active toolchain against this configuration.
The managed `waveshare/waveshare_boards` package must already contain the board.
There is no per-project board copy or CI fallback definition.

The [official ESP32-S31 status page](https://developer.espressif.com/hardware/esp32s31/)
still directs developers to preview support. ESP-IDF v6.1 lists `esp32s31` among
`PREVIEW_TARGETS`, while these examples require `>=6.2`. This workflow uses the
previously verified development commit recorded in the configuration, enables
`--preview` explicitly, labels builds experimental and marks Releases as
prereleases. It does not claim a stable framework or hardware-validation matrix.

Each firmware manifest records its source commit, exact framework commit,
resolved registry dependency versions/hashes and board-definition file hashes.
The aggregate validator rejects dependency or board drift between matrix jobs.

Third-party Actions are pinned to full commit SHAs and maintained by Dependabot.
Workflow permissions default to `contents: read`; only the release job gets
`contents: write` through the built-in GitHub token. No personal token is needed.

See [Release instructions](../releases/README.md#automatic-github-releases)
([简体中文](../releases/README_ZH.md)) for tagging and GitLab mirror requirements.
Build success does not establish physical-board functionality.
