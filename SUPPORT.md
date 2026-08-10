# Support

Start with the product and development links in the root `README.md`, then read
the README for the affected project under `examples/esp-idf/`.

## Repository Issues

When reporting a reproducible repository problem, include:

- product name and hardware revision when known,
- affected example or file path,
- exact ESP-IDF preview package, tag, or commit,
- build and flash commands,
- reproduction steps,
- expected and actual behavior,
- relevant build logs, serial output, photos, or screenshots,
- whether the board was flashed from source or with the checked-in test image.

ESP32-S31 is a preview target. A failure with a normal stable ESP-IDF release
does not by itself indicate an example defect; first confirm that the selected
toolchain explicitly supports `esp32s31`.

This repository currently provides ESP-IDF examples only. It does not provide
an Arduino board definition, Arduino sketches, or an Arduino CI target.

## Hardware And Order Support

For wiring, damaged hardware, warranty, or order-specific questions, contact
Waveshare through the official [support contact page](https://www.waveshare.com/contact_us).
Include the product model, hardware revision, order information, and clear
photos when relevant.

Security-sensitive reports should follow `SECURITY.md` instead of being posted
in a public issue.
