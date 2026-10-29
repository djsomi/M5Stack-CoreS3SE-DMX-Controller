# Stage 6B.5A — Shared Device Settings

Implementation/refinement and automated validation: 2026-10-29. The original
physical-to-web synchronization was hardware-tested successfully. The user has now
physically accepted the complete live slider interaction and final responsive polish.
Branch: `codex/stage6b-web-ui`. Stage 6B.5A is approved for local baseline closure.

## Accepted work committed first

Phase A created only the previously hardware-accepted preset-action presentation
commit `ce1b9f63431874ae05588c1275644878ee29a973`, message
`fix(web): stabilize preset action buttons during pending commands`.
Its parent is the accepted two-bundle commit
`4e6fbbeada642e5dc8345421a1d11eb9acc6baa5`.
Phase A reviewed its exact nine-file inventory, deterministic assets,
regressions and all four matching successful firmware receipts before committing.
No Stage 6B.5A implementation was included in that commit.

## Existing implementation verified

`src/src.cpp` previously owned three mutable level globals and separately derived
hardware values. `view_sender.h` read the globals and called their setters.
`loadUiSettings()` read unsigned bytes from `dmxui`; `saveUiSettings()` wrote all
three keys synchronously without checking individual results. Physical dragging
could invoke that writer on each distinct level. Unchanged levels returned early.

| Setting | Existing values/default | NVS key | Existing hardware mapping |
|---|---|---|---|
| Brightness | 1–5; default 3 | `bright` | 32, 64, 128, 192, 255 |
| Screen timeout | 0–4; default 3 | `timeout` | 0 Always On; 1–4 minutes |
| UI volume | 0–4; default 2 | `volume` | 0, 20, 40, 100, 200 |

Invalid stored brightness/timeout still fall back to 3. Legacy stored volume
above 4 still clamps to 4. Loading does not rewrite, migrate or erase anything.
Battery-warning volume remains 204 and restores the current normal volume after
the tone. Wake still consumes the complete first touch. These behaviors and the
existing physical control geometry/tones are retained.

## Authority and owner boundary

`include/device_settings.h` defines the small, allocation-free `DeviceSettings`
authority and injected `DeviceSettingsBackend`. Public operations:

- `begin()`: startup read/normalization and hardware application, called once.
- `value(DeviceSetting)` / `snapshot()`: current levels, independent 64-bit
  revision, three-bit persistence confirmation mask.
- `valid(key, uint32_t)`: exact range validation before narrowing.
- `set(key, value, persist=true)`: apply an accepted change; optionally commit.
- `commitPending()`: persist final physical gesture values once after release.
- `edit(...)`: owner/session/gesture/revision-qualified web preview, commit or cancel.
- `previewOwner()` / `servicePreview(now, owner_alive)`: retire closed or expired
  preview leases on the application task, before any Dispatcher early return.

The static application instance owns the three level values. The remaining
brightness PWM, timeout milliseconds and speaker-volume globals are derived
hardware caches used by the unchanged sleep and sound services, not independently
editable configuration. Nothing is added to the DMX universe or Controller revision.

The physical setters use this authority with `persist=false`. The Settings page
commits only on release/idle, not while dragging or rendering. It redraws from
the authority's revision for remotely changed settings, using the existing shared
canvas and tone-aware rendering rules. Read/draw operations never save.

HTTP/WebSocket callbacks validate and enqueue bounded copied requests only.
`Bridge::ownerPoll()` performs settings access, hardware application and NVS I/O
on the same application task as physical controls. It uses the existing inbox,
session generations and one-request-per-owner-poll scheduling. No new task,
mutex around NVS, domain observer, or networking framework is introduced.
DMX commands continue through CommandDispatcher unchanged; configuration requests
are a separate owner-side operation, and work in both Sender and Receiver modes.

Simultaneous clients use arrival/FIFO owner order; the last processed change wins.
Already-queued intent can apply even if its client disconnects after admission;
the UI treats a lost response as uncertain and reads current state on reconnect.
Requests from an already-dead token are discarded. No automatic write replay.

## Persistence result contract

The adapter opens only `dmxui`, writes only the changed byte key, and requires
both `putUChar()==1` and matching readback before confirming persistence.

| Result | Runtime value | Saved claim |
|---|---|---|
| `applied` | Applied immediately | Confirmed by mask when commit succeeded |
| `no_change` | Already equal | Existing mask retained; no write |
| `storage_error` | Applied | Bit clear; explicitly not confirmed saved |
| `invalid` | Not changed | No write or hardware change |

Mask bits 0/1/2 mean brightness/timeout/volume respectively. Defaults with absent
or invalid storage are usable but are not reported as explicitly saved. Valid
startup bytes establish saved state. Physical previews clear the appropriate
bit until release commit. If a drag returns to the known durable value, no write
is needed. Failure clears that key's durable cache as well: a write may have
reached flash before its confirmation failed. Returning to a previously saved
value must then perform a real write, not rely on a potentially stale cache.

Failed physical commits clear pending intent and do not retry every idle frame.
An unchanged request likewise does not silently retry. The web page says
"Applied, but not saved. Change again to retry saving." A completed changed slider gesture (or legacy settings.set)
is an explicit new attempt. There is no background retry or hidden rollback.
The existing physical screen retains its layout; the web page exposes persistence
status and errors. Reboot restores whatever is actually durable, not an assumed
successful runtime value.

Arduino Preferences writes/commits remain synchronous. This removes repeated
drag writes and three-key amplification, but does not establish a bound on flash
commit latency. A single final commit/release can delay the application task. The active
immutable UART snapshot remains protected and its ISR can finish; scheduling of
the next packet may be delayed. Actual inter-frame gaps under NVS activity require
external measurement. No claim of uninterrupted wire cadence follows from host
tests. An asynchronous storage task is deliberately outside this small change.

## Backward-compatible WebSocket extension

Legacy version-1 hello, Controller snapshot/update, acknowledgments, blackout and
preset shapes remain unchanged. Clients opt in by requesting settings; only those
clients receive settings updates. Old clients never receive unsolicited new types.
An older server can reject the new request without disconnecting the updated
client's otherwise working DMX connection; only Device Settings is unavailable.

Requests, exact shapes (same monotonic client IDs and 256-byte input limit):

```json
{"v":1,"id":1,"type":"settings.request"}
{"v":1,"id":2,"type":"settings.set","setting":"brightness","value":4}
```

`setting` is exactly `brightness`, `timeout` or `volume`; `value` is an integer
within that setting's range. Unknown keys, extra fields, invalid types and values
are rejected before enqueue. A mutation requires prior settings opt-in.

Responses carry the complete small authoritative configuration:

```json
{"v":1,"type":"settings.snapshot","id":1,"revision":"1","seq":"3","brightness":3,"timeout":3,"volume":2,"persisted":7}
{"v":1,"type":"settings.update","id":0,"revision":"3","seq":"4","brightness":4,"timeout":3,"volume":2,"persisted":7}
{"v":1,"type":"settings.result","id":2,"revision":"3","seq":"5","brightness":4,"timeout":3,"volume":2,"persisted":7,"setting":"brightness","status":"applied"}
```

Settings revision is independent of Controller revision, starts at 1 and can
advance for both value and persistence confirmation. Global publication sequence
is shared; gaps are permitted, including for clients not opted in. On a changed
web operation, peers and initiator receive an update before the initiating result.
Physical previews and final persistence changes publish current full settings on
owner polls. No historical settings-event queue is introduced.

Frontend parsing validates complete exact shapes, settings ranges, mask, counters,
response ID, response family and result key. A snapshot cannot acknowledge a
mutation. It never synthesizes authoritative configuration from the draft/ack.
One settings request is outstanding per browser, with a five-second deadline.
Read timeout affects settings availability; an unknown mutation outcome retires
the session through existing recovery. Reconnect reads current settings after a
valid Controller snapshot. Generation guards cancel old responses and timers.

Settings reads begin only when Settings is first opened and on reconnect.
The request/snapshot API remains available; the final UI has no Refresh control.
Once opted in, updates remain subscribed across tab navigation. No new
HTTP polling or socket is added. Existing four-outstanding, four-client, eight-slot
outbox, 16-slot inbox, 20-request/s, 2,560-byte output and all HTTP budgets remain
unchanged. There is no queue-capacity workaround.

## Live slider transaction refinement

The previous `settings.set` remains a backward-compatible immediate apply/save
operation. It is no longer used by the slider UI. Existing read, update and
legacy result shapes are unchanged; old opted-in clients still receive normal
full configuration updates. The new request shapes are exact and remain under
256 bytes, with positive decimal-string 64-bit base revisions:

```json
{"v":1,"id":8,"type":"settings.preview","setting":"brightness","value":4,"gesture":8,"base":"6"}
{"v":1,"id":9,"type":"settings.preview","setting":"brightness","value":5,"gesture":8,"base":"7"}
{"v":1,"id":10,"type":"settings.commit","setting":"brightness","gesture":8,"base":"8"}
{"v":1,"id":11,"type":"settings.cancel","setting":"brightness","gesture":8,"base":"8"}
```

Preview has a value; commit/cancel forbid it. Read/set forbid gesture/base fields.
Only the first preview has `gesture == id`; continuation/finish has a later ID.
Monotonic request admission prevents an old first request being re-admitted.
New-operation results retain `settings.result` full values/mask and add exact
`phase` (preview/commit/cancel) and `gesture` fields. They can additionally return
`conflict`. The browser matches response family, ID, setting, phase and gesture.
A generic `settings.update` cannot acknowledge or commit a preview.

One small global lease lives in the existing DeviceSettings authority, not a
second config object. It records owning session generation, gesture, key, entry
value/persistence bit and two timestamps. The browser cannot select/spoof its
server-side session owner. Hardware and published values always reflect the same
one authority; the entry value is only a rollback snapshot.

- First preview requires an exact current revision and no pending physical
  writes. Subsequent preview/commit requires the same session/gesture/key and
  current revision. No preview invokes the writer or enters physical pending
  storage, so a physical Settings idle tick cannot accidentally save it.
- Commit ends ownership before one verified single-key persistence attempt.
  Repeated or late commits are conflicts. Returning to a known durable value
  skips the write. A failed commit retains the applied value, explicitly unsaved,
  with no hidden retry or rollback.
- Explicit cancellation restores the entry value/persistence bit without NVS.
  Cancel requires the same owner/gesture/key; it cannot roll back a newer lease.
- A fresh-revision preview from another client can supersede the lease: restore
  the old entry, then apply the new preview, publishing the final authoritative
  state once. A stale-base start is rejected. Physical setters and legacy
  `settings.set` cancel the old lease before applying the new intent, even on
  another setting. Old continuation/finish cannot overwrite that newer intent.
- The application owner checks session liveness and lease expiry before every
  Bridge owner poll, including in-flight Dispatcher early returns. Closed owners,
  10 seconds without an accepted preview, or 30 seconds total cause rollback.
  Arithmetic is wrap-safe. These are owner-poll deadlines, not hard real-time
  guarantees if the application task itself is stalled. A bad-base operation is
  rejected; any still-owned preview remains bounded by the same lease.

The browser keeps just one transient gesture and one settings request in flight.
The first eligible changed step is sent immediately; later movement is paced at
125 ms with one coalesced latest step, never a backlog. Thus at most eight
previews/s, with room for commit/cancel under the unchanged 20-request/s limit.
In very rapid separate gestures there can be up to two operations per preview.
The label changes synchronously; network latency/backpressure may skip intermediate
steps, but a completed gesture drains its final preview before sending one commit.
No commands are emitted for unchanged steps/gestures. No percentages are invented.

Pointer release commits; pointer cancellation/lost capture, Escape, blur during
an unfinished edit or navigation cancels. Keyboard range input previews natively;
key release commits, without committing each repeated change event. Assistive
change events work without pointer/keyboard prerequisites. The active slider retains native keyboard focus during a pending final save:
aria-disabled and the input admission guard reject further editing, without
temporarily setting native disabled. Other sliders and disconnected controls
remain natively disabled. A held-response browser regression reproduced focus
loss before this correction and now checks both pending and completed-save focus.
An already-ended edit cannot turn its release into a blur-cancel. Local lifetime is also bounded
at 30 seconds. Only the active slider surface uses touch-action none; surrounding
page scrolling and the accepted fixed-scale iOS policy remain unchanged.

Own update-before-result events do not move a newer local thumb backwards. A
foreign update with no own request pending interrupts the local gesture; events
arriving during a request resolve through the matching result/revision guard.
The remainder of an interrupted pointer/key gesture cannot overwrite the new
state. On socket loss all desired intent/timers are dropped. The server retires
the preview; reconnect only reads authoritative state, never replays preview,
commit or cancel. A still-detecting old connection may expose an accurate unsaved
preview briefly until session retirement/lease expiry, never indefinitely.

Preview/commit/cancel paths use the same existing hardware adapter: no wake on
remote brightness/rollback, no remote preview sounds, battery-warning override
priority preserved, no change to timeout mapping, NVS keys, Controller, DMX,
blackout, preset storage, HTTP deadlines, partitions or maintenance controls.

Extra acceptance: cancel a drag, change tab while dragging, hold beyond the lease,
disconnect/background/resume during preview and during final commit, and race a
physical edit/two web clients. Verify rollback versus newer intent, no late replay,
all three current values and saved flags, no layout movement and one final NVS
write where the final value differs from durable storage. Actual Safari/Home
Screen touch behavior and RS-485 gaps during the final commit remain hardware gates.

## Device Settings web page and hardware semantics

Three native accessible horizontal range inputs each have exactly five steps:
brightness 1–5 and volume 0–4; timeout retains data values 0–4 but presents
positions 1–5 as 1, 2, 3, 4 minutes, Always On, matching the physical device.
No Apply buttons or select elements. Endpoint labels are Dim/Bright,
1 min/Always On and Mute/Maximum. The card has a plain heading with no Refresh
button; Browser Preferences remain separate. The selected step is shown
synchronously as the value only. Local intent, applied state and persistence
confirmation remain tracked internally, without routine visible descriptions. Timeout 0 is
shown as Always On and volume 0 as Mute, including aria-valuetext. Controls retain their geometry. Empty feedback consumes no space; meaningful
errors are shown in full below the controls. Only an active local drag keeps its thumb from
following older self acknowledgments; a superseding change cancels that intent.
Browser Preferences retain their separate local-storage path and do not write NVS.

Brightness updates the normal PWM cache while asleep without touching backlight
hardware. Timeout updates do not change physical activity timestamps or wake the
screen, including Always On; the next normal physical touch wakes at the latest
brightness. Shortening the timeout while awake can cause the unchanged auto-off
logic to turn off an already-idle display on the next loop.

Remote volume changes set the normal cache silently. They do not play a preview
or reduce an active battery-warning override. The unchanged warning service
restores the latest normal value, including Mute, after the warning. Physical
preview and startup sound semantics remain intact. No Wi-Fi/service restart.

Existing header, navigation, viewport and gesture policy are unchanged. Native
iOS range behavior and physical effects still require actual iPhone acceptance;
Chromium checks do not establish Safari behavior.

The historical native HTTP/WebSocket synchronous-send stall limitation remains.
This stage does not rewrite delivery, increase queues/timeouts, or claim that
healthy mock/browser tests eliminate slow-client stalls on ESP32 hardware.
The accepted ordinary-HTTP sub-millisecond deadline correction is unchanged.

## Automated validation

- Frontend factory/navigation/live suite: passed (83 existing live groups,
  12 Status groups, loader/navigation checks, plus new Settings protocol/recovery
  cases). Covers strict ranges/shapes, independent revisions, old-server rejection,
  Receiver mode, pending/error, timeout/reconnect without replay and wrong-family
  response rejection.
- New C++ authority/Bridge tests: passed, including the preview lease suite. Every supported level, bounds, defaults,
  legacy restoration, unchanged write avoidance, release-only persistence,
  successful/failed/ambiguous writes, opt-in legacy isolation, physical-to-web,
  ordered multi-client writes, reconnect and dead generation rejection.
- Actual extracted NVS/hardware adapter tests: passed. Exact key/mappings,
  open/write failures, asleep brightness, Always On and active warning priority.
- Existing 22 freshly compiled host cases: passed. Controller, Dispatcher,
  WebSocket/actual native I/O adapter, transport ISR fault injection, SoftAP policy
  variants, station diagnostics, canvas, startup defaults, HTTP deadlines and
  generated asset API. Includes 385 deliberately incomplete UART-tail cases.
- Ten Python pipeline/network/Dispatcher/infrastructure/patch-state commands:
  passed. Fifteen asset pipeline tests and six infrastructure tests included.
  Historical full-file guards accept only pinned reviewed Settings deltas before
  reconstructing their old baselines; actual new behavior is tested separately.
  DMX/HTTP/USB/partition/driver inputs remain protected.
- Headless Chromium: Settings at 320/375/390/430/768/1280; existing header/gesture,
  blackout and preset regressions at seven widths; preset action regression at
  six widths. Passed authoritative/draft/pending/failure, fixed geometry,
  scroll/overflow, browser preferences, focus, Refresh and disconnect checks.
- Native loopback HTTP: gzip/SRI/cache, startup failures/recovery, one socket per
  browser, held Core, 1–4 staggered browsers and fail-closed burst cases passed.
  Synthetic fixtures never access device NVS, presets or hardware. The slider
  browser suite additionally uses actual pointer, keyboard and touch range events,
  a second synthetic session, no thumb echo, zero preview writes/one final write,
  cancellation, physical/peer interruption and failed-save fixed geometry.
- Asset generation twice is identical; hashes/SRI/manifest verify; all assets
  remain strictly below 16 KiB. Final build/whitespace results below.

Initial validation: `dist/device-settings-20261029/`; slider refinement artifacts:
`dist/settings-sliders-20261029/`, including an untouched before-state snapshot. Existing build
provenance correctly rejected intermediate builds changed during compilation;
final builds are rerun from settled source/assets and verify their receipts.

## Resources before the follow-up presentation polish

All four final builds passed, with verified 16 MiB layout/embedded-asset receipts.
Linked byte counts and refinement deltas relative to the preserved pre-slider
Stage 6B.5A builds (RAM 88,020/90,732; flash respectively 1,073,505,
1,073,481, 1,078,077 and 1,078,049 bytes):

| Environment | Static RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| `cores3se_unit` | 88,644 | +624 | 1,075,865 | +2,360 |
| `cores3se_base` | 88,644 | +624 | 1,075,841 | +2,360 |
| `cores3se_unit_diagnostics` | 91,356 | +624 | 1,080,449 | +2,372 |
| `cores3se_base_diagnostics` | 91,356 | +624 | 1,080,421 | +2,372 |

One identical final source fingerprint across all four build receipts:
`b7ec9bac90d4c2d4473dd5eb83484bd600865ca64b78c74d2ae76662d3b82566`.
`git diff --check` passes; new files are also checked for trailing whitespace.

| Gzip asset | Pre-slider bytes | New bytes | Delta |
|---|---:|---:|---:|
| HTML | 4,270 | 4,230 | -40 |
| CSS | 4,475 | 4,487 | +12 |
| Core JS | 9,782 | 10,103 | +321 |
| UI JS | 9,503 | 10,320 | +817 |

All embedded assets remain flash-resident. No additional framebuffer or task.
The settings authority uses fixed small state; queue lengths are unchanged.
Host structure sizes are not ESP32 ABI measurements; linked firmware static RAM
and flash figures are the relevant comparison. Runtime heap, stack and NVS latency
must still be measured on hardware.

## Hardware acceptance still required

1. Record existing physical brightness/timeout/volume; boot with preserved NVS.
   Confirm restored values/defaults and unchanged presets, Wi-Fi and DMX.
2. iPhone Safari and Home Screen: drag through every brightness/timeout/volume step.
   Verify hardware responds before release, one final save synchronizes physical
   UI and another client's Settings page. Check native ranges, keyboard/focus,
   scrolling, safe areas, stable feedback/header/nav and existing gesture policy.
3. Change settings physically while two web clients show Settings. Drag across
   levels and release; verify live values then Saved, with one final persistence.
   Change from two clients; verify owner order and final agreement.
4. Put display to sleep. Change brightness and timeout remotely (also Always On):
   screen stays dark. First physical touch wakes at new brightness and operates
   no control. Verify normal timeout behavior after wake.
5. Set volume Mute and trigger the existing battery warning through the controlled
   hardware procedure. Change UI volume during the tone; warning priority remains
   high, then the current normal volume is restored. Remote adjustments are silent.
6. Disconnect during preview/final commit, reconnect/background/resume and reopen Settings:
   controls are unavailable until synchronization, current settings restore,
   unknown mutation is never replayed. DMX/presets/blackout remain operational.
7. Reboot only after confirmed Saved, verify durability without erasing NVS.
   Storage-failure branches are host-fault-injected; do not corrupt device flash
   to test them. Unsaved state must never be mistaken for durable configuration.
8. Measure heap/stack and touch responsiveness with multiple clients and physical
   dragging. Capture RS-485 frame length/coherence/break/MAB/gaps around NVS final commit
   and release with external equipment. Unit stability and experimental Base
   glitches remain separate concerns. Host tests do not establish electrical
   correctness or a real-time upper bound on NVS duration.

## Future maintenance controls — design only

No new remote Restart or Wi-Fi-password mutation exists in this implementation.
Existing physical controls remain as before.

Restart should require explicit confirmation and administrative authorization,
optionally a short physical confirmation window. Stop accepting new commands,
resolve pending configuration writes, enqueue an acknowledgment and allow its
bounded delivery before transport teardown/reboot. Use the Stage 3 ownership
contract and an explicitly approved output policy: the legacy shutdown helper
does not establish that three actual zero frames were transmitted. Never reclaim
an ambiguous active snapshot. A reboot acknowledgment is intent, not proof of
reboot; reconnect must read fresh snapshots and never replay the request.

Wi-Fi credentials currently use `dmxwifi/ap1`: version-2 108-byte Credentials with
origin legacy/factory/custom, with compatible reading of the original version-1
64-byte blob. Factory-reset intent is the separate `factory` key. A future password
change should validate the existing 8–63 printable-ASCII policy, persist/read-verify
a custom-origin v2 blob, preserve SSID and all other namespaces, and never
automatically replace legacy/custom credentials with predictable defaults.

Require physical or explicitly authenticated administrative approval; AP association
and predictable MAC-derived factory passwords are not sufficient authorization.
Do not expose the current password via Settings/Status, broadcast the submitted
password, log it, store it in browser preferences, or echo it in errors. The current
HTTP/WS network is unencrypted beyond WPA2; secret submission needs a reviewed
trusted administrative channel/physical authorization and an explicit threat
model. Validate/persist first; acknowledge accepted intent before a scheduled,
bounded AP restart that disconnects clients. Lost acknowledgment must not replay
rotation. Supply a physically authorized offline factory-recovery procedure,
without erasing presets or UI settings. Partial write/restart failures need
explicit failure states and hardware testing before this is implemented.

## Exact Stage 6B.5A file inventory

Production (8): `include/device_settings.h`, `include/websocket_bridge.h`,
`include/websocket_service.h`, `src/src.cpp`, `src/view_sender.h`,
`src/websocket_bridge.cpp`, `src/websocket_protocol.cpp`, `src/websocket_service.cpp`.

Web source/composed output (9): `web/src/index.html`, `web/src/style.css`,
`web/src/js/app.js`, `web/src/js/commands.js`, `web/src/js/protocol.js`,
`web/src/js/settings.js`, `web/src/js/transport.js`, `web/src/core.js`, `web/src/ui.js`.

Generated (5): `web/generated/index.html.gz`, `web/generated/style.css.gz`,
`web/generated/core.js.gz`, `web/generated/ui.js.gz`, `web/generated/manifest.json`.

Tests/guards (15): `tests/dispatcher/check_integration.py`,
`tests/network/check_integration.py`, `tests/network/preset_loaded_guard_support.py`,
`tests/network/device_settings_guard_support.py`, `tests/settings/check_adapter.py`,
`tests/settings/run_tests.py`, `tests/settings/test_settings.cpp`,
`tests/settings/test_adapter.cpp`, `tests/settings/test_preview.cpp`,
`tests/web/check_pipeline.py`, `tests/web/check_scope.py`,
`tests/web/run_tests.mjs`, `tests/web/test_header_browser.cjs`,
`tests/web/test_device_settings.mjs`, `tests/web/test_device_settings_browser.cjs`.

Documentation: this report. Historical files, the unrelated Stage 3 regression
document and stash remain intact. No upload, flash erase, NVS access on the
device, push, merge, tag or release. Only Phase A was committed.

## Exact slider-refinement delta

The preserved uncommitted pre-slider Stage 6B.5A state is snapshotted under
ignored `dist/settings-sliders-20261029/before/`. This refinement changes only
the following 27 files relative to that state (the complete uncommitted Stage
6B.5A inventory above is 38 files):

- `docs/STAGE6B5A_DEVICE_SETTINGS.md`
- `include/device_settings.h`
- `include/websocket_bridge.h`
- `src/websocket_bridge.cpp`
- `src/websocket_protocol.cpp`
- `tests/network/device_settings_guard_support.py`
- `tests/settings/run_tests.py`
- `tests/settings/test_adapter.cpp`
- `tests/settings/test_preview.cpp`
- `tests/web/check_pipeline.py`
- `tests/web/test_device_settings.mjs`
- `tests/web/test_device_settings_browser.cjs`
- `tests/web/test_header_browser.cjs`
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/src/core.js`
- `web/src/index.html`
- `web/src/js/app.js`
- `web/src/js/commands.js`
- `web/src/js/protocol.js`
- `web/src/js/settings.js`
- `web/src/js/transport.js`
- `web/src/style.css`
- `web/src/ui.js`

No stage, commit, upload, push, merge, tag, NVS erase or maintenance feature was
performed. Existing stash and unrelated/historical contents are preserved.

## Follow-up Settings presentation polish — 2026-10-29

The user reports that live Device Settings behavior works well. This follow-up
changes presentation/layout and the explicitly requested timeout position order.
It does not change the settings transaction protocol or device behavior.

- Settings now use the Editor's 44 × 18 px rectangular thumb, 5 px corner radius,
  blue fill/border and dark 6 px track, with WebKit and Firefox styling rules.
  The native range's interactive area remains 44 px high. No new gesture handlers.
- Each range has five visible, evenly spaced ticks/labels. Their centers align
  with the native thumb's travel endpoints and quarter positions. Brightness is
  1–5; volume is 0–4; timeout is 1, 2, 3, 4, ON.
- The physical source at `drawSettingsScreen()` maps timeout 0 to position 5,
  and `handleSettingsTouch()` maps position 5 back to 0. The web now uses exactly
  that presentation mapping: Home = 1 minute, End = Always On. Its payload,
  authoritative current text, NVS byte and aria-valuetext still use the actual
  semantic value; no new duration, default or mapping is introduced.
- The left controls column is 92% wide, capped at 360 px. This shortens mobile
  rails modestly and desktop rails substantially, leaving usable space on the
  right for a later maintenance-controls design. No fake buttons, placeholders,
  Restart/Power Off commands or administrative behavior are added.
- No changes to preview pacing, owner/gesture/revision guards, final commit,
  cancellation, sync/recovery, persistence failures, physical UI, sleep/speaker
  protection, browser preferences, header or navigation.

Automated validation passed: complete frontend/navigation/live tests; all five
timeout-position/payload/current-text/peer-sync cases; Settings browser suite at
320/375/390/430/768/1280 px; seven-width header/gesture regressions; preset action
regressions; native local HTTP gzip/SRI/cache/startup/recovery; ten pipeline/network/
infrastructure/patch-state checks; 22 existing host cases and three Settings host
executables. Browser assertions measure the actual Chromium native shadow thumb
at 44 × 18 px, all five tick/text centers, compact column width, 44 px touch area,
no horizontal overflow, stable layout, delayed-save keyboard focus and unchanged
preview/commit/cancel/sync behavior. Mobile/desktop screenshots were inspected.
Firefox rules are supplied, but actual Firefox and iOS Safari rendering are not
established by Chromium. Physical iPhone Safari/Home Screen acceptance remains.

Generation twice is identical; hashes, manifest, all four build/component receipts
and `git diff --check` pass. Logs/snapshots/screenshots are below ignored
`dist/settings-polish-20261029/`. Final source fingerprint:
`f7e7b98b3fcb08b2a1dfe6e75caf5e93d3f2e0072b267a1a1e061e84e52ad97d`.

| Asset | Pre-polish gzip bytes | Current bytes | Delta |
|---|---:|---:|---:|
| HTML | 4,230 | 4,277 | +47 |
| CSS | 4,487 | 4,655 | +168 |
| Core JS | 10,103 | 10,103 | 0 |
| UI JS | 10,320 | 10,350 | +30 |

Every asset remains below 16 KiB. All four builds pass:

| Environment | Static RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| Unit | 88,644 | 0 | 1,076,097 | +232 |
| Base | 88,644 | 0 | 1,076,073 | +232 |
| Unit diagnostics | 91,356 | 0 | 1,080,697 | +248 |
| Base diagnostics | 91,356 | 0 | 1,080,653 | +232 |

Exact follow-up delta (12 files):

- `web/src/index.html`
- `web/src/style.css`
- `web/src/js/settings.js`
- `web/src/ui.js`
- `web/generated/index.html.gz`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/generated/manifest.json`
- `tests/web/test_device_settings.mjs`
- `tests/web/test_device_settings_browser.cjs`
- `tests/network/device_settings_guard_support.py` (only the reviewed UI hash)
- `docs/STAGE6B5A_DEVICE_SETTINGS.md`

Next physical check: compare all five timeout stops with the physical controller,
including 4 minutes → Always On and a physical Always On change projecting to
the rightmost web stop. Recheck rectangular-handle dragging/tapping, release save,
cancel/interruption, keyboard focus, saved/current text and scrolling outside the
sliders on iPhone Safari and Home Screen. Restart/Power Off placement, confirmation,
authorization and safe shutdown behavior remain deferred to their own phase.

Everything remains unstaged/uncommitted. Existing work, historical files and stash
are preserved. No firmware upload, push, merge or device NVS operation occurred.

## Final quiet Settings cleanup — 2026-10-29

The user physically approved the five-stop slider styling, tick marks, timeout
order and live synchronization. This final presentation cleanup awaits its own
physical acceptance. There are no protocol, transaction, persistence or device
behavior changes.

- Removed the Refresh button and its listener/disabled-state renderer. The header
  is now just the existing Device Settings heading, with no flex gap, empty wrapper
  or minimum button-height spacing. The read/request/snapshot capability remains
  intact in transport and commands; initial reads, subscriptions and reconnect
  reads remain automatic. No HTTP polling or replacement request loop was added.
- Current-value labels contain only actual level/time/Mute text: `3`, `2 min`,
  `Always On`, etc. Removed all routine Saved/Selected/Applied/Saving/Cancelling
  suffixes. The native control continues to show local intent during a drag,
  without claiming that it was acknowledged or saved. Outside a local gesture,
  the value comes from authoritative state. Existing pending admission guards,
  aria-busy/aria-disabled and acknowledgment/persisted-mask tracking are preserved.
- Removed routine Waiting for device/Awaiting device settings footer text. The
  empty existing live region is hidden and has no height or margin. Real
  transport/rejection/conflict/persistence errors still use the existing error
  data and are shown in a readable, wrapping error region. In particular,
  `Applied, but not saved. Change again to retry saving.` stays visible on a
  confirmed failed commit. An unset persisted bit alone is not invented into a
  failure: it can also represent a legitimate preview or an unstored default.
- Loading remains safe: controls are disabled and values explicitly Unavailable
  until authoritative settings arrive. On loss of synchronization the existing
  connection indicator reports the real connection state, controls are disabled,
  and aria-valuetext is also set to Unavailable instead of retaining an old value.
- Successful operations, including loading and pending/ack transitions, do not
  change card/control/header/navigation geometry. A genuine error intentionally
  adds its readable message below the controls; there is no permanent empty
  space reserved for that exceptional content. The sliders themselves do not
  move when an error appears. Errors are not clipped or hidden indiscriminately.

Validation passed:

- Full frontend/navigation/live suites and focused quiet-settings assertions.
  The underlying settings read API is explicitly exercised after removal of its
  visible button. Timeout ordering, final commit, ownership and no-replay cases
  still pass.
- Native Chromium Settings suite at 320/375/390/430/768/1280 px, including a held
  initial snapshot (disabled controls, Unavailable values, zero feedback height),
  no Refresh/empty header, values-only text, silent successful preview/commit,
  one final write, delayed-save keyboard focus, peer/physical synchronization,
  visible failed-save message, recovery to quiet success, actual disconnect and
  automatic fresh snapshot with no mutation replay. One settings read per initial
  session and no extra reads during normal edits are asserted. The existing
  44 × 18 px handles, five aligned ticks, 44 px touch height, timeout endpoints,
  width cap, no horizontal overflow and fixed navigation are still checked.
- Seven-width header/gesture/browser regressions; preset action regressions;
  native local HTTP gzip/SRI/cache/startup/recovery; ten existing pipeline/network/
  Dispatcher/infrastructure/patch-state checks; 22 firmware host cases and three
  Settings host executables. No real device/preset/NVS operation occurs in tests.
- Deterministic generation twice, 16 KiB per-asset limits, all four build/component
  receipts and whitespace checks. Phone and desktop screenshots were inspected.

Artifacts/snapshots are under ignored `dist/settings-quiet-20261029/`. Final source
fingerprint: `f5f4255a4b4fbc28a3f4632ee392be325827f0b7a71a2836f904c56d0c187d55`.

| Asset | Before cleanup | Current gzip bytes | Delta |
|---|---:|---:|---:|
| HTML | 4,277 | 4,260 | -17 |
| CSS | 4,655 | 4,633 | -22 |
| Core JS | 10,103 | 10,103 | 0 |
| UI JS | 10,350 | 10,223 | -127 |

All four builds pass; static RAM is unchanged:

| Environment | Static RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| Unit | 88,644 | 0 | 1,075,945 | -152 |
| Base | 88,644 | 0 | 1,075,921 | -152 |
| Unit diagnostics | 91,356 | 0 | 1,080,529 | -168 |
| Base diagnostics | 91,356 | 0 | 1,080,501 | -152 |

Exact cleanup inventory (12 files):

- `web/src/index.html`
- `web/src/style.css`
- `web/src/js/settings.js`
- `web/src/ui.js`
- `web/generated/index.html.gz`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/generated/manifest.json`
- `tests/web/test_device_settings.mjs`
- `tests/web/test_device_settings_browser.cjs`
- `tests/network/device_settings_guard_support.py` (reviewed UI hash only)
- `docs/STAGE6B5A_DEVICE_SETTINGS.md`

Final physical check: iPhone Safari/Home Screen and desktop should show value-only
labels, no Refresh or empty footer, unchanged drag/release/ticks/timeout behavior,
quiet successful synchronization, safe initial/reconnect loading and identifiable
real failures. Chromium does not replace actual iPhone validation. Restart/Power
Off remain unimplemented. All work remains unstaged/uncommitted; unrelated work,
historical files and stash are preserved. No upload, push, merge or NVS erase.

## Stage 6B.5A closure acceptance

The user physically accepted authoritative brightness/timeout/volume, live preview
and release persistence, five discrete stops, physical/web/multi-client sync,
responsive mobile Settings, shorter Editor faders, fixed-scale iPhone gestures and
red authoritative lower blackout status. Prior pending acceptance statements above
are historical. Current resources and final polish are recorded in
`STAGE6B_RESPONSIVE_UI_POLISH.md` (HTML 4,269; CSS 4,771; Core 10,103; UI 10,276 gzip
bytes; Unit/Base RAM 88,644; diagnostics RAM 91,356).

Closure reran the full frontend, Settings/protocol/adapter host cases, 22 existing
Controller/Dispatcher/transport/network host cases, all ten infrastructure/scope/
patch regressions, native browser suites and all four PlatformIO builds. Assets
were regenerated twice and verified deterministic, under 16 KiB each. Detailed
logs are in ignored `dist/stage6b5-closure/`. No functional changes were made during
closure. The existing historical folder, unrelated regression document, logs and
stash are excluded. Maintenance is a separate subsequent implementation; no power
operation was executed as part of this closure.
