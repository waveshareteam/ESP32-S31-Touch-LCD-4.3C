# USB Host

[简体中文](README_ZH.md)

## What It Does

Enumerates USB devices and logs connection, disconnection and descriptor information.

## Hardware Required

Board, USB data cable and a USB device connected to the board’s host interface.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor shows device speed, configuration, VID, PID and class after connection.

## Troubleshooting

Host VBUS comes directly from the board’s 5 V rail, so provide suitable board power. This example enumerates devices; it does not implement every USB class application.
