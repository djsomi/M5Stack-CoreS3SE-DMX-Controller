# Local Wi-Fi power control and Settings footer

Implemented locally on `codex/stage6b-web-ui`, based on
`9fdf07ee02340003e2ac4efc3daf61ce8c787062` plus the preserved release,
diagnostic-isolation, captured-fader and accepted fullscreen-navigation work.
Runtime Wi-Fi ON/OFF and the shared x=10..240 slider/button alignment have
passed user physical acceptance. The subsequent one-second hold/quiet-label
change awaits acceptance. No device has been flashed by this task.

## Initial Wi-Fi implementation inventory

Modified (relative to the incoming working tree):

- `include/network_service.h`: local request API, intent/actual state and STOPPING.
- `src/network_service.cpp`: persisted preference and serialized worker lifecycle.
- `src/view_sender.h`: Wi-Fi control, capture, invalidation and footer relocation.
- `tests/navigation/run_tests.py`: normalize only the separately tested addition.
- `tests/navigation/test_navigation.cpp`: fake field for toggle-capture reset.
- `tests/network/fader_guard_support.py`: chain the exact new scope adapter.
- `tests/wifi/check_integration.py`: retain historical worker checks separately
  from the newly tested lifecycle and update content-height expectation.
- `tests/wifi/test_settings_render.cpp`: new row/capture/state regression cases.

Added:

- `tests/network/wifi_power_delta.json`: exact before/after reconstruction data.
- `tests/network/wifi_power_guard_support.py`: strict reviewed-delta reversal.
- `tests/wifi_power/run_tests.py`: current-source extraction and test runner.
- `tests/wifi_power/test_lifecycle.cpp`: SDK/NVS lifecycle fault-injection cases.
- `tests/wifi_power/test_offline_controller.cpp`: domain/session recovery cases.
- `docs/SETTINGS_WIFI_POWER.md`: behavior, evidence and physical acceptance plan.

All other incoming changes remain intact. Build/test outputs are confined to
ignored `.pio` and `dist/settings-wifi-power-20261031/` paths.

## Architecture and behavior

The current firmware configures **SoftAP only**, not STA or AP+STA. Both Unit
and Base use the same network manager. No profile, radio security, station
limit, DHCP routing, credential format, WebSocket protocol or partition changed.

`network_service::requestEnabled(bool)` is a local application-owner operation.
It verifies a separate `dmxwifi/enabled` U8 preference before submitting intent
to the existing network worker. Missing preference defaults to ON, without a
write. Values 0 and 1 are OFF/ON; a bad type/value leaves the radio disabled with
an error until explicit local repair. Credential blobs `ap1`, transaction
journal `ap_try`, and next-boot reset intent `factory` are unchanged. Existing
presets, device settings, battery statistics and migration offsets are untouched.

- OFF closes the native HTTP server first. Its existing close callbacks retire
  WebSocket client generations; no HTTP callback can survive successful server
  shutdown. Then the worker stops Wi-Fi, unregisters its association handler,
  destroys the AP netif and deinitializes the radio driver. Station/IP and pump
  state are cleared only after successful cleanup.
- ON reuses saved credentials and the established AP initialization, protected
  WPA2 configuration, DHCP router suppression, memory guards and HTTP routes.
  A new HTTP server accepts fresh client generations and initial snapshots.
- There is one worker, retained while OFF or failed. The existing `Phase` state
  gains STOPPING; no parallel network manager or networking task is introduced.
  TCP/IP initialization and default-event-loop setup are guarded for process
  lifetime. The installed ESP-IDF `esp_netif.h` requires `esp_netif_init()` to be
  called once; its global stack is not deinitialized on a radio toggle.
- Startup and teardown are serialized on that worker. The application loop
  does not call radio/HTTP stop functions or wait for them. The preference
  commit/readback is synchronous on the owner, like the existing local Wi-Fi
  reset/settings persistence; its hardware latency still needs acceptance.
- No implicit retry follows startup/runtime failure, and OFF never invokes AP
  fallback or an automatic restart. Explicit local ON can retry a clean failure.
  Unverified teardown latches a lifecycle fault and forbids resource reuse until
  reboot; the UI reports FAILED rather than claiming a successful transition.
  A worker-creation failure also requires reboot. The physical Restart control
  remains available. These are error recovery limits, not normal OFF behavior.
- Requests are rejected during PREPARED/STARTING/STOPPING, during a credential
  approval/application transaction, from other tasks, or after unsafe teardown.
  No credentials can be changed concurrently with a power transition. NVS write
  or readback errors leave running intent unchanged and display an error. An
  uncertain write may nevertheless have reached storage; next boot rereads it.
- OFF affects connectivity, not Controller state, blackout or DMX. Commands
  already admitted retain their existing semantics. Closed generations cannot
  replay queued browser mutations. Local input continues through the unchanged
  Controller/Dispatcher/transport loop. Reconnected clients receive current state.

The requested preference is distinct from confirmed driver state. A failed ON
attempt stays saved ON for a subsequent reboot but is never displayed as a
successful ON. A next-boot factory-reset marker does not silently enable Wi-Fi
or apply new credentials to the running instance.

## Touchscreen layout

- Shared inclusive visible bounds for Brightness, Screen Timeout, Volume and
  both Wi-Fi buttons are x=10..240. Slider centers travel from x=18 to x=232;
  the unchanged 8-pixel handle radius reaches both visible edges. The track
  span is 214 pixels. Buttons use x=10 and width 231 to include pixel x=240,
  with the existing height 50. Drawing and hitboxes share these bounds.
- Five slider stops are x=18/71/125/178/232. Drawing and nearest-step mapping
  use proportional integer arithmetic so a span not divisible by four still
  reaches the last endpoint. Brightness 1..5, timeout 1..4/Always On and Volume
  0..4, their defaults, persistence and preview behavior remain unchanged.
- Slider labels and Wi-Fi button labels are centered at x=125. Title, network
  information, reset notice and footer retain x=132. Normal Wi-Fi labels remain
  `Wi-Fi: ON` / `Wi-Fi: OFF` and STARTING/STOPPING, with the
  same Font0, rounded border and confirmed-state colors. All y positions remain
  unchanged: ON/OFF y=288..337, RESET y=434..483.
- ON/OFF requires a continuous 1-second hold of the captured point. Early
  release, leaving the button, the existing 9-pixel movement guard or identity
  change cancel it. The requested direction is captured at press-down, so an
  intervening radio failure cannot reverse intent. One completed hold submits
  only once, including rejection; a new hold requires release. The elapsed
  check uses unsigned `millis()` arithmetic and `isPressed()`, not just nonzero
  state: M5Unified release states 2/6/10/14 are nonzero but are not pressed.
- Confirmed ON uses exactly `TFT_GREEN`, as RESET's active border; confirmed OFF
  uses `TFT_WHITE`. A live hold uses `TFT_YELLOW` and `KEEP HOLDING...`, clearly
  distinct from persistent ON. PREPARED/STARTING/STOPPING uses gray and does not
  arm a hold. The existing error line remains visible. A requested but not yet
  confirmed ON does not turn green. RESET's hold handler remains unchanged; its normal label is `RESET WIFI`.
- Navigation cancels local capture. The handler only flags redraw on interaction
  transitions; it does not redraw continuously, block, add polling or alter
  touch-library thresholds. Speaker-deferred rendering is preserved.
- Network status/SSID/password/IP centers are y=356/374/392/410. The reset
  button occupies y=434..483; its notice is centered at y=502. Revision
  invalidation includes the Wi-Fi button through the reset notice. The normal
  notice is `RESET TAKES EFFECT AFTER REBOOT`. Password visibility and the
  existing text-fitting logic are unchanged.
- Build Date is the final content element at y=540; content height is 562 and
  maximum scroll is 322. At maximum scroll it is at screen y=218, below reset
  information. Existing timestamp value, format, font, muted color and build
  provenance generation are unchanged. Right-hand power/back controls are fixed.

## Automated validation

All passed:

- `tests/wifi_power/run_tests.py`: exact production lifecycle/request/preference
  code with SDK/NVS fakes; absent/persisted/corrupt preferences; OFF/ON; one-time
  TCP/IP/event initialization; saved AP security/channel/client count; startup
  API failures; queued HTTP work retirement; stop/unregister/deinit failures;
  no resource reuse after failed teardown; no implicit restart; admission and
  persistence errors. Actual touchscreen methods test hitboxes, movement and
  identity cancellation, duplicate suppression, actual ON/OFF and error text.
- Real Controller/Dispatcher/Bridge test: retire two clients, discard a queued
  stale-generation command, mutate local channel 23 while offline, reconnect
  with fresh complete snapshots and reject old-generation traffic.
- Fourteen established validation groups: navigation (including required
  pre-fix failures), investigation isolation, THDIFF host simulation only,
  read-only snapshot host tests, fader and acquisition regressions, network/
  transport/patch/infrastructure checks, 23 core host programs, frontend suites,
  ten browser suites, settings, maintenance, Wi-Fi and release-candidate checks.
- Final-source network/transport/scope regression rerun. Exact inverse-delta
  adapter retains historical scope guards instead of relaxing their DMX,
  touch, power, USB, protocol, schema or packaging protections.
- Deterministic asset generation twice in an isolated directory matched every
  checked gzip asset and the manifest byte-for-byte. No web asset changed.
- All four final PlatformIO builds and ELF inspection: captured raw-fader path
  present; Stage 0 only in diagnostic environments; THDIFF, acquisition,
  configuration snapshot and physical-fader investigation implementations absent
  in all four; no APPLY/RESTORE experiment command markers.
- Git whitespace checks and preservation inventory.

| Environment | Static RAM | Flash | RAM delta | Flash delta |
|---|---:|---:|---:|---:|
| cores3se_unit | 95,924 | 1,102,253 | +16 | +1,952 |
| cores3se_base | 95,924 | 1,102,229 | +16 | +1,952 |
| cores3se_unit_diagnostics | 98,652 | 1,106,817 | +24 | +1,936 |
| cores3se_base_diagnostics | 98,652 | 1,106,789 | +24 | +1,924 |

Bytes relative to the accepted navigation-fix working tree. The existing 8 KiB
worker stack is now retained while OFF/failed; there is no second worker or
second stack. Runtime heap recovery and task scheduling are not measured here.

Unchanged gzip sizes: HTML 4,950; CSS 5,199; Core JS 12,467; UI JS 12,624 bytes.
Every asset remains below 16,384 bytes.

Source fingerprint:
`cf0356740b5d004df04afbb6cdb98996520895f54ef56c321ae3d7366de9c3ab`

Build ID: `0.3.0-rc.1+9fdf07ee0234.modified.cf0356740b5d`.
Initial implementation evidence is retained in
`dist/settings-wifi-power-20261031/`; layout-refinement evidence is in
`dist/settings-wifi-layout-20261031/`; final hold refinement evidence is in
`dist/settings-wifi-hold-20261031/`. Original RC
archives/checksums, historical files, stash, version and provenance logic remain
unchanged. No replacement RC bundles were prepared.

## Manual acceptance still required

1. Install the intended Unit production build manually. Confirm boot label,
   normal DMX output, captured-fader behavior and accepted scrolling navigation.
2. Verify Wi-Fi ON with the existing SSID/password; connect two browser clients.
   Scroll Settings to the local control. Hold it for 1 second to request OFF;
   confirm both clients
   lose connectivity and the AP disappears. Confirm OFF only after transition.
3. While OFF, exercise physical fader, Precision, presets, blackout, speaker and
   sleep/wake. DMX must continue; software tests do not establish electrical
   packet integrity or wire timing.
4. Hold for 1 second to request ON locally. Confirm the same AP/security/credentials, DHCP behavior
   and station count. Rejoin from clients; existing automatic recovery must
   obtain fresh snapshots. If its bounded retry window expired during OFF,
   use the existing manual Retry. No queued mutation may be replayed.
5. Repeat OFF/ON several times, with idle and actively updating clients. Check
   failed/slow sends remain diagnosed and no sustained heap decline, orphaned
   sessions, radio restart while OFF or UI/DMX stalls appear. The historical
   synchronous HTTP/WebSocket send limitation is unchanged.
6. Save OFF and perform a normal restart. Confirm no AP starts and local ON is
   reachable. Re-enable, restart again and confirm the same saved credentials.
7. Verify an existing installation without the key starts ON. Verify credentials,
   presets and non-default settings persist. Do not erase NVS for this test.
8. Check slider-versus-toggle hitboxes, movement away from a toggle, interrupted
   taps, screen exit/re-entry and the footer at maximum scroll, with and without
   speaker-deferred rendering. The footer must not overlap any control.
9. Confirm credential approval/recovery and Maintenance still work normally after
   re-enabling; do not invoke factory reset merely to test the toggle.
10. Repeat appropriate runtime checks on Base. Base DMX remains experimental;
    its existing glitches are not evidence about this Wi-Fi lifecycle change.

Host/browser tests do not establish RF shutdown, real station reassociation,
NVS power-loss behavior, touchscreen rasterization or electrical DMX timing.


## Accepted-runtime layout refinement

The user accepted the runtime ON/OFF implementation before this refinement.
Only touchscreen rendering coordinates, button sizes, matching touch predicates,
redraw extent and one informational string changed. Radio, persistence, reset
hold timing, capture/cancellation logic and fader/domain behavior are unchanged.
The first button moved down 16 pixels; button height increased from 28 to 50
and width from 216 to 220. The enlarged buttons share geometry constants for
both painting and hit testing. The footer retains its 22-pixel bottom margin.

Files changed specifically for the layout refinement:

- `src/view_sender.h`
- `tests/network/wifi_power_delta.json`
- `tests/network/check_softap_integration.py`
- `tests/wifi/check_integration.py`
- `tests/wifi/run_tests.py`
- `tests/wifi/test_settings_render.cpp`
- `tests/wifi_power/run_tests.py`
- `docs/SETTINGS_WIFI_POWER.md`

Targeted tests compile the actual button rendering, both touch handlers and
footer visibility block. They exercise all four button edges, excluded adjacent
pixels and the right column, scroll positions through the new limit, full-length
credentials without mutation/new fitting behavior, hold/reset cancellation,
pending/errors and the footer at screen y=218. The test runner now generates
both current touch-method headers and the footer header in a fresh output path.
The exact historical scope adapter has been updated for this layout only.

Navigation regressions (including pre-fix/partial-fix failures), network and
transport integration, core host cases, Wi-Fi configuration/approval/recovery,
fader and diagnostic-isolation checks are rerun for this refinement, along with
all four firmware builds and Git whitespace checks. Static RAM, flash and gzip
sizes were unchanged at that layout-only step (see its verified.json). The
resource table above now reflects the subsequent hold refinement. Physical visual acceptance remains required for spacing, readability,
button edges and footer scrolling on the real display. Earlier full browser
validation remains recorded above; this refinement does not change web sources.


## Final hold/color/left-edge refinement

Current changes are limited to these five files:

- `src/view_sender.h`: ON/OFF hold ownership/timestamp, confirmed-state border,
  holding feedback, shared left extent and centering of button labels only.
- `tests/wifi/test_settings_render.cpp`: actual M5Unified state-bit semantics,
  hold/cancel/one-shot/wraparound/direction/second-point tests, border colors and
  revised left-edge/hitbox assertions; RESET and scroll/footer tests retained.
- `tests/wifi_power/run_tests.py`: focused source invariants for hold admission,
  unchanged text fitting/default information position and Volume handle radius.
- `tests/network/wifi_power_delta.json`: refresh only the reviewed view delta;
  network implementation and network header entries remain unchanged.
- `docs/SETTINGS_WIFI_POWER.md`: final semantics, evidence and acceptance notes.

The network worker, NVS persistence, radio setup, WebSocket protocol, Controller,
DMX transport, faders, right-column controls and vertical layout are unchanged.
Only the two button labels receive a different horizontal center; SSID,
password, IP, clients, notices and Build Date retain their text and positions.
No source or dependency involved in touch-controller acquisition is changed.

Automated results: targeted actual-method tests pass, including no request at
1999 ms, one at 2000 ms with the touch bit set, no retrospective completion on
release, independent OFF/ON holds (including disabled Wi-Fi), cancellation that
cannot be stolen by another point, failed-request one-shot behavior and no green
border solely from an ON request. The existing six regression groups and all
four firmware builds pass. ELF inspection retains Stage 0 only in diagnostics
and excludes suspended investigation implementations in all four environments.
Assets remain unchanged. Git whitespace and preservation checks pass.

Versus the accepted previous layout: static RAM increases by 8 bytes in each
build; flash increases by 264 bytes for normal Unit/Base, 296 for Unit diagnostics
and 268 for Base diagnostics. No additional task, timer, wait or loop is added.

Manual acceptance: verify the x=0 screen-edge alignment and 220x50
hitboxes, no activation on taps or short holds, yellow holding feedback,
exactly one action per full hold, leave/re-enter cancellation, separate OFF/ON
holds, green only after confirmed ON and neutral OFF. Check pending/failure
feedback, unchanged RESET hold, footer visibility and navigation. No automatic
flash or device register/NVS operation has been performed.


### Earlier x=0 alignment clarification (superseded)

The user's final choice supersedes the initially assumed x=14 handle boundary.
Both button outlines and hitboxes are x=0..219, with label centers at x=110.
All y positions, 220x50 dimensions, the Volume slider and the right column stay
unchanged. The two-second hold/color behavior is unchanged. Tests now include
x=0 and x=219 acceptance, x=-1/x=220 exclusion and label centering, while network
information remains centered at x=132. Targeted tests, relevant navigation,
network/transport/scope and isolation checks plus all four builds pass. Evidence
is in `dist/settings-wifi-x0-20261031/`. No hardware upload, staging or commit occurred.


## Final shared x=10..240 alignment

This latest user instruction supersedes the earlier x=0/x=250 alternatives.
All three Settings sliders and both Wi-Fi buttons now use the shared bounds
specified above. Right-column controls, vertical layout, two-second hold,
network worker, NVS, physical DMX fader and all other screens remain unchanged.
The text-fitting logic, network values and Build Date formatting are untouched.

Changes in this alignment task:

- `src/view_sender.h`: shared bounds, inset slider travel, matching touch extent,
  proportional five-stop drawing/mapping and centered slider labels.
- `tests/wifi/test_settings_render.cpp`: x=10/240 edge acceptance, x=9/241
  exclusion, label centers and retained hold/color/reset/scroll/footer checks.
- `tests/wifi_power/run_tests.py`: current geometry checks and actual slider
  function extraction, alongside unchanged lifecycle and offline-domain tests.
- `tests/wifi_power/test_slider_alignment.cpp` (new): actual Brightness/Timeout/
  Volume rendering and mapping under drawing fakes, all five positions, endpoint
  overhang, min/max and monotonic/clamped values across the full touch width.
- `tests/network/wifi_power_delta.json`: exact reviewed view reconstruction only.
- `docs/SETTINGS_WIFI_POWER.md`: current geometry, results and acceptance record.

The installed M5GFX implementation was checked: `fillCircle` draws x-r..x+r
using a 2r+1 center scanline; `drawRoundRect` draws its far border at x+w-1.
The host bounds test uses those exact primitive extent contracts. It establishes
coordinate/layout correctness, not physical LCD/touch behavior.

Before the usage-limit interruption all four firmware builds and the focused
alignment/hold tests had succeeded. On resume the receipts were checked against
the current source fingerprint; builds were not repeated. The previously
unexecuted navigation, network/transport/asset/scope, core, Wi-Fi, fader and
isolation regression groups then passed. Normal and diagnostic builds retain
their existing isolation. Assets are unchanged and Git whitespace checks pass.
Evidence is in `dist/settings-alignment-10-240-20261031/`.

Static RAM is unchanged from the previous x=0 build; flash increases by 16 bytes
for Unit, Base and Unit diagnostics, and 32 bytes for Base diagnostics. The
current totals appear in the resource table above. No new task, timing delay,
register write, persistence key, RC bundle or device upload was introduced.

Manual acceptance: check all five controls against x=10 and x=240 at minimum,
mid and maximum positions; verify their touch edges, all five discrete setting
values and timeout Always On, existing settings preview/save, Wi-Fi hold/cancel/
colors, footer visibility and navigation. No changes are staged or committed.


## One-second Wi-Fi hold and quiet label

After successful user hardware acceptance of the shared alignment, the Wi-Fi
ON/OFF threshold was reduced from 2000 to 1000 ms and the normal label suffix
`- HOLD 2s` removed. Normal labels are now `Wi-Fi: ON` / `Wi-Fi: OFF`.
Yellow `KEEP HOLDING...`, authoritative green ON, neutral OFF and transitional/
error feedback remain unchanged. RESET still uses its existing 2000 ms hold and
`HOLD RESET WIFI 2s` label. No geometry, network, persistence or DMX code changed.

Modified files: `src/view_sender.h`, `tests/wifi/test_settings_render.cpp`,
`tests/wifi_power/run_tests.py`, `tests/network/wifi_power_delta.json` and this
report. Tests explicitly verify no action at 999 ms, one action at 1000 ms,
release at the deadline cannot complete a hold, disabled-state ON, repeated
separate holds, cancellation, wraparound, one-shot rejection, quiet labels and
unchanged two-second RESET. Existing alignment and local/offline-domain tests
remain in the same targeted run. Relevant navigation/network/transport/scope/
isolation regressions, all four builds and Git whitespace checks pass.
Evidence is in `dist/settings-wifi-1s-20261031/`. Physical acceptance of the
shorter hold is pending. No firmware upload, staging or commit was performed.


## Button-font verification and RESET WIFI label

Source inspection confirms both Wi-Fi buttons and the Restart/Power Off/Back
buttons use the same `fonts::Font0`. Power Off inherits Font0 from Restart;
Wi-Fi, Restart and Back select it explicitly. No text-size change occurs between
these groups or in the intervening timestamp/battery render helpers. The label
fitting helper truncates strings only and does not scale the font. No font-size
change was therefore needed or introduced.

The sole production change for this follow-up is the normal reset button label
`HOLD RESET WIFI 2s` -> `RESET WIFI`. `KEEP HOLDING...` feedback is preserved,
as are RESET's 2000 ms hold, Wi-Fi's 1000 ms hold, colors, geometry and behavior.
The source-scope fixture, existing font/label rendering assertions and this
report were updated alongside `src/view_sender.h`. No new source files.

Targeted tests record Font0 for the rendered Wi-Fi/reset rows and verify the
side-button font declarations and absence of intervening scaling calls. They
assert `RESET WIFI` and retain hold/geometry/error/scroll checks. Relevant
navigation, network/transport/scope and isolation checks and all four firmware
builds pass. Assets are unchanged. Git whitespace/preservation checks pass.
Evidence: `dist/settings-wifi-font-label-20261031/`. No device upload, staging,
commit, push or release was performed.

## v0.3.0 local closure

This accepted production prerequisite is included in the local v0.3.0 closure.
Historical version/build measurements above remain unchanged as evidence.
Current version is 0.3.0; see [closure record](V0_3_0_LOCAL_CLOSURE.md).
The captured raw-coordinate correction and diagnostic isolation remain intact.
No new touch configuration, NVS operation, upload or electrical validation is
introduced by closure. Suspended THDIFF write experiments stay disabled.
