# ESP DMX Controller — ESP32-S3 / M5Stack CoreS3SE

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange)
![DMX512](https://img.shields.io/badge/Protocol-DMX512-blue)
![M5Stack](https://img.shields.io/badge/Hardware-M5Stack-red)
![License](https://img.shields.io/badge/License-MIT-green)

An **open-source, portable ESP32 DMX512 lighting controller** for the **M5Stack CoreS3SE**. Control all 512 DMX channels from the touchscreen or an offline Wi-Fi web interface, with presets, blackout, battery monitoring and synchronized settings.

> [!IMPORTANT]
> **Recommended and tested:** M5Stack CoreS3SE + Unit DMX (U183) + M5GO Bottom3.

> [!WARNING]
> **Base DMX support is experimental.** Random output glitches occurred on the Base DMX unit tested during development, even with static channel values and simplified firmware. The same CoreS3SE, fixtures and cabling worked without those glitches using Unit DMX. M5Stack support is investigating; this does **not** establish that all Base DMX units are affected.

<p align="center">
  <img src="docs/images/hero.jpg" width="100%" alt="ESP32 DMX Controller – M5Stack CoreS3SE portable DMX512 lighting controller with Wi-Fi web interface">
</p>

<table width="100%">
  <tr>
    <td align="center" width="50%">
      <img src="docs/images/unit-hardware.jpg" width="100%" alt="M5Stack CoreS3SE portable DMX controller with Unit DMX U183"><br>
      <b>CoreS3SE + Unit DMX</b>
    </td>
    <td align="center" width="50%">
      <img src="docs/images/base-hardware.jpg" width="100%" alt="M5Stack CoreS3SE DMX controller with M5Stack DMX Base"><br>
      <b>CoreS3SE + Base DMX (experimental)</b>
    </td>
  </tr>
</table>

## Device Screenshots

<table>
  <tr>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-startup-screen.jpg" width="280" alt="ESP DMX Controller startup screen"><br>
      <b>Startup</b>
    </td>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-main-screen.jpg" width="280" alt="ESP DMX Controller main DMX channel screen"><br>
      <b>Main Screen</b>
    </td>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-editor-screen.jpg" width="280" alt="ESP DMX Controller precision value editor"><br>
      <b>Value Editor</b>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-preset-screen.jpg" width="280" alt="ESP DMX Controller preset management screen"><br>
      <b>Presets</b>
    </td>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-settings-screen.jpg" width="280" alt="ESP DMX Controller device settings screen"><br>
      <b>Settings</b>
    </td>
    <td align="center">
      <img src="docs/images/esp-dmx-controller-settings-screen-2.jpg" width="280" alt="ESP DMX Controller additional settings screen"><br>
      <b>Additional Settings</b>
    </td>
  </tr>
</table>

---

## v0.3.0 — Wi-Fi & Web Control

Connect to the controller's Wi-Fi using the credentials shown in its physical Settings screen, then open **http://192.168.4.1/**. The web interface works offline on desktop, tablet and mobile browsers, including iPhone Home Screen mode. Web Editor, Presets, Settings and Status share live controller state.

- **Editor:** All 512 channels, with faders, fine adjustments and numeric entry.
- **Presets:** Load (double-tap), save and delete P1–P8. Overwrites and deletions require confirmation.
- **JSON Backup:** Transactional export/import of all eight preset slots, with timestamped downloads and progress dialogs. Import replaces every slot, including empty ones, without changing live DMX output. **Keep the browser tab active** during transfers.
- **Blackout:** Transmits zeros while preserving the entry snapshot. Leaving blackout restores that snapshot; edits made during blackout are discarded.
- **Settings & Maintenance:** Live previews, persistent settings saved on release, physical/web synchronization, browser preferences and confirmed restart/power-off. Maintenance actions allow five seconds for physical **CANCEL**.
- **Wi-Fi Configuration:** Credential changes require physical **OK** within 60 seconds. A two-second **HOLD RESET WIFI** schedules a Wi-Fi-only reset on the next explicit restart; presets and device settings are retained.

### Web Interface Screenshots

<table>
  <tr>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-web-ui-editor.jpg">
        <img src="docs/images/esp-dmx-controller-web-ui-editor.jpg" width="260" alt="ESP32 DMX Controller desktop Web Editor">
      </a><br><b>DMX Editor</b>
    </td>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-web-ui-preset.jpg">
        <img src="docs/images/esp-dmx-controller-web-ui-preset.jpg" width="260" alt="ESP32 DMX Controller desktop presets and backup">
      </a><br><b>Presets & Backup</b>
    </td>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-web-ui-settings.jpg">
        <img src="docs/images/esp-dmx-controller-web-ui-settings.jpg" width="260" alt="ESP32 DMX Controller desktop device and Wi-Fi settings">
      </a><br><b>Device Settings</b>
    </td>
  </tr>
</table>

### Mobile Interface Screenshots

<table>
  <tr>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-mobile-ui-editor.jpg">
        <img src="docs/images/esp-dmx-controller-mobile-ui-editor.jpg" width="260" alt="ESP32 DMX Controller mobile Web Editor">
      </a><br><b>DMX Editor</b>
    </td>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-mobile-ui-preset.jpg">
        <img src="docs/images/esp-dmx-controller-mobile-ui-preset.jpg" width="260" alt="ESP32 DMX Controller mobile presets and backup">
      </a><br><b>Presets & Backup</b>
    </td>
    <td align="center" width="33%">
      <a href="docs/images/esp-dmx-controller-mobile-ui-settings.jpg">
        <img src="docs/images/esp-dmx-controller-mobile-ui-settings.jpg" width="260" alt="ESP32 DMX Controller mobile device and Wi-Fi settings">
      </a><br><b>Device Settings</b>
    </td>
  </tr>
</table>

### Firmware & Updates

Two factory images are built from the v0.3.0 release source:

- `CoreS3SE-DMX-Unit-v0.3.0.bin` — **recommended**, Unit DMX U183 + Battery Bottom.
- `CoreS3SE-DMX-Base-v0.3.0.bin` — **experimental**, Base DMX + external PSU.

- **From v0.2.0:** Erase the entire flash and install the matching **factory BIN at `0x0`**. Presets and settings will be lost.
- **For the tested current-format v0.3.0:** An **application-only** update without erase preserves presets and settings. Do **not** use a factory BIN for a data-preserving update: it also covers the NVS area.
- **Downgrading:** Export your data first. v0.2.0 cannot read the generation-backed preset storage used by v0.3.0.

### Security & Known Limitations

> [!CAUTION]
> **Change the predictable factory Wi-Fi password before use in untrusted environments.** Anyone who can join the access point can access DMX control, presets and maintenance. Physical approval protects **Wi-Fi configuration changes only**; connected clients otherwise share control access. HTTP/WebSocket traffic has no application-layer encryption.

The device runs as a Wi-Fi access point with embedded HTTP/WebSocket assets and automatic connection recovery (no STA/AP+STA mode or OTA updater). Mobile browser zoom is intentionally restricted to avoid an iOS gesture regression. A slow TCP/WebSocket client can temporarily block web control, and external DMX timing/coherence validation remains outstanding.

**Intel AX200 compatibility:** Driver `24.20.2.1` has a user-reported SoftAP issue. Verified client workaround: **Device Manager → Intel Wi-Fi 6 AX200 → Advanced → MIMO Power Save Mode → No SMPS**. This is not a confirmed firmware defect.

**Documentation:** [Installation & Updates](docs/V0_3_0_INSTALLATION.md) · [Changelog](CHANGELOG.md) · [Release Notes](docs/V0_3_0_RELEASE_NOTES_DRAFT.md) · [Known Issues](KNOWN_ISSUES.md) · [Preset Backup Guide](docs/PRESET_EXPORT_IMPORT.md)

---

## Hardware & Power

**Recommended:** [CoreS3SE](https://docs.m5stack.com/en/core/M5CoreS3%20SE) + [Unit DMX U183](https://docs.m5stack.com/en/unit/Unit-DMX) + [M5GO Bottom3](https://docs.m5stack.com/en/module/M5GO3%20Bottom).

**Experimental:** [M5Stack Base DMX](https://docs.m5stack.com/en/base/DMX_Base).

> [!IMPORTANT]
> The tested **Unit DMX setup requires the battery bottom** to power the DMX hardware; **Base DMX requires an external power supply**.

The M5GO Bottom3 has a **3.7 V / 500 mAh** battery. The tested configuration ran for approximately **5 hours**, depending on brightness, activity and battery condition.

## On-Device Controls

### DMX Channel Editor

Select a channel on the main screen and adjust its value with the slider, or tap **EDIT** for precise control:

- **+ / −:** Adjust the value, or hold for continuous adjustment.
- **STEP:** Select increments of **1, 5, 10 or 25**.
- **Numeric entry:** Tap the value to enter an exact number from **0–255**.
- **Fader:** Adjust the channel value directly; **BACK** returns to the main screen.

Changes are sent immediately to the live DMX output. The main screen includes a DMX activity/heartbeat indicator.

### Physical Presets

Eight presets (**P1–P8**) are stored persistently in ESP32 NVS.

- **Load:** Double-tap a preset.
- **Save:** Select a slot and hold **SAVE** for approximately one second. Releasing early cancels the operation.
- **Copy / Clear:** Long-press an occupied slot. **COPY** only targets an empty slot; **CLEAR** requires a hold action.

### Physical Blackout

Double-tap **BLACKOUT** to send zero values while saving the current DMX universe. Double-tap again to restore the saved values. The first tap prompts **TAP AGAIN** to prevent accidental activation.

### Device Settings & Power

| Setting | Options | Notes |
|---|---|---|
| Brightness | `1 / 2 / 3 / 4 / 5` | Five levels |
| Screen Timeout | `1 / 2 / 3 / 4 / ON` | `ON` keeps the display on |
| Volume | `0 / 1 / 2 / 3 / 4` | `0` mutes UI sounds |

Settings are stored persistently and include touch and startup sounds. Physical **RESTART** and **POWER OFF** require a double tap.

### Battery Monitoring

The screen shows battery percentage, charging/external-power status and estimated remaining runtime.

| Battery Level | Indicator |
|---|---|
| 36–100% | Green |
| 21–35% | Yellow |
| 0–20% | Red |

Runtime estimates appear after calibration has collected enough discharge data and are smoothed over a rolling window. Visual and audible warnings occur at approximately **20%** and **10%**; warning tones use a fixed high volume regardless of the normal UI volume.

**Known touchscreen limitation:** Slow continuous fader movement may intermittently stall near the touch-acquisition layer. No application-level cause has been demonstrated, and occurrence in the original firmware is unconfirmed. The raw-coordinate correction remains; diagnostic tools are opt-in and `THDIFF` register-write experiments are suspended. See [Testing](docs/TESTING.md).

---

## Development & Builds

### DMX Hardware Profiles

The same firmware source supports two PlatformIO build environments. Hardware-specific UART and direction-control pins differ:

| Signal | Unit DMX U183 (Port A) | Base DMX (M-Bus) |
|---|---|---|
| DMX TX | GPIO 2 | GPIO 7 |
| DMX RX | GPIO 1 | GPIO 10 |
| Direction / Enable | `-1` (not used) | GPIO 6 |
| PlatformIO environment | `cores3se_unit` (default) | `cores3se_base` |

Unit DMX handles RS-485 direction internally; Base DMX uses the M-Bus pins from M5Stack's original DMX512Tools example.

### PlatformIO Commands

```bash
# Build
pio run -e cores3se_unit
pio run -e cores3se_base

# Upload
pio run -e cores3se_unit -t upload
pio run -e cores3se_base -t upload
```

### Software Dependencies

- PlatformIO `espressif32@6.7.0`, Arduino 2.0.16 / ESP-IDF 4.4.7
- [M5Unified 0.2.21](https://github.com/m5stack/M5Unified)
- [M5GFX 0.2.28](https://github.com/m5stack/M5GFX)
- [esp_dmx v2.02](https://github.com/someweisguy/esp_dmx)

### Reproducible Builds & Release Identity

Repository-local PlatformIO hooks apply and verify the reviewed ESP32-S3 compatibility and TX ownership/`RS485_CLASH` patches to upstream `esp_dmx` v2.02. **Do not manually edit `.pio/libdeps`.** Compressed web assets are built deterministically into the firmware; no filesystem upload is needed.

The read-only `/status` endpoint and web Status page show the version, build environment, Git commit, modified-input flag and source fingerprint. Release binaries must match committed source and checksums. Pre-commit builds are marked `modified`; rebuild after committing to obtain a clean release identity. Historical release candidates, source archives and receipts must not be relabeled.

Only Unit/Base production builds are packaged; diagnostics are developer checks. Two OTA slots reserve space but **do not** implement an OTA updater or validated rollback. Slow synchronous WebSocket sends, upper-flash writes, external DMX timing/coherence testing and physical fader precision remain documented limitations or deferred work. See [Release Notes](docs/V0_3_0_RELEASE_NOTES_DRAFT.md) and [Testing](docs/TESTING.md).

---

## Roadmap

Potential future work includes:

- **Wireless DMX:** ESP-NOW transmitter, ESP32/AtomS3 receivers, pairing, signal-quality indication and multiple receivers.
- **Protocols:** Art-Net / sACN support and CRMX-compatible hardware exploration.
- **Usability:** Fixture profiles, named channels and additional battery statistics.

## Acknowledgements

Based on and extensively developed from M5Stack's [DMX512Tools example](https://github.com/m5stack/M5Module-DMX512/tree/master/examples/DMX512Tools) in the [M5Module-DMX512 repository](https://github.com/m5stack/M5Module-DMX512), originally published under the MIT License. Thanks to **M5Stack** for making the example available.

## License & Third-Party Notices

This project is distributed under the **MIT License**. [LICENSE](LICENSE) retains the original **Copyright (c) 2022 M5Stack**, the upstream license text and the additional-project notice.

[Third-party notices](THIRD_PARTY_NOTICES.md) detail audited libraries, SDKs and runtime components. Some M5GFX subcomponents carry BSD or other terms, and Arduino-ESP32 is LGPL-2.1-or-later; the project's MIT License does not replace those obligations.

Firmware distribution must include the required notices and full license texts. Before publishing binaries, follow the [source/relink delivery guide](docs/V0_3_0_LGPL_DELIVERY.md) and [build-material inventory](docs/V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md). See [Publication Readiness](docs/V0_3_0_PUBLICATION_READINESS.md) for final commit and source/binary checksum requirements.

## Contributing

Issues, test results and pull requests are welcome. For Base DMX reports, please include the **CoreS3SE host model, Base revision (if known), PSU, fixture, termination, build environment, `esp_dmx` version** and whether glitches occur with static channel values. Avoid generalizing a single tested Base DMX issue to all hardware without reproducible evidence.

## Disclaimer

DMX512 controls lighting and related equipment. Test new firmware safely before using it at a live event. This is an independent community project, **not an official M5Stack product**.
