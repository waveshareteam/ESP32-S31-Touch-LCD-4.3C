| Supported Targets | ESP32-S31 |
| ----------------- | --------- |

# A2DP Bluetooth Speaker via ESP Board Manager

This project is a standalone port of the original BSP example. It keeps the
Classic Bluetooth A2DP sink, AVRCP metadata, ES8389 speaker output, and the
800x480 LVGL music screen, but all board hardware is initialized through
`espressif/esp_board_manager`.

The Waveshare board definition is included under
`components/esp_boards/esp32_s31_touch_lcd_4_3c`, while
`main/idf_component.yml` declares only the example's direct dependencies.
The project does not reference files outside its own directory except
components shipped with the active ESP-IDF installation.

## First build

Run these commands from this project directory:

```text
idf.py set-target esp32s31
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
```

The Board Manager command reads the local board YAML files and automatically
generates
`components/gen_bmgr_codes`; no generated source needs to be copied or edited
manually.

## Features

Example of A2DP audio sink role

This is the example of API implementing Advanced Audio Distribution Profile to receive an audio stream.

This example involves the use of Bluetooth legacy profile A2DP for audio stream reception and I2S for audio stream output interface.

## Component dependencies

The main/idf_component.yml manifest declares every direct non-project
dependency. Bluetooth common helpers resolve from the active ESP-IDF
installation, while Board Manager and the LVGL adapter resolve through the ESP
Component Manager. Board Manager supplies esp_boards transitively.
Project-specific audio and UI code is included under components.

## How to use this example

### Hardware Required

- Waveshare ESP32-S31-Touch-LCD-4.3C
- A loudspeaker connected to the board speaker output

The example uses the on-board ES8389 and the board definition included in this
project. The relevant audio signals are fixed by that definition:

| Board signal | GPIO |
| :----------- | ---: |
| I2S BCLK     |   37 |
| I2S LRCK     |   38 |
| I2S DOUT     |   39 |
| I2S DIN      |   42 |
| PA enable    |   57 |

### Configure the project

```
idf.py menuconfig
```

The default configuration decodes SBC with Bluedroid and sends PCM to the
on-board ES8389 through the Board Manager audio device.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output.

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

After the program is started, the example starts inquiry scan and page scan, awaiting being discovered and connected. Other bluetooth devices such as smart phones can discover a device named "ESP_SPEAKER". A smartphone or another ESP-IDF example of A2DP source can be used to connect to the local device.

Once A2DP connection is set up, there will be a notification message with the remote device's bluetooth MAC address like the following:

```
I (106427) BT_AV: A2DP connection state: Connected, [64:a2:f9:69:57:a4]
```

If a smartphone is used to connect to local device, starting to play music with an APP will result in the transmission of audio stream. The transmitting of audio stream will be visible in the application log including a count of audio data packets, like this:

```
I (120627) BT_AV: A2DP audio state: Started
I (122697) BT_AV: Audio packet count 100
I (124697) BT_AV: Audio packet count 200
I (126697) BT_AV: Audio packet count 300
I (128697) BT_AV: Audio packet count 400
```

Audio is played through the on-board ES8389 and the GPIO57-controlled speaker
power amplifier.

## Troubleshooting
* At the current stage, the supported audio codecs are SBC and AAC. If the internal A2DP codec is used, only SBC is supported; if the external A2DP codec is used, both SBC and AAC are supported, which can be achieved by registering Stream Endpoints (SEPs). Additionally, both internal and external codec options can be reconfigured via API calls.
* As a usage limitation, A2DP sink can support at most one connection with remote A2DP source devices. Also, A2DP sink cannot be used together with A2DP source at the same time, but can be used with other profiles such as SPP and HFP.
