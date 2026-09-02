# M5Stack CoreS3SE DMX Controller

A touchscreen DMX512 controller for the M5Stack CoreS3SE, built with PlatformIO and Arduino.

The recommended and physically tested configuration for this release is:

```text
CoreS3SE + M5Stack Unit DMX U183 + M5GO3 Bottom / Bottom3 battery
```

The repository also includes an experimental M5Stack DMX Base hardware profile so the same application can be built for comparison and investigation with M5Stack Support.

## Supported Hardware

### Recommended: Unit DMX U183

Use the `cores3se_unit` PlatformIO environment.

Unit DMX U183 is connected to CoreS3SE Port A:

```text
TX: GPIO2
RX: GPIO1
EN: unused / automatic direction
```

Unit DMX U183 is stable in physical fixture testing. In the current M5Unified configuration, the Battery Bottom must be attached for the Unit DMX module to receive external 5 V power. The bare CoreS3SE itself can cold-start from USB without the battery, but the Unit DMX module remains unpowered without the battery.

### Experimental: M5Stack DMX Base

Use the `cores3se_base` PlatformIO environment.

DMX Base pin mapping:

```text
TX: GPIO7
RX: GPIO10
EN: GPIO6
```

The DMX Base requires its external PSU. This profile is experimental because random DMX output glitches remain reproducible on tested physical fixtures. It is included primarily for reproduction and investigation with M5Stack Support.

## Features

- 512-channel DMX output
- Touchscreen channel selection and editing
- Persistent presets P1-P8
- Protected preset save, copy, and clear
- Protected blackout
- Persistent brightness, screen timeout, and UI volume settings
- Battery percentage, charging, external-power, and runtime display
- Low-battery warnings
- Startup screen and startup sound
- Software restart and power-off controls

## Build Environments

```text
cores3se_unit
cores3se_base
```

`cores3se_unit` is the default environment.

Build the recommended Unit DMX firmware:

```bash
pio run -e cores3se_unit
```

Build the experimental DMX Base firmware:

```bash
pio run -e cores3se_base
```

Upload examples:

```bash
pio run -e cores3se_unit -t upload --upload-port COM8
pio run -e cores3se_base -t upload --upload-port COM8
```

Adjust the upload port for your system.

## Pinned Toolchain And Dependencies

The reproducible PlatformIO configuration pins:

```text
platform: espressif32@6.7.0
board: esp32-s3-devkitc-1
framework: arduino
M5Unified: M5Stack/M5Unified@0.2.21
M5GFX: M5Stack/M5GFX@0.2.28
esp_dmx: upstream v2.02 archive
```

Verified PlatformIO package versions from the release build:

```text
framework-arduinoespressif32: 3.20016.0 / Arduino ESP32 2.0.16
tool-esptoolpy: 1.40501.0 / esptool.py 4.5.1
toolchain-riscv32-esp: 8.4.0+2021r2-patch5
toolchain-xtensa-esp32s3: 8.4.0+2021r2-patch5
```

## esp_dmx 2.0.2 Patch

This project intentionally uses upstream `esp_dmx v2.02`.

For ESP32-S3 compatibility with the pinned toolchain, PlatformIO runs a repository-local pre-build script:

```text
scripts/patch_esp_dmx_v202.py
```

The script applies three deterministic source compatibility changes inside the downloaded `esp_dmx` dependency before `esp_dmx` is compiled:

```text
hw->uart_idle_conf_reg_t.tx_idle_num  ->  hw->idle_conf.tx_idle_num
hw->uart_txbrk_conf_reg_t.tx_brk_num  ->  hw->txbrk_conf.tx_brk_num
hw->uart_status_reg_t.rxd             ->  hw->status.rxd
```

The patch is idempotent, accepts an already patched dependency, fails on mixed partial state, and fails clearly if the expected `esp_dmx v2.02` source text is different.

## Power Behavior

The firmware starts M5Unified with external output disabled so a bare CoreS3SE can cold-start from USB:

```cpp
auto cfg = M5.config();
cfg.output_power = false;
M5.begin(cfg);
```

For the Unit DMX profile only, the firmware enables external output after startup when the CoreS3SE AXP2101 reports that a battery is physically present. This keeps USB-only cold-start working while still powering Unit DMX when the Battery Bottom is attached. The firmware uses `M5.Power.setExtOutput(true)` and does not bypass M5Unified's safety guard.

For the DMX Base profile, external output remains disabled; the Base is powered by its external PSU.

## Known Issues

### DMX Base Glitches

The `cores3se_base` profile is experimental. With the same CoreS3SE, application, fixtures, and DMX cabling, the M5Stack DMX Base produced random DMX output glitches on physical fixtures, while Unit DMX U183 remained stable.

Historical diagnostic tests included minimal firmware, alternate DMX library versions, forced GPIO6 direction enable, and raw RMT-generated DMX. The exact historical diagnostic source files were not all preserved as verified archives, so this repository does not publish reconstructed diagnostic programs as if they were the original tested versions.

## Release

Initial public release:

```text
v0.1.0
```

Unit DMX U183 is the recommended interface for real use. DMX Base support is included for controlled reproduction and support investigation.
