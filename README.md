# M5Stack CoreS3SE DMX Controller

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange)
![DMX512](https://img.shields.io/badge/Protocol-DMX512-blue)
![M5Stack](https://img.shields.io/badge/Hardware-M5Stack-red)
![License](https://img.shields.io/badge/License-MIT-green)

A compact, battery-powered **DMX512 touchscreen controller** built around the **M5Stack CoreS3SE**.

The project started from M5Stack's official **DMX512Tools** example and has been extensively modified into a practical standalone controller with a redesigned touchscreen UI, persistent presets, blackout, battery monitoring, settings, software restart/power-off, and support for two DMX hardware profiles.

> [!IMPORTANT]
> **Recommended / tested setup:** M5Stack CoreS3SE + M5Stack Unit DMX (U183) + M5GO Bottom3.

> [!WARNING]
> **DMX Base support is currently experimental.**
>
> On the DMX Base unit tested during development, random DMX output glitches could be reproduced even with static channel values and simplified test firmware. The same CoreS3SE, fixtures and cabling operated without those glitches when using Unit DMX. This is currently being investigated with M5Stack support and should not be interpreted as a claim that all DMX Base units are affected.

---

## Gallery


### Hardware

<table>
  <tr>
    <td align="center">
      <img src="docs/images/hero.jpg" width="500"><br>
      <b>CoreS3SE DMX Controller</b>
    </td>
  </tr>
</table>

<table>
  <tr>
    <td align="center">
      <img src="docs/images/unit-hardware.jpg" width="300"><br>
      <b>Recommended: CoreS3SE + Unit DMX</b>
    </td>
    <td align="center">
      <img src="docs/images/base-hardware.jpg" width="300"><br>
      <b>Experimental: CoreS3SE + DMX Base</b>
    </td>
  </tr>
</table>

### Application Screens

<table>
  <tr>
    <td align="center">
      <img src="docs/images/main-screen.jpg" width="280"><br>
      <b>Main DMX Screen</b>
    </td>
    <td align="center">
      <img src="docs/images/settings-screen.jpg" width="280"><br>
      <b>Settings</b>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/images/presets-screen.jpg" width="280"><br>
      <b>Presets</b>
    </td>
    <td align="center">
      <img src="docs/images/startup-screen.jpg" width="280"><br>
      <b>Startup Screen</b>
    </td>
  </tr>
</table>

---

## Features

- Full **512-channel DMX universe**
- Touchscreen channel selection and value editing
- Persistent **P1-P8 presets**
- Protected preset save using hold-to-save
- Preset **copy** and **clear**
- **Blackout** with double-tap protection
- DMX activity / heartbeat indicator
- Battery percentage display
- Charging and external-power indication
- Battery runtime estimation after calibration
- Low-battery visual and audible warnings
- Persistent display brightness setting
- Persistent screen timeout setting
- Persistent UI volume setting
- Screen always-on option
- Smooth kinetic scrolling in Settings
- Flicker-reduced off-screen Settings rendering
- Smooth incremental startup progress bar
- Startup sound / chime
- Software **RESTART**
- Software **POWER OFF**
- Double-tap protection for restart and power-off
- Separate hardware profiles for:
  - **M5Stack Unit DMX U183**
  - **M5Stack DMX Base**

---

## Hardware

### Recommended Setup

- [M5Stack CoreS3SE](https://docs.m5stack.com/en/core/M5CoreS3%20SE)
- [M5Stack Unit DMX U183](https://docs.m5stack.com/en/unit/Unit-DMX)
- [M5Stack M5GO Bottom3](https://docs.m5stack.com/en/module/M5GO3%20Bottom)

The M5GO Bottom3 contains a **3.7 V / 500 mAh** battery. In the tested controller configuration, practical runtime was approximately **5 hours**, depending on display brightness, activity and battery condition.

### Experimental Hardware

- [M5Stack DMX Base](https://docs.m5stack.com/en/base/DMX_Base)

---

## DMX Hardware Profiles

The application code is shared. Hardware-specific UART / direction-control settings are selected by the PlatformIO build environment.

### Unit DMX U183

The Unit DMX is connected to **CoreS3SE Port A**.

| Signal | CoreS3SE GPIO |
|---|---:|
| DMX TX | GPIO 2 |
| DMX RX | GPIO 1 |
| Direction / Enable | `-1` |
| Port | Port A |

The Unit DMX handles the RS-485 direction internally, therefore the firmware does not use a separate enable pin.

Example configuration:

```cpp
transmitPin = GPIO_NUM_2;
receivePin  = GPIO_NUM_1;
enablePin   = -1;
```

### DMX Base

The DMX Base uses the CoreS3 M-Bus UART / enable pins used by M5Stack's original DMX512Tools example.

| Signal | CoreS3SE GPIO |
|---|---:|
| DMX TX | GPIO 7 |
| DMX RX | GPIO 10 |
| RS485 Enable | GPIO 6 |

Example configuration:

```cpp
transmitPin = GPIO_NUM_7;
receivePin  = GPIO_NUM_10;
enablePin   = GPIO_NUM_6;
```

---

## PlatformIO Build Environments

The recommended repository layout is to keep one application source tree and expose two PlatformIO environments.

```text
.
├── platformio.ini
├── src/
│   ├── src.cpp
│   ├── view_sender.h
│   └── ...
├── docs/
│   └── images/
│       ├── hero.jpg
│       ├── main-screen.jpg
│       ├── settings-screen.jpg
│       ├── presets-screen.jpg
│       ├── startup-screen.jpg
│       ├── unit-hardware.jpg
│       └── base-hardware.jpg
├── LICENSE
└── README.md
```

Suggested environment names:

```text
cores3se_unit
cores3se_base
```

### Build Unit DMX version

```bash
pio run -e cores3se_unit
```

### Upload Unit DMX version

```bash
pio run -e cores3se_unit -t upload
```

### Build DMX Base version

```bash
pio run -e cores3se_base
```

### Upload DMX Base version

```bash
pio run -e cores3se_base -t upload
```

> [!NOTE]
> If your actual `platformio.ini` uses different environment names, update the commands above to match the repository.

---

## Software Dependencies

The project is built with PlatformIO and uses:

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)
- [esp_dmx](https://github.com/someweisguy/esp_dmx)

The current application branch was developed around **esp_dmx v2.02** because it matches the API used by the original application.

### CoreS3SE compatibility note

With esp_dmx v2.02 on newer ESP32-S3 toolchains, the library may require compatibility changes in `dmx_ll.h` for renamed ESP-IDF UART register members.

The changes used during development were:

```cpp
hw->uart_idle_conf_reg_t.tx_idle_num
```

to:

```cpp
hw->idle_conf.tx_idle_num
```

```cpp
hw->uart_txbrk_conf_reg_t.tx_brk_num
```

to:

```cpp
hw->txbrk_conf.tx_brk_num
```

and:

```cpp
hw->uart_status_reg_t.rxd
```

to:

```cpp
hw->status.rxd
```

If these patches are included in the repository, they should be documented or automated rather than requiring users to modify `.pio/libdeps` manually.

---

## DMX Base Issue Under Investigation

The DMX Base profile is included primarily so the behavior can be reproduced and investigated.

### Observed behavior

Random DMX output glitches / fixture flickers were observed with the tested DMX Base hardware, including when transmitting static DMX values.

### Tests already performed

The following were tested while isolating the problem:

- Proper DMX cables
- 120 ohm termination
- DMX Base bias resistor enabled
- Different fixtures
- Individual fixture testing
- RGBWAUV fixtures
- Different 12 V power supplies
- USB-powered CoreS3SE
- `esp_dmx` v2.02
- `esp_dmx` v4.x
- Full 512-channel universe
- Approximately 30 FPS refresh
- Different DMX Break / MAB timings
- Waiting for transmission completion
- Minimal static-value transmitter firmware
- Explicitly holding the DMX Base RS485 enable pin active
- Raw RMT-based DMX generation, bypassing the normal UART / esp_dmx transmit path

The glitches remained reproducible with the tested DMX Base.

### Comparison test

The same:

- CoreS3SE
- fixtures
- DMX cables
- termination
- application logic

were then tested with **M5Stack Unit DMX U183**.

The Unit DMX version operated without the random glitches during extended testing.

This makes the two build profiles useful for support/debugging:

```text
cores3se_base  -> reproduces the issue on the tested DMX Base
cores3se_unit  -> working comparison using Unit DMX
```

---

## Presets

Eight presets are available:

```text
P1 P2 P3 P4
P5 P6 P7 P8
```

Preset data is stored persistently in ESP32 NVS.

### Save

Saving is protected against accidental taps.

- Select a preset
- Hold the SAVE control for approximately 1 second
- Progress is shown while holding
- Releasing early cancels the save

### Copy / Clear

Long-press an occupied preset slot to open preset options.

Available operations:

- `COPY`
- `CLEAR`
- `BACK`

Copying is only allowed to an empty destination slot.

Clearing is protected by a hold action.

---

## Blackout

BLACKOUT uses double-tap protection.

```text
First tap  -> TAP AGAIN
Second tap -> BLACKOUT
```

The second tap must arrive within approximately **500 ms**.

When blackout is activated, the current DMX universe is preserved and zero values are transmitted.

Double-tapping again restores the previous values.

---

## Settings

The Settings screen provides:

### Brightness

Five levels:

```text
1 / 2 / 3 / 4 / 5
```

### Screen Timeout

```text
1 / 2 / 3 / 4 / ON
```

`ON` means the display remains on continuously.

### Volume

Five positions:

```text
0 / 1 / 2 / 3 / 4
```

`0` is mute.

### Software Power Controls

The Settings screen also contains:

```text
RESTART
POWER OFF
BACK
```

Both RESTART and POWER OFF require a double tap within approximately **500 ms**.

Before restart / shutdown, the firmware sends several all-zero DMX frames.

---

## Battery Monitoring

The UI displays:

- Battery percentage
- Charging status
- External-power status
- Estimated remaining runtime

Battery indicator colors:

| Battery Level | Color |
|---|---|
| 36-100% | Green |
| 21-35% | Yellow |
| 0-20% | Red |

Runtime estimation begins only after enough discharge data has been collected.

The estimator uses a rolling observation window and smooths the calculated runtime so the displayed value does not jump excessively.

Low-battery warnings are generated at approximately:

- **20%**
- **10%**

Battery-warning tones use a fixed high warning volume independent of the normal UI volume.

---

## User Interface Rendering

The application contains several rendering optimizations for the CoreS3SE display.

### Settings

The Settings page is rendered to an off-screen framebuffer and pushed to the display as a completed frame.

This avoids the visible black-frame flashing that occurred when the complete physical display was cleared and redrawn during kinetic scrolling.

Short audio tones are protected from heavy display transfers to avoid distorted / chopped speaker output.

### Startup Screen

The startup progress bar is updated incrementally.

Instead of clearing and redrawing the entire lower screen area every frame, only newly added progress pixels are written.

This reduces:

- display flashing
- display traffic
- interference with startup audio

---

## Original Source / Acknowledgements

This project is based on and heavily extended from the official M5Stack:

### [DMX512Tools example](https://github.com/m5stack/M5Module-DMX512/tree/master/examples/DMX512Tools)

from the:

### [M5Stack M5Module-DMX512 repository](https://github.com/m5stack/M5Module-DMX512)

The upstream M5Stack repository is distributed under the **MIT License**.

The original example provided the foundation for the DMX tool architecture. This project adds substantial modifications and new functionality, including:

- CoreS3SE adaptation
- Unit DMX U183 support
- DMX Base hardware profile
- redesigned touchscreen sender UI
- persistent presets
- preset copy / clear
- protected preset saving
- blackout with double-tap protection
- battery monitoring
- battery runtime estimation
- low-battery warnings
- persistent brightness / timeout / volume settings
- startup screen
- startup audio
- software restart
- software power-off
- DMX activity indicator
- improved display rendering
- smooth Settings scrolling

Many thanks to **M5Stack** for publishing the original DMX512Tools example and making it available for further development.

---

## License

This project is released under the **MIT License**.

Because this project is derived from M5Stack's MIT-licensed DMX512Tools example, the original M5Stack copyright and MIT license notice must be retained for portions derived from the upstream source.

A repository `LICENSE` file should therefore preserve the upstream MIT notice and clearly identify the additional modifications / contributions made in this project.

Example attribution wording:

```text
Original DMX512Tools source:
Copyright (c) M5Stack

Additional modifications and project-specific code:
Copyright (c) 2026 Zoltan Somogyvari
```

The exact upstream license text should be preserved in the repository `LICENSE` file.

---

## Releases

Pre-built firmware binaries can be published through the repository's **Releases** page.

Recommended release artifacts:

```text
cores3se-unit-firmware.bin
cores3se-base-firmware.bin
```

Recommended versioning:

```text
v0.1.0
v0.2.0
v1.0.0
```

A release should clearly identify whether the binary is intended for:

- **Unit DMX U183**
- **DMX Base**

Do not flash a binary built for the wrong hardware profile.

---

## Roadmap

Possible future development:

- ESP-NOW wireless DMX transmitter mode
- Small ESP32 / AtomS3 wireless DMX receiver
- Link-quality / RSSI indicator
- Wireless receiver pairing
- Multiple receiver support
- Art-Net / sACN support
- CRMX-compatible hardware exploration
- Fixture profiles
- Named channels
- Preset export / import
- Additional battery statistics
- Automated GitHub Actions builds
- Pre-built firmware attached automatically to GitHub Releases

---

## Contributing

Issues, test results and pull requests are welcome.

For DMX Base testing, useful reports should include:

- exact M5Stack host model
- DMX Base revision if known
- power supply
- fixture model
- termination configuration
- firmware build environment
- esp_dmx version
- whether the issue also occurs with static DMX values

Please avoid reporting a DMX Base issue as confirmed hardware failure unless the problem has been isolated with reproducible testing.

---

## Disclaimer

DMX512 controls lighting and related equipment. Test new firmware in a safe environment before using it during a live event.

This project is an independent community project and is **not an official M5Stack product**.
