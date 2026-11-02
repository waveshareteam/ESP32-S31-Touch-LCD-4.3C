# TF Card

[简体中文](README_ZH.md)

## What It Does

Mounts a TF card over 4-bit SDMMC, writes and reads a test file, and lists the root directory.

## Hardware Required

Board, USB data cable and a FAT-formatted TF card.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor reports file readback and root-directory entries.

## Troubleshooting

Use a card with writable free space and back up important files before running this write test. Card power and the 4-bit bus are controlled by the managed board definition.
