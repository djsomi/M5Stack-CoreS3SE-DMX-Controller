# Stage 6B.5B — Device Maintenance: Corrected Countdown and Original Power Off

Branch: `codex/stage6b-web-ui`. Stage 6B.5B is physically accepted and validated
for one focused local closure commit. This closure changes documentation only;
the accepted production source, tests and generated assets remain unchanged.
No upload, restart, power-off, push, merge, tag, release or device NVS operation
was performed during closure.

## Accepted reference and correction

Stage 6B.5A baseline: `487fce1073def9c3328905d3b913d4482cd3b75d`, parent
`ce1b9f63431874ae05588c1275644878ee29a973`, message:
`feat(web): finalize Stage 6B.5A device settings and responsive UI`.
That local commit contains the 42 accepted Settings/responsive/UI/test/asset files.
Stage 6B.5B is recorded as a separate commit on top of this accepted baseline.

The user confirms the original physical Power Off worked with USB connected.
That is the accepted hardware reference. The earlier uncommitted restrictions
came from an unsupported precautionary inference, not a demonstrated failure.
They have been removed from admission, physical UI, web UI, protocol state, tests
and this report. No new PMIC feasibility investigation was performed.

### Original Power Off is restored exactly

- `src/src.cpp::controllerPowerOff()` is byte-for-byte identical to the baseline:
  `M5.Power.powerOff();`.
- `src/view_sender.h::sendDmxZeroBeforePowerAction()` is also identical to the
  baseline, including three iterations of zero staging, the existing 50-tick
  wait and 10 ms delay. No new transport starts, retirement requirements,
  source checks or extra preparation precede Power Off.
- One small `performOriginalPowerOff()` wrapper calls that original helper and
  `controllerPowerOff()` in the original order. Physical Settings and the web
  execution backend call the same wrapper. The physical Power Off double-tap
  handler is otherwise identical to the baseline; its normal label is restored.
- The new Restart zero-frame/quiescence preparation is not run for Power Off.
  Restart retains the prior Stage 6B.5B implementation and its fault handling.
- USB connection, battery presence and Unit/Base profile do not gate Power Off.
  The new backend has no power-source query or PMIC reconfiguration. Accepted
  startup power initialization, battery monitoring, charging configuration,
  pins, driver and normal transmission timing are unchanged.

The legacy Power Off helper was already documented as staging/waiting rather
than guaranteeing three completed zero frames. Restoring the accepted implementation
does not reinterpret that contract or invent a new wire guarantee. Its existing
bounded waits occur only after execution begins. The preceding five-second
countdown remains nonblocking. No screen-only or infinite-loop substitute exists.
No claim about continuing battery charging is inferred from source or host mocks;
that intended behavior remains part of the physical acceptance check.

## Web maintenance UX and owner sequencing

1. Choose Restart or Power Off in the browser, then explicitly confirm.
   The first activation alone never submits an operation. Local browser
   confirmation expires after ten seconds or when leaving Settings.
2. The existing native WebSocket handler validates and copies a bounded request.
   Application-task `Bridge::ownerPoll()` admits it once, rejecting duplicates,
   conflicts, stale IDs and unsupported modes. Settings transactions already in
   progress must finish before admission; a restoration still owning the physical
   canvas is also busy, preventing a new request from interrupting that handoff.
3. Admission starts one monotonic, wrap-safe **5,000 ms** device cancellation
   window. The full-screen popup has the operation, large 5/4/3/2/1 countdown and
   one 240 x 50 px CANCEL button at x40/y174. There is no approval gesture.
4. Physical CANCEL acts on a fresh press, not a release after the deadline. A
   pre-existing touch must release first. Cancellation at elapsed 4,999 ms is
   accepted; at 5,000 ms or later it is rejected, even before the next poll.
   The initiating live browser can also cancel its own generation/request.
5. DMX transmission, Controller/Dispatcher, network, Settings servicing and
   battery monitoring continue during the window. Ordinary web mutations stay
   available until execution commits. The popup exclusively owns physical touch
   and painting. Cancellation does not alter the universe, transport or presets.
6. The deadline never resets because of paint, state messages, navigation,
   disconnect, reconnect or a fresh read. An accepted countdown continues after
   the initiating browser disconnects. A new generation reads its remaining
   state without replaying a request; physical CANCEL remains available. A new
   session cannot reuse the old browser's cancellation ownership.
7. At expiry, mutations are blocked through the existing owner-side admission
   gates. Cancellation is no longer admitted. Power Off advances directly to the
   original shared execution wrapper on the next owner turn; no new shutdown
   preparation is inserted. Restart uses its existing bounded preparation of
   three qualified zero frames and safe transport retirement, then `esp_restart()`.
8. Acknowledgments/progress are attempted before execution. Connection loss is
   not proof of completed restart/off. An unexpectedly returning hardware mock
   leaves a visible failed state and never invokes the operation repeatedly.
   Restart's genuine transport/preparation faults also remain visible.

Physical Settings retains its existing double-tap interaction. Physical Power Off
executes the same original wrapper directly; it does not acquire a new countdown
or supply restriction. Physical Restart uses the existing shared Restart path.
No work is replayed automatically after device boot or browser reconnect.

### Security scope

The countdown is a cancellation opportunity, **not authentication**. Browser
confirmation expresses intent, and generation/ID checks enforce ownership and
ordering; none establishes the identity of an associated client. This remains the
existing SoftAP-only product. No STA/LAN access, credential change, token store,
permanent secret or telemetry credential was added. Before future LAN/STA/Home
Assistant exposure, destructive maintenance must acquire a separately reviewed
authorization boundary. Predictable factory passwords and WPA2 association alone
must not be represented as that boundary. Existing bounds/rate limits remain;
an associated client can request maintenance, so physical cancellation is not
protection against a determined associated attacker.

## Physical rendering correction

Concrete source defects in the previous implementation included using Main's
incremental grid renderer as a complete popup restore, returning through a stale
pre-cancellation snapshot, and having no explicit LCD clip/transfer handoff guard.
These could leave old popup pixels or allow intermediate normal-page drawing.
The user subsequently confirmed clean physical screen transitions after correction;
the earlier source findings do not independently prove each hardware symptom's cause.

- Finish any outstanding display transfer and clear the display clip before
  claiming the maintenance page. Clear the shared canvas clip and paint a complete
  black-backed initial frame. No additional framebuffer/sprite is allocated.
- Cache phase independently from countdown revision. A changed digit clears and
  paints only an 80 x 60 px rectangle at x120/y78. The LCD transfer is clipped to
  that rectangle. Unchanged digits do not repaint; phase transitions get a full
  frame. No per-loop full-screen pushes or continuous diagnostic print is added.
- Cancellation immediately leaves the countdown branch; the old snapshot is not
  drawn afterward. Ownership stays exclusive through the complete restore and
  CANCEL release. `serviceControllerChanges()` is also gated by this UI ownership,
  even after the maintenance authority has returned to idle.
- Settings, Precision, Keypad and Presets use their complete page renderers.
  Main explicitly clears the entire old popup, forces the grid redraw and rebuilds
  the full slider/action column or Preset/Battery/Settings/Blackout/heartbeat area.
  Channel, scroll position, STEP, keypad draft, blackout state and preset metadata
  remain intact. Interrupted drag/repeat/flick intent is retired.
- A short active speaker tone can defer the complete restoration; the maintenance
  page retains ownership until it can finish. DMX/network servicing continues.
  New maintenance requests cannot interrupt that pending handoff. The entire
  CANCEL press/release is consumed before the underlying page resumes input.

The extracted production renderer runs against a pixel/clip/display fake in host
regressions. This validates ownership, clipping, clearing and transfer decisions;
it does not reproduce actual SPI/DMA, font rasterization or iPhone/panel timing.

## Web card layout

The real Device Maintenance card retains desktop 50/50 columns, the existing
16 px gap, mobile full-width stacking and separate Browser Preferences. It now
uses a compact natural height, consistent 12 px internal gaps and readable line
wrapping. Unused capability warnings and the large empty confirmation region are
removed. Confirm/Back, operation controls and countdown/cancel controls share one
stable button row; two text lines are reserved for concise normal guidance.
Real errors use a separate readable live region only when needed.

Settings slider appearance, mappings, preview/release persistence, gestures,
browser preferences, other views and the main/lower blackout indicators are
unchanged. The fixed iOS shell, navigation and touch policies are preserved.

## Protocol correction (unreleased Stage 6B.5B extension)

Version 1, request IDs, session generations, opt-in subscriptions and all existing
message/rate/queue budgets remain. Request shapes are unchanged:

```json
{"v":1,"id":1,"type":"maintenance.get"}
{"v":1,"id":2,"type":"maintenance.request","action":"power_off"}
{"v":1,"id":3,"type":"maintenance.cancel","request":2}
```

The initial result means a countdown was accepted, not that hardware completed:

```json
{"v":1,"type":"maintenance.result","id":2,"revision":"2","seq":"10","action":"power_off","phase":"countdown","request":2,"reason":"none","remaining_ms":5000,"status":"accepted"}
```

Snapshots/results/updates carry the same complete small state; updates have id 0,
results add status. The removed capability field is replaced by `remaining_ms`,
rounded up to seconds, updated at most once per second. The application deadline
remains authoritative; clients do not extend or reconstruct it. Revision is
independent of Controller/Settings; publication sequence is shared.

Phases: idle/countdown/zeroing/quiescing/executing/failed. Reasons:
none/busy/cancelled/transport/deadline/returned. Results:
invalid/accepted/cancelled/unauthorized/busy/unsupported. Late cancellation is
busy; foreign-generation cancellation is unauthorized. Neither implies a new
power-source policy. Frame parsing rejects extra keys/types/actions and incomplete
state. The existing five-second response deadline, stale-socket detection and
reconnect state machine remain; a lost response never causes mutation replay.

## Validation and physical acceptance

Automated checks include:

- Countdown 0/1/999/1,000/3,999/4,000/4,999/5,000 ms boundaries, uint32 wrap,
  cancellation before/after expiry, twenty request/cancel cycles, duplicates,
  conflicts, continued mutations during countdown, reconnect-independent expiry,
  post-commit rejection, execution once, Restart fault paths and NVS preservation.
- Exact baseline function/helper and physical call-order comparisons. Extracted
  original Power Off executes for USB/battery/external combinations in both Unit
  and Base host builds. Tests assert no added source reads or charging changes;
  this is a code regression test, not a new hardware feasibility test.
- Extracted physical methods with six prior pages: full first frame despite a
  stale clip/unfinished transfer, digit-only updates, immediate cancellation,
  full clean restore, speaker deferral, consumed release, expired window,
  repeated requests, preserved values/STEP and one shared framebuffer.
- Full frontend/navigation/live/Settings/protocol cases, Controller/Dispatcher/
  transport/ISR/network/HTTP host cases and existing infrastructure/patch checks.
- Native browser cases at seven widths exercise both controls, explicit browser
  confirmation, cancellation, ordinary fader use during countdown, no deadline
  reset on tab changes/reconnect, fresh reads/no replay, compact/equal/stacked
  card geometry, stable controls/navigation, and preferences. Existing Settings,
  header/preset/gesture, twelve-viewport and lower blackout suites remain covered.
- Deterministic asset generation twice, gzip/hash verification, four PlatformIO
  builds, matching source/component receipts and Git whitespace checks.

The user has now physically accepted Stage 6B.5B Restart, Power Off, the
five-second physical CANCEL countdown, USB-connected Power Off and responsive
Settings layout. Final physical acceptance also includes clean screen transitions,
the green Confirm accent, desktop 50/50 columns, mobile Maintenance, and existing
Device Settings, DMX, presets and blackout. The checklist below remains a reference;
it does not imply new electrical measurements or battery-charging verification.

1. iPhone Safari/Home Screen and desktop: concise confirmation, correct five-second
   popup and CANCEL hitbox, repeated/rapid request/cancel cycles, natural-height
   card, desktop equal columns and mobile stacking.
2. Start/cancel from Main channel and value modes, Settings, Precision, Keypad,
   Presets and preset options. Check no stale pixels, flicker/ghost text, accidental
   underlying touch, lost selection/draft/STEP or second framebuffer allocation.
   Repeat during a tone and with physical display sleep/wake.
3. Countdown traffic and browser disconnect/reconnect: DMX/network remain active,
   deadline does not extend, physical CANCEL remains effective before expiry,
   late cancel cannot abort committed execution, and no command replays on boot.
4. Let Restart and Power Off expire once, and compare physical double-tap Power
   Off with the accepted original behavior. Include USB attached/disconnected,
   battery attached/absent as applicable, and Unit/Base configurations. Report
   observed results; do not impose a new policy or substitute an operation.
5. Verify the intended USB-connected off/charging use case on hardware; charging
   configuration is untouched, but charging continuation is not newly asserted
   from software tests. Confirm settings/presets survive restart/off.
6. Any electrical DMX shutdown/gap assertion still requires external RS-485
   capture. Power Off intentionally retains the original legacy contract;
   Restart's qualified frames are simulated evidence until captured electrically.

Existing synchronous HTTP/WebSocket send stalls and experimental Base DMX glitches
remain separate documented limitations. Detailed validation artifacts and the
preserved pre-correction workspace are under ignored
`dist/maintenance-correction-20261029/`. Historical files, unrelated documents,
logs, migration artifacts and stash are preserved.

## Earlier correction validation and resource deltas

All validation listed above passed, including the full frontend/navigation/live
and Settings suites; four maintenance/render/original-power host executables;
three Settings executables; all 22 existing host cases; ten integration/scope/
asset/patch checks; existing browser suites and the corrected seven-width
Maintenance browser suite. The extracted production power function, original
helper and physical handler call order match the accepted baseline exactly.
Phone and desktop screenshots were inspected. No physical upload or NVS write.

Gzip deltas below compare the preceding uncommitted Stage 6B.5B candidate:

| Asset | Before correction | Current bytes | Delta |
|---|---:|---:|---:|
| HTML | 4,486 | 4,477 | -9 |
| CSS | 4,839 | 4,862 | +23 |
| Core JS | 11,214 | 11,198 | -16 |
| UI JS | 11,346 | 11,190 | -156 |

All are below 16 KiB. Generation twice was byte-identical, manifests/gzip contents
verified, and all four compiled source/component receipts match this source:
`9d503decf62975a53e8c7105b2a65c4040bde42f9ee42ee6c6effaa07d1bbac4`.

| Environment | Build | Static RAM | RAM delta | Flash | Flash delta |
|---|---|---:|---:|---:|---:|
| cores3se_unit | PASS | 89,900 | +248 | 1,083,541 | -556 |
| cores3se_base | PASS | 89,900 | +248 | 1,083,521 | -468 |
| cores3se_unit_diagnostics | PASS | 92,612 | +240 | 1,088,137 | -516 |
| cores3se_base_diagnostics | PASS | 92,612 | +240 | 1,088,101 | -448 |

No new full-screen canvas allocation. Runtime heap/PSRAM and actual display/DMX
waveform behavior remain hardware measurements, not inferred from static sizes.

Native browser measurements (CSS px):

| Viewport | Settings width | Maintenance width | Maintenance height | Arrangement |
|---|---:|---:|---:|---|
| 320 | 288 | 288 | 204.58 | Full-width stacked |
| 375 | 343 | 343 | 204.58 | Full-width stacked |
| 390 | 358 | 358 | 204.58 | Full-width stacked |
| 430 | 398 | 398 | 204.58 | Full-width stacked |
| 600 | 568 | 568 | 204.58 | Full-width stacked |
| 768 | 348 | 348 | 204.58 | 50/50 desktop columns |
| 1280 | 476 | 476 | 204.58 | 50/50 desktop columns |

The Maintenance card is approximately 205 px high at all tested sizes, instead
of the previous roughly 397–417 px phone card. Normal confirmation/countdown/
cancellation/progress retain card/control geometry; real errors remain readable.

## Earlier correction inventory

These are the files changed by this correction relative to the previous
uncommitted Stage 6B.5B working tree, rather than all accumulated Phase B work:

- `docs/STAGE6B5B_DEVICE_MAINTENANCE.md`
- `include/device_maintenance.h`
- `src/src.cpp`
- `src/view_sender.h`
- `src/websocket_bridge.cpp`
- `src/websocket_protocol.cpp`
- `tests/maintenance/check_integration.py`
- `tests/maintenance/run_tests.py`
- `tests/maintenance/test_maintenance.cpp`
- `tests/maintenance/test_power.cpp`
- `tests/maintenance/test_render.cpp`
- `tests/network/maintenance_guard_support.py`
- `tests/web/test_header_browser.cjs`
- `tests/web/test_maintenance.mjs`
- `tests/web/test_maintenance_browser.cjs`
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/src/core.js`
- `web/src/index.html`
- `web/src/js/protocol.js`
- `web/src/js/settings.js`
- `web/src/js/transport.js`
- `web/src/style.css`
- `web/src/ui.js`

At that earlier correction checkpoint, historical work and the existing stash were
unchanged, the staging area was empty, and no commit, push, merge, tag, upload,
flash erase or NVS operation occurred. Final acceptance and closure supersede that
checkpoint's awaiting-acceptance state.

## Final cosmetic follow-up: green browser Confirm

Stage 6B.5B functional hardware acceptance is recorded above. This follow-up
changes production CSS only, scoped to `#maintenance-confirm:not(:disabled)`:

- Dark green fill `#245840`, slightly lighter desktop hover `#2b664b`, darker
  pressed `#1e4936`, matching green borders and pale green keyboard focus ring.
- Existing `#eef2f8` foreground, 2 px focus outline/4 px offset, font, padding,
  border width and dimensions are retained. Text contrast exceeds 4.5:1 in every
  enabled color state. Hover is restricted to fine pointers with hover support.
- Disabled/pending Confirm does not match any green selector, so existing native
  disabled colors/cursor apply. Back, initial Restart and initial Power Off keep
  their existing neutral/danger styles. No state, text, event or protocol change.
- Seven-width native browser tests pass for Restart and Power Off: normal/hover/
  pressed contrast, keyboard focus, neutral Back/actions, unchanged card/button
  geometry, pending response held before acknowledgment, duplicate guard and
  return through the existing countdown/cancel interaction. Screenshots were
  inspected. The user subsequently physically accepted this cosmetic follow-up;
  browser automation itself is not a physical iPhone test.
- Full frontend/navigation/live/Settings/protocol suites, existing seven-width
  Maintenance interaction tests, all ten relevant pipeline/network/Dispatcher/
  infrastructure/patch checks and all four firmware builds pass. Generation twice
  is deterministic; gzip/hash/receipt/component and whitespace checks pass.

Final gzip bytes: HTML **4,477**, CSS **4,930** (+68), Core **11,198**, UI **11,190**.
Every asset remains below 16 KiB. Core/UI source and gzip bytes are unchanged.
Static RAM remains 89,900 bytes normal / 92,612 bytes diagnostics. Flash is
1,083,621 Unit (+80), 1,083,585 Base (+64), 1,088,201 Unit diagnostics (+64), and
1,088,165 Base diagnostics (+64), relative to the accepted pre-polish candidate.

Exact follow-up inventory:

- `web/src/style.css`
- `web/generated/style.css.gz`
- `web/generated/index.html.gz` (new CSS content-hash URL, unchanged source HTML)
- `web/generated/manifest.json`
- `tests/web/test_maintenance_confirm_style_browser.cjs`
- `docs/STAGE6B5B_DEVICE_MAINTENANCE.md`

At the cosmetic follow-up checkpoint, application source, JavaScript, markup,
maintenance behavior, original Power Off path, Restart, DMX, NVS, presets and
unrelated/historical files were unchanged from that task's entry. HEAD/stash were
unchanged and the index remained empty. Its validation artifacts are under ignored
`dist/maintenance-confirm-green-20261029/`. The subsequent user acceptance covers
the green Confirm button with unchanged behavior.

## Final Stage 6B.5B acceptance and local closure — 2026-10-29

The user physically tested and accepted web Restart, the original established
Power Off path (including USB-connected operation without supply restrictions),
the five-second physical CANCEL window, clean display transitions, desktop equal
columns, mobile layout and green Confirm. Existing Settings, DMX, presets and
blackout remain operational. No further functional or visual changes were made
during closure. Source inspection and regression checks confirm the original
Power Off function/helper and shared physical/web call order remain intact.

All final checks passed:

- Four maintenance/original-power/render host executables, three Settings host
  executables and all 22 existing Controller/Dispatcher/transport/ISR/WebSocket/
  SoftAP/HTTP/canvas/startup/station/asset host cases.
- Complete frontend/navigation/live/Settings/maintenance/status/loader suites,
  including 83 live integration groups and 12 asynchronous Status groups.
- Ten existing asset, network, Dispatcher, infrastructure and reproducible patch
  integration checks, including abnormal transport/ISR and patch-state cases.
- Six native-browser suites: header/presets/gestures, Settings, twelve responsive
  viewports, lower blackout status, Maintenance and green Confirm. Seven-width
  maintenance cases cover request/cancel, countdown, reconnect, continued control,
  keyboard focus, contrast, disabled/pending behavior and stable geometry.
- Deterministic composition and gzip regeneration twice, manifest/hash/round-trip
  checks, every compressed asset below 16,384 bytes, four PlatformIO builds,
  matching source/component receipts and Git whitespace checks.

Final artifacts and isolated validation logs are in ignored
`dist/stage6b5b-finalization/`; previous logs and screenshots were preserved.
No software test claims electrical DMX integrity or actual panel/DMA timing.

| Asset | Final gzip bytes |
|---|---:|
| HTML | 4,477 |
| CSS | 4,930 |
| Core JavaScript | 11,198 |
| UI JavaScript | 11,190 |

| Environment | Result | Static RAM bytes | Flash bytes |
|---|---|---:|---:|
| cores3se_unit | PASS | 89,900 | 1,083,621 |
| cores3se_base | PASS | 89,900 | 1,083,585 |
| cores3se_unit_diagnostics | PASS | 92,612 | 1,088,201 |
| cores3se_base_diagnostics | PASS | 92,612 | 1,088,165 |

All resource values and generated bytes match the accepted green-Confirm candidate;
closure adds no application RAM, flash or asset growth. Compiled source fingerprint:
`a107bdb0c3e51d0cc32d0fdee6f021f4497f28625bdddd648dc5b02cfbdcd10f`.
Asset bundle fingerprint:
`9c626d07e7b04fadc2ad77139f057882fdc0d57f1d268c9e9a4ba53fc6c2bfc9`.

### Deferred observation: apparent countdown around update-triggered reboot

The user observed an apparent five-second countdown around a firmware-update-
triggered reboot, in addition to web-requested Restart. Its cause and exact reset
path are **unknown**; this is a future investigation, not a Stage 6B.5B blocker.
Future work should distinguish explicit user maintenance requests from automatic
firmware update/reset paths and determine whether an update-required reboot could
be delayed or cancelled unintentionally. No reboot behavior was investigated or
changed in this closure task. Do not infer causality from the observation alone.

Existing synchronous HTTP/WebSocket send stalls, experimental Base glitches,
electrical shutdown-frame/gap verification and charging-continuation measurements
remain separate limitations. No next-stage implementation is included.

### Complete reviewed Stage 6B.5B commit inventory

Only these 42 production, test, documentation and generated-asset files belong to
the local closure commit. The historical development folder, unrelated
`docs/STAGE3_NO_RECEIVER_REGRESSION.md`, existing logs/backups/migration bundles and
stash are excluded and preserved.

```text
docs/STAGE6B5B_DEVICE_MAINTENANCE.md
include/device_maintenance.h
include/device_settings.h
include/maintenance_transport.h
include/websocket_bridge.h
include/websocket_service.h
src/src.cpp
src/view_sender.h
src/websocket_bridge.cpp
src/websocket_protocol.cpp
src/websocket_service.cpp
tests/dispatcher/check_integration.py
tests/maintenance/check_integration.py
tests/maintenance/run_tests.py
tests/maintenance/test_maintenance.cpp
tests/maintenance/test_power.cpp
tests/maintenance/test_render.cpp
tests/network/device_settings_guard_support.py
tests/network/maintenance_guard_support.py
tests/transport/test_transport.cpp
tests/web/check_scope.py
tests/web/run_tests.mjs
tests/web/test_device_settings.mjs
tests/web/test_device_settings_browser.cjs
tests/web/test_header_browser.cjs
tests/web/test_maintenance.mjs
tests/web/test_maintenance_browser.cjs
tests/web/test_maintenance_confirm_style_browser.cjs
tests/web/test_responsive_layout_browser.cjs
web/generated/core.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/generated/ui.js.gz
web/src/core.js
web/src/index.html
web/src/js/commands.js
web/src/js/protocol.js
web/src/js/settings.js
web/src/js/transport.js
web/src/style.css
web/src/ui.js
```
