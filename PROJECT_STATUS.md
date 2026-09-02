# PROJECT_STATUS.md

## Project Status

**M5Stack CoreS3SE DMX Controller**

Current status:

```text
CoreS3SE + Unit DMX U183: WORKING / STABLE
CoreS3SE + DMX Base: RANDOM DMX GLITCHES UNDER INVESTIGATION
```

The current controller has been used successfully in real operation.

Observed battery runtime is approximately:

```text
~5 hours
```

with the M5GO3 Bottom / Bottom3.

---

# 1. Current Hardware

## Main Controller

- M5Stack CoreS3SE
- M5GO3 Bottom / Bottom3
- 3.7 V / 500 mAh internal Bottom battery
- M5Stack Unit DMX U183 connected to Port A

Current stable DMX pin mapping:

```cpp
TX = GPIO_NUM_2;
RX = GPIO_NUM_1;
EN = -1;
```

Unit DMX handles direction automatically.

---

# 2. Alternate Hardware Under Investigation

## M5Stack DMX Base

CoreS3 / CoreS3SE Base pin mapping:

```cpp
TX = GPIO_NUM_7;
RX = GPIO_NUM_10;
EN = GPIO_NUM_6;
```

The Base produced random DMX output glitches.

The issue was investigated extensively before switching to Unit DMX.

---

# 3. Current Software Stack

The application is built with PlatformIO / Arduino.

Important libraries:

- M5Unified
- M5GFX
- esp_dmx

Current application DMX library:

```text
esp_dmx v2.02
```

A compatibility patch is required for ESP32-S3 / current framework headers.

The patch currently exists locally in the downloaded dependency and must be converted into a reproducible repository solution before public release.

---

# 4. esp_dmx v2.02 ESP32-S3 Compatibility Changes

Current local changes in `dmx_ll.h`:

```cpp
hw->uart_idle_conf_reg_t.tx_idle_num
```

->

```cpp
hw->idle_conf.tx_idle_num
```

---

```cpp
hw->uart_txbrk_conf_reg_t.tx_brk_num
```

->

```cpp
hw->txbrk_conf.tx_brk_num
```

---

```cpp
hw->uart_status_reg_t.rxd
```

->

```cpp
hw->status.rxd
```

These should be automated or incorporated cleanly before GitHub release.

---

# 5. Current Application Features

## DMX

- full 512-channel universe
- touchscreen channel selection
- channel value editing
- continuous DMX output
- protected buffer handling
- software DMX activity indicator
- wired DMX output through Unit DMX U183

---

## Presets

8 persistent presets:

```text
P1 P2 P3 P4 P5 P6 P7 P8
```

Features:

- LOAD
- protected SAVE
- COPY
- CLEAR

SAVE:

- approximately 1 second hold
- progress feedback
- success tone

Preset options:

- long-press occupied slot
- opens `P# OPTIONS`

CLEAR:

- approximately 1 second hold
- persistent cleared-slot mask is stored in NVS

COPY:

- copies an occupied preset
- intended destination is an empty preset
- occupied destinations are blocked

---

## Blackout

BLACKOUT is protected by a double tap.

Timing:

```text
500 ms
```

Behavior:

First tap:

```text
TAP AGAIN
```

Second tap:

- snapshots/restores the DMX state
- outputs zero values during blackout

---

## Settings

Settings is vertically scrollable and currently contains:

### Brightness

```text
1 / 2 / 3 / 4 / 5
```

### Screen timeout

```text
1 / 2 / 3 / 4 / ON
```

`ON` means always-on display.

### Volume

```text
0 / 1 / 2 / 3 / 4
```

`0` means mute.

### Build timestamp

Displayed automatically from compiler date/time.

### Power controls

```text
RESTART
POWER OFF
BACK
```

RESTART and POWER OFF require a double tap.

Before either action, several all-zero DMX frames are sent.

---

# 6. Display / UI Rendering History

## Main DMX page

The main channel page was already visually smooth.

It uses clipped/buffered drawing rather than repeatedly blanking the complete display.

---

## Settings flashing problem

The first Settings scrolling implementation visibly flashed.

Root cause:

```text
physical LCD was cleared to black
then the complete Settings page was redrawn
```

This was especially visible with kinetic scrolling because redraw continued after finger release.

### Current fix

A dedicated full-screen Settings canvas is used:

```text
320 x 240
RGB565
~153.6 KB
```

The complete Settings frame is built off-screen and pushed to the display once.

This eliminated the visible black flash.

---

## Settings canvas audio interference

After implementing the full-screen Settings canvas, short speaker tones became distorted/chopped.

Likely cause:

```text
large full-screen display transfer occurring while audio was active
```

### Current fix

Settings frame transfer is deferred while:

```cpp
M5.Speaker.isPlaying()
```

A redraw-pending flag is stored and the frame is pushed immediately after the speaker becomes idle.

This preserved smooth rendering and clean sound.

---

# 7. Startup Screen

Startup duration:

```text
3 seconds
```

The startup screen includes:

- `DMX CONTROLLER`
- `CoreS3SE + Unit DMX`
- battery status
- countdown
- progress bar
- startup sound

Touching the screen during startup skips immediately to Sender mode.

The skip touch is fully consumed so it cannot accidentally operate a Sender control.

---

## Startup loader flashing fix

Original implementation repeatedly cleared approximately:

```text
320 x 60 pixels
```

on every loop iteration.

This produced visible flashing.

### Current solution

- countdown text redraws only when the displayed second changes
- progress outline is drawn once
- bar interior is reset once
- subsequent updates draw only the newly added green portion

No full-screen canvas is required for the loader.

---

# 8. Startup Sound

The startup sound is non-blocking.

It is a short rising four-note sequence.

Approximate frequencies:

```text
900 Hz
1350 Hz
1900 Hz
2700 Hz
```

The startup sound continues correctly even if the startup screen is skipped immediately.

Normal touch sounds should not interrupt a currently active sound.

---

# 9. Battery / Runtime

Battery display includes:

- percentage
- battery icon
- charging state
- external-power state
- estimated runtime

Colors:

```text
36-100%  green
21-35%   yellow
0-20%    red
```

While charging, green is used.

---

## Runtime Estimator

The runtime estimator learns from actual battery decline.

It does not use a fixed battery-runtime constant.

Current logic uses an observation period and rolling window.

The estimate is shown only when enough discharge data is available.

Example display:

```text
~8H07M
```

During initial calibration:

```text
CALCULATE
```

No ETA is shown while charging or externally powered.

Observed real-world full runtime has been approximately:

```text
5 hours
```

---

# 10. Low-Battery Warnings

Warnings occur around:

```text
20%
10%
```

Battery warning volume intentionally ignores normal muted/low UI volume.

Warning volume is approximately:

```text
80% / 204 of 255
```

After the warning tone finishes, the user's normal UI volume is restored.

The battery icon flashes at low battery while running from battery.

---

# 11. Persistent UI Settings

Stored with Preferences / NVS.

Namespace:

```text
dmxui
```

Keys currently include:

```text
bright
timeout
volume
```

Brightness and timeout survive restart.

Volume survives restart.

---

# 12. Display Sleep

The application turns off only the display backlight.

The controller continues sending DMX while the display is off.

First touch:

```text
wake display only
```

The complete wake gesture is consumed so it cannot accidentally:

- select a channel
- change a slider
- trigger a preset
- activate another control

---

# 13. Restart / Shutdown

Software restart:

```cpp
esp_restart();
```

Software shutdown:

```cpp
M5.Power.powerOff();
```

Both are accessible from Settings and protected by double tap.

Before either operation, zero-DMX frames are sent.

---

# 14. DMX Base Problem

The M5Stack DMX Base exhibited random output glitches/flickering.

The problem occurred with static and changing DMX data.

The Base was investigated much more deeply than the normal application path.

---

# 15. DMX Base Tests Performed

> Note: the exact source files for all historical diagnostic variants were not preserved as verified archives. The results below document what was tested, but the first public repository should not include reconstructed test code presented as the original versions.

The following were tested:

- proper DMX cable
- 120 ohm termination
- DMX Base bias resistor enabled
- multiple different fixtures
- individual fixture testing
- RGBWAUV fixture testing
- different 12 V power supplies
- different USB power arrangements
- full 512-channel universe
- approximately 30 FPS DMX
- conservative DMX timings
- waiting for complete send before next frame
- static DMX values
- minimal firmware with no UI activity
- esp_dmx v2.02
- esp_dmx v4.1.0
- GPIO6 direction-enable handling
- GPIO6 permanently forced HIGH
- raw RMT-generated DMX bypassing esp_dmx
- raw RMT-generated DMX bypassing UART

The glitches remained with the DMX Base.

---

# 16. Minimal esp_dmx Base Tests

A minimal static test was created that:

- writes one fixed DMX universe
- sends the same data continuously
- has no touch processing
- has no presets
- has no NVS activity
- performs no display redraws in the loop
- does not modify DMX values after setup

This test still reproduced the Base issue.

---

# 17. esp_dmx v4 Test

A separate test used:

```text
esp_dmx v4.1.0
```

with Base pins:

```cpp
TX = 7
RX = 10
RTS = 6
```

The issue remained.

---

# 18. Base GPIO6 Forced-HIGH Test

A stronger test removed GPIO6 from esp_dmx direction control.

Instead:

```cpp
pinMode(GPIO_NUM_6, OUTPUT);
digitalWrite(GPIO_NUM_6, HIGH);
```

was used so the Base transmitter stayed permanently enabled.

The issue remained.

This made normal RTS/direction timing a much less likely cause.

---

# 19. Raw RMT DMX Test

A raw ESP32-S3 RMT DMX transmitter was created.

It bypassed:

- esp_dmx v2
- esp_dmx v4
- ESP32 UART DMX generation

Approximate waveform:

```text
BREAK    ~200 us
MAB      ~24 us
baud     250 kbit/s
format   8N2
refresh  ~30 FPS
```

GPIO6 remained permanently HIGH.

The Base still produced glitches.

This is an important diagnostic test and should be preserved in the public repository.

---

# 20. Unit DMX Comparison

The M5Stack Unit DMX U183 was then tested using:

- the same CoreS3SE
- the same fixtures
- the same DMX cables
- the same general application
- the same environment

Unit DMX operated without the random glitches.

The full controller application now runs successfully using Unit DMX.

This strongly points toward the DMX Base hardware/output path rather than the application's DMX data generation.

---

# 21. Current Working Unit DMX Configuration

For CoreS3SE:

```cpp
int transmitPin = GPIO_NUM_2;
int receivePin  = GPIO_NUM_1;
int enablePin   = -1;
```

Then:

```cpp
dmx_set_pin(
    dmxPort,
    transmitPin,
    receivePin,
    enablePin
);
```

This configuration should be preserved as the reference build.

---

# 22. Current M5Stack Support Case

M5Stack Support has been contacted regarding the DMX Base glitches.

Support requested the source code.

The next goal is therefore to create a clean, reproducible GitHub repository that contains:

1. the stable Unit DMX application
2. a DMX Base build profile using the same application
3. documented Base troubleshooting history

The exact historical diagnostic test source was not fully archived and should not be recreated from memory for the initial public release.

If M5Stack requires a dedicated reproduction program, create a new minimal test from the cleaned repository, verify it on the actual hardware, and clearly label it as a newly created reproduction test.

This still allows M5Stack engineers to compare the Unit and Base hardware profiles using the same application code.

---

# 23. Recommended Repository Architecture

Preferred structure:

```text
M5Stack-CoreS3SE-DMX-Controller/
|
|-- platformio.ini
|-- README.md
|-- LICENSE
|-- AGENTS.md
|-- PROJECT_STATUS.md
|-- .gitignore
|
|-- include/
|   `-- dmx_hardware.h
|
`-- src/
    |-- src.cpp
    |-- view_sender.h
    |-- view_receiver.h
    |-- view_base.h
    `-- preset_store.h
```

Exact structure may be adjusted after inspecting the real current project.

Do not reorganize files until the known-good build has been committed and verified.

---

# 24. Proposed PlatformIO Targets

Desired result:

```text
cores3se_unit
cores3se_base
```

Conceptually:

```ini
[env:cores3se_unit]
build_flags =
    -DDMX_HARDWARE_UNIT

[env:cores3se_base]
build_flags =
    -DDMX_HARDWARE_BASE
```

Both should compile the same application source.

---

# 25. GitHub Release Plan

Proposed repository:

```text
M5Stack-CoreS3SE-DMX-Controller
```

Initial release:

```text
v0.1.0
```

Suggested release positioning:

```text
Initial public release / hardware support investigation
```

The project is functional but still evolving, so v0.1.0 is preferred over v1.0.

---

# 26. Git Migration Plan

Before cleanup:

```bash
git init
git add .
git commit -m "Working CoreS3SE Unit DMX baseline"
```

Then create:

```bash
git checkout -b cleanup/github-release
```

Perform repository cleanup only on that branch.

Before publishing:

- build Unit target
- test Unit target on hardware
- build Base target
- verify Base target reproduces or can be tested
- verify fresh clone can build without manually editing `.pio`

---

# 27. Immediate Next Steps

A new coding session should proceed in this order:

1. inspect the existing repository
2. make no changes initially
3. verify the current Unit DMX build
4. identify all required source/header files
5. identify current PlatformIO dependencies
6. preserve the known-good baseline in Git
7. make esp_dmx patch reproducible
8. introduce Unit/Base hardware profiles
9. document the historical Base troubleshooting results
10. do not publish reconstructed diagnostic test source unless freshly verified
11. create README / LICENSE / .gitignore
12. build both hardware targets
13. publish GitHub repository
14. send repository link to M5Stack Support

---

# 28. Longer-Term Development Ideas

These are future ideas only and should not be mixed into the GitHub cleanup unless explicitly requested.

Potential future work:

- wireless DMX transport
- ESP-NOW DMX transmitter/receiver
- AtomS3-Lite + Unit DMX receiver
- wireless link status / RSSI
- packet-loss monitoring
- wired + wireless simultaneous output
- professional CRMX / W-DMX integration

The current priority is repository cleanup and M5Stack support reproducibility.
