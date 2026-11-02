# CAN-FD

[简体中文](README_ZH.md)

## What It Does

Sends 16-byte CAN-FD counter frames with identifier 0x123 and logs received frames.

## Hardware Required

Board, USB data cable and an active CAN-FD peer on a correctly terminated bus.

## How to Run

Follow the [ESP-IDF instructions](../README.md#build-and-flash) to prepare the pinned toolchain, resolve dependencies, generate this project’s board configuration, then build and flash.

## Expected Result

The serial monitor prints transmit counters and received frame identifiers, lengths and payloads.

## Troubleshooting

Match the managed board definition: 500 kbit/s arbitration and 2 Mbit/s data phase. A compatible active peer is needed to acknowledge frames.
