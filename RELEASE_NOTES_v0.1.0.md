# M5Stack CoreS3SE DMX Controller v0.1.0

First public release of the M5Stack CoreS3SE DMX Controller.

## Highlights

- Unit DMX U183 is the physically tested and recommended DMX interface.
- Two compile-time hardware profiles are provided from one application source tree:
  - `cores3se_unit`
  - `cores3se_base`
- PlatformIO dependency stack is pinned for reproducible builds.
- Upstream `esp_dmx 2.0.2` is kept and patched automatically for CoreS3SE / ESP32-S3 compatibility.
- Controller features include 512 DMX channels, presets P1-P8, blackout, persistent brightness/timeout/volume settings, battery/status display, restart, and power-off.
- CoreS3SE USB cold-start power initialization is fixed by starting M5Unified with external output disabled.
- Unit DMX currently requires the Battery Bottom for external Unit power.
- DMX Base profile is experimental because random DMX output glitches remain reproducible on tested physical fixtures.

## Recommended Firmware

Use:

```text
M5Stack-CoreS3SE-DMX-v0.1.0-UNIT.bin
```

for:

```text
CoreS3SE + Unit DMX U183 + Battery Bottom
```

## Experimental Firmware

Use:

```text
M5Stack-CoreS3SE-DMX-v0.1.0-BASE-EXPERIMENTAL.bin
```

only for:

```text
CoreS3SE + M5Stack DMX Base + external PSU
```

The Base firmware is included for reproduction and investigation with M5Stack Support.
