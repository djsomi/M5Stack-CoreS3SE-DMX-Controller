# AGENTS.md

## Project

**M5Stack CoreS3SE DMX Controller**

This repository contains a touchscreen DMX512 controller built around:

- M5Stack CoreS3SE
- M5Stack M5GO3 Bottom / Bottom3 battery base
- M5Stack Unit DMX U183 as the current known-good DMX interface
- M5Stack DMX Base as a second hardware target currently under investigation

The project is built with PlatformIO / Arduino and uses M5Unified, M5GFX, and `esp_dmx`.

---

## Primary Goal

Preserve the current **known-good CoreS3SE + Unit DMX U183 build** while cleaning the repository for public GitHub release.

The repository should eventually support two build targets from one source tree:

1. **CoreS3SE + Unit DMX U183**
2. **CoreS3SE + DMX Base**

The application logic should remain identical between the two targets. Only the DMX hardware profile should differ.

---

## Known-Good Hardware Configuration

### CoreS3SE + Unit DMX U183

Unit DMX is connected to **CoreS3SE Port A**.

```cpp
TX = GPIO_NUM_2;
RX = GPIO_NUM_1;
EN = -1;
```

Meaning:

- GPIO2: CoreS3SE TX -> Unit DMX UART RX
- GPIO1: CoreS3SE RX <- Unit DMX UART TX
- Unit DMX handles RS-485 direction automatically
- No external direction-enable GPIO is used

This is the current stable configuration.

Do not change these pins without a specific reason.

---

## DMX Base Hardware Configuration

For M5Stack CoreS3 / CoreS3SE + DMX Base:

```cpp
TX = GPIO_NUM_7;
RX = GPIO_NUM_10;
EN = GPIO_NUM_6;
```

Meaning:

- GPIO7: DMX data TX
- GPIO10: DMX RX
- GPIO6: RS-485 direction / enable

The DMX Base is currently being investigated because random DMX glitches were observed with it.

---

## Important Development Rule

### DO NOT create two copied application source trees.

Preferred architecture:

```text
src/
    application code

include/
    dmx_hardware.h

platformio.ini
```

with build-time hardware selection such as:

```cpp
DMX_HARDWARE_UNIT
DMX_HARDWARE_BASE
```

Example intent:

```cpp
#if defined(DMX_HARDWARE_UNIT)

static constexpr int DMX_TX_PIN = GPIO_NUM_2;
static constexpr int DMX_RX_PIN = GPIO_NUM_1;
static constexpr int DMX_EN_PIN = -1;

#elif defined(DMX_HARDWARE_BASE)

static constexpr int DMX_TX_PIN = GPIO_NUM_7;
static constexpr int DMX_RX_PIN = GPIO_NUM_10;
static constexpr int DMX_EN_PIN = GPIO_NUM_6;

#else
#error "Select a DMX hardware profile"
#endif
```

The same UI and DMX application code should compile for both.

---

## Current `esp_dmx` Version

The working application currently uses:

```text
esp_dmx v2.02
```

A newer v4.1.0 version was tested during DMX Base troubleshooting but is not the current application dependency.

---

## ESP32-S3 Compatibility Patch Required by esp_dmx v2.02

The current development environment required three local compatibility changes in:

```text
.pio/libdeps/.../esp_dmx/src/impl/dmx_ll.h
```

Changes:

```cpp
hw->uart_idle_conf_reg_t.tx_idle_num
```

became:

```cpp
hw->idle_conf.tx_idle_num
```

---

```cpp
hw->uart_txbrk_conf_reg_t.tx_brk_num
```

became:

```cpp
hw->txbrk_conf.tx_brk_num
```

---

```cpp
hw->uart_status_reg_t.rxd
```

became:

```cpp
hw->status.rxd
```

### Important

Do **not** treat editing `.pio/libdeps` manually as the final public solution.

For GitHub release, make this reproducible by one of the following:

- an automatic PlatformIO patch step,
- a small maintained fork,
- or another clean dependency mechanism.

Do not commit the `.pio` directory.

---

## Known-Good Functional Baseline

The current working controller includes:

- 512-channel DMX control
- touchscreen channel selection/editing
- smooth channel-list scrolling
- persistent presets P1-P8
- preset load/save
- protected preset save
- preset copy
- preset clear
- blackout
- persistent brightness setting
- persistent screen timeout
- persistent UI volume
- battery percentage
- charging / external-power indication
- battery runtime estimation
- low-battery warnings
- startup screen
- startup sound
- smooth startup progress bar
- software DMX heartbeat indicator
- Settings page
- smooth Settings scrolling using an off-screen canvas
- audio-safe Settings redraw handling
- software restart
- software power-off

Do not remove any of these during cleanup unless explicitly requested.

---

## Preset Behavior

There are 8 presets:

```text
P1 ... P8
```

Presets are persistent using Preferences/NVS.

Protected save:

- save requires approximately 1 second hold
- progress feedback is shown
- success tone occurs once

Long-pressing an occupied preset opens options:

- COPY
- CLEAR
- BACK

COPY:

- intended for copying to an empty preset slot
- occupied destination is blocked

CLEAR:

- requires approximately 1 second hold
- a persistent cleared-slot mask is used because the original preset storage layer did not provide delete behavior

Do not simplify this behavior without checking the existing implementation.

---

## Blackout Behavior

BLACKOUT uses a **double tap within 500 ms**.

First tap:

```text
TAP AGAIN
```

Second tap:

- stores/restores the pre-blackout state
- sends zero output while blackout is active

Blackout is intentionally protected against accidental activation.

---

## Settings Power Controls

Settings contains:

```text
RESTART
POWER OFF
BACK
```

RESTART and POWER OFF use the same style of double-tap protection:

- first tap -> `TAP AGAIN`
- second tap within approximately 500 ms -> action
- timeout cancels the armed state
- arming one cancels the other

Before restart or shutdown, several zero-DMX frames are sent.

Restart uses:

```cpp
esp_restart();
```

Power off uses:

```cpp
M5.Power.powerOff();
```

---

## Battery / Power Behavior

The controller uses an M5GO3 Bottom / Bottom3 battery.

Observed real runtime is approximately:

```text
~5 hours
```

depending on usage and brightness.

Battery UI currently includes:

- percentage
- charging status
- external-power status
- runtime estimate
- low-battery color changes
- low-battery sound warnings

Runtime estimate uses observed discharge history rather than a hard-coded duration.

Low-battery warning sound intentionally overrides normal UI volume and plays at a strong fixed volume, then restores the user's configured UI volume.

---

## UI Rendering Notes

### Main channel page

The main page is already smooth and uses clipped / buffered rendering.

### Settings page

The Settings page originally flashed because the complete LCD was cleared and redrawn during scrolling.

Current solution:

- render Settings to a 320x240 off-screen canvas
- push the completed frame to the physical display
- do not clear the physical LCD before each frame

Because a full-screen canvas transfer can interfere with short speaker tones, Settings rendering is deferred while the speaker is active and then performed immediately afterward.

Do not remove the audio-protection logic without testing sound quality.

### Startup loader

The startup loader originally flashed because it repeatedly cleared a 320x60 region.

Current solution:

- countdown text redraws only when the displayed second changes
- progress-bar outline is drawn once
- bar background is cleared once
- only newly added green progress pixels are written

Do not revert to full-area clearing each frame.

---

## Audio

Normal UI touch feedback is a short high-frequency click.

Startup uses a non-blocking rising multi-note chime.

UI volume has 5 positions:

```text
0 / 1 / 2 / 3 / 4
```

with:

```text
0 = mute
```

The previous default corresponds approximately to level 2.

Battery warning sounds use their own stronger fixed level and then restore the UI volume.

---

## Display Settings

Brightness has 5 levels.

Screen timeout options are:

```text
1 / 2 / 3 / 4 / ON
```

`ON` means:

```text
always on
```

and internally corresponds to no timeout.

The first touch after display sleep should only wake the display and must not activate a UI control.

---

## DMX Timing / Transmission

The project has undergone extensive DMX timing testing.

Do not casually change:

- packet size
- DMX frame cadence
- Break
- MAB
- transmission synchronization
- buffer ownership
- direction control

without documenting the reason and testing against real fixtures.

The working Unit DMX build should be treated as the reference.

---

## DMX Base Investigation

Random DMX glitches were observed using the M5Stack DMX Base.

The same CoreS3SE, fixtures, DMX cable, and application worked correctly with Unit DMX U183.

The Base issue has already been tested extensively.

See `PROJECT_STATUS.md` for the full test history.

Do not assume the Base issue is caused by the UI or application code unless new evidence demonstrates it.

---

## Diagnostic Test History

Several Base-specific diagnostic programs were used during troubleshooting, including:

- minimal static esp_dmx tests
- esp_dmx v4 tests
- GPIO6 permanently forced HIGH
- a raw RMT-based DMX generator

However, these exact test versions were **not all preserved as known-good archived source files**.

### Important publication rule

Do **not** recreate these tests from memory and publish them as if they were the original tested versions.

For the first public GitHub release:

- document that these tests were performed
- document the observed results
- publish only source that is known to match the current verified project state
- omit unverified reconstructed diagnostic programs

If diagnostic reproduction programs are needed later for M5Stack Support, create fresh tests deliberately, label them clearly as new reproduction tests, and verify them on hardware before committing them.

---

## Git / Repository Rules

Before major cleanup:

1. preserve the exact known-good working source
2. initialize Git
3. commit that state
4. perform cleanup/refactoring on a separate branch

Suggested baseline commit:

```text
Working CoreS3SE Unit DMX baseline
```

Suggested cleanup branch:

```text
cleanup/github-release
```

Do not rewrite the known-good baseline commit.

---

## Files That Should Not Be Committed

At minimum:

```gitignore
.pio/
.vscode/
.DS_Store
*.bin
*.elf
```

Generated PlatformIO dependency/build directories must not be committed.

---

## Public Repository Goal

Proposed repository name:

```text
M5Stack-CoreS3SE-DMX-Controller
```

Suggested initial public release:

```text
v0.1.0
```

Do not call the first public version v1.0 yet.

---

## First Task for a New Coding Agent

When opening this repository for the first time:

1. Read this file.
2. Read `PROJECT_STATUS.md`.
3. Inspect the complete repository.
4. Do **not modify anything yet**.
5. Identify:
   - current PlatformIO environments
   - current source/header layout
   - DMX initialization path
   - esp_dmx dependency
   - local/untracked patches
   - files required for a successful build
6. Build the current known-good Unit DMX target.
7. Report findings before proposing cleanup.

Only after the known-good build is confirmed should structural cleanup begin.
