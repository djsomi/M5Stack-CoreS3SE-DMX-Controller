# Main-to-fullscreen navigation during kinetic scrolling

## Confirmed mechanism and minimal correction

Main's touch handler could open Settings, then continue into Main button handling
and the kinetic-scroll redraw in the same Sender loop iteration. Settings entry
reset its own settings_scroll_add but retained Main's scroll_y_add. A late channel
grid redraw could overwrite the left area of a completed Settings frame, leaving
its right-hand actions intact. Settings had already cleared its pending-redraw
flag, so the overwrite could persist. Presets already canceled Main momentum.
Precision entry had the same potential ordering hazard as Settings.

The only production source additions are:

1. setActiveFullscreenPage cancels scroll_y_add on entry to any non-Main owner.
   It does not change scroll_y, Settings' momentum, or the normal Main deceleration.
2. After Main touch processing, a non-Main fullscreen owner returns true before
   Main button handling, kinetic animation or Main status painting can execute.

The application src.cpp is unchanged: transport polling, network owner servicing,
WebSocket owner servicing, Dispatcher processing and speaker-volume restoration
precede the call to Sender loop. Sender services DMX output and battery monitoring
before touch handling and the new return. Destination-page servicing continues
through the existing branches on subsequent iterations. No common domain/transport
work, required cleanup or settings persistence is moved behind the new return.

Captured raw-coordinate handling, release/capture ownership, shared-canvas
allocation/publication, Settings audio deferral and Preset rendering are unchanged.
The fix adds no delay, polling, smoothing or animation wait. Phase 1 investigation
isolation and the suspended THDIFF configuration remain intact.

## Targeted regression

tests/navigation/run_tests.py extracts the complete real Sender loop and real
Settings/Preset/Precision entry/exit, page-ownership, Precision reset and canvas
push methods. Peripheral input/output and rendering are host fakes. A pixel fake
models complete page regions and exposes a late Main overwrite; it does not test
font rasterization, actual DMA, panel timing or electrical correctness.

The pre-fix source must fail because Main draws after fullscreen entry. A version
missing only the cancellation must fail its zero-momentum assertion; a version
missing only the early return must fail the stale-Main-action assertion. The
unchanged captured-coordinate correction is present in every source variant.

Corrected cases cover:

- Settings with -150, -1, 0, +1 and +150 momentum, with and without active audio.
- No Main draws after entry; complete Settings pixels persist through idle frames.
- Pending Settings rendering finishes when the speaker becomes idle.
- Presets and Precision entry with positive/negative momentum and complete frames.
- Simultaneous Main button input cannot execute after fullscreen entry.
- Return to Main preserves position without resumed motion; repeated navigation.
- One shared canvas reused across Settings entry/re-entry, with guarded ownership.
- All extracted fullscreen owners cancel Main momentum, including keypad ownership.
- Fifteen normal Main trajectories, each 100 iterations, retain the existing linear
  decay and upper/lower clamping behavior when no navigation occurs.
- Sender DMX and battery servicing still execute in transition iterations.

All four host configurations exercise the same extracted control flow. One existing
diagnostics-only unused log-label parameter is annotated unused in the host copy;
no executable statement is changed and strict compiler warnings remain enabled.

## Exact task file inventory

- src/view_sender.h — only the two navigation guards.
- tests/network/fullscreen_navigation_guard_support.py — remove exactly those two
  additions for existing historical scope checks; no broad exception.
- tests/network/fader_guard_support.py — invoke that precise scope normalization.
- tests/fader/run_tests.py — delimit the unchanged fader input slice independently
  of the newly inserted navigation guard; preserve existing fader assertions.
- tests/navigation/run_tests.py — actual-source extraction, pre-fix/partial-fix
  failure checks, common-work ordering and four corrected host configurations.
- tests/navigation/test_navigation.cpp — fake peripherals/pixels and regressions.
- docs/MAIN_FULLSCREEN_NAVIGATION_FIX.md — this report.

## Validation and hardware acceptance

All four PlatformIO builds passed. Their source fingerprint is
`8151c66a0ebb9f1d3e7ed4e90624ae9a327ab7b9bba74e79d71c37450ae0ebdb`;
build ID is `0.3.0-rc.1+9fdf07ee0234.modified.8151c66a0ebb`.

| Environment | Static RAM | Flash | RAM delta | Flash delta |
|---|---:|---:|---:|---:|
| cores3se_unit | 95,908 | 1,100,301 | 0 | -20 |
| cores3se_base | 95,908 | 1,100,277 | 0 | +4 |
| cores3se_unit_diagnostics | 98,628 | 1,104,881 | 0 | -28 |
| cores3se_base_diagnostics | 98,628 | 1,104,865 | 0 | -16 |

Deltas compare Phase 1. ELF and firmware-string inspection confirms investigation
implementations and THDIFF commands are absent in all four; captured fader code
remains present, and Stage 0 remains present only in diagnostics. Two independent
asset generations equal the existing approved gzip files and manifest; gzip sizes
remain HTML/CSS/Core/UI 4,950 / 5,199 / 12,467 / 12,624 bytes.

The targeted test demonstrated the expected pre-fix overwrite failure and both
partial-fix failures, then passed in all four corrected host configurations.
Initial fixture compilation found unused test/log declarations; those were
restricted/annotated only in host scaffolding, retaining -Wall/-Wextra/-Werror
and unchanged firmware control flow. No firmware build failure occurred.

All fourteen validation groups passed: targeted navigation, default-off isolation,
simulated THDIFF, read-only snapshot, captured-fader/recorder, acquisition
differential, integration/regression, 23 core host programs, frontend/live/
navigation, ten browser suites, Settings, Maintenance, Wi-Fi and release checks.
Whitespace checks pass. Exact historical scope reconstruction removes only the
two reviewed additions before comparing protected application/domain inputs.
Local evidence is preserved in `dist/main-fullscreen-navigation-20261031/`.
Original RC artifacts/checksums, Phase 1 changes, stash and unrelated evidence are
preserved. No firmware upload, register operation, NVS operation, staging, commit,
push, merge, tag or release is part of this task.

Physical acceptance still needs fast long swipes in both directions, Settings taps
during motion, repeated exit/re-entry, audio-active entry, Presets and Precision,
and confirmation that ordinary Main scrolling and DMX/web synchronization remain
normal. Host tests do not replace that physical acceptance.

## v0.3.0 local closure

This accepted production prerequisite is included in the local v0.3.0 closure.
Historical version/build measurements above remain unchanged as evidence.
Current version is 0.3.0; see [closure record](V0_3_0_LOCAL_CLOSURE.md).
The captured raw-coordinate correction and diagnostic isolation remain intact.
No new touch configuration, NVS operation, upload or electrical validation is
introduced by closure. Suspended THDIFF write experiments stay disabled.
