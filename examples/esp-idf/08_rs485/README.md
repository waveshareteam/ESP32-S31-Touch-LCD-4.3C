# RS485

[简体中文](README_ZH.md)

## What It Does

Sends a counter string and logs incoming bytes through the managed half-duplex RS485 UART.

## Hardware Required

Board, USB data cable and an RS485 peer with matching wiring and baud rate.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The peer receives increasing counter values; replies appear in the serial monitor.

## Troubleshooting

Match the board definition’s 115200 baud setting and connect a common reference ground. This example requires an external RS485 peer for receive testing.
