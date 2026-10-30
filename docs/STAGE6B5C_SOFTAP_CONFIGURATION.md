# Stage 6B.5C — SoftAP Configuration

Baseline: `3730ea421a5a10d6f2be4451df6328e18c1e3aa1`.
Branch: `codex/stage6b-web-ui`. The user accepted Stage 6B.5C functionality,
physical Wi-Fi approval/persistence/recovery, the four-card desktop layout and
iPhone UI. On 2026-10-30, closure validation identified and then verified a narrow
browser lifecycle correction for deferred maintenance navigation. All required
closure checks now pass; the accepted implementation is ready for the authorized
single local commit. Earlier pending-acceptance statements below are historical;
this acceptance and the final closure section supersede them.
No upload, device NVS access, credential change, erase, push, merge or release
was performed. Historical files, prior logs/backups and the stash are preserved.

## Implemented behavior and ownership

The application-task `WifiConfiguration` authority owns exactly one admitted,
immutable proposal. It binds the candidate to the credential revision, request
identifier and live WebSocket generation. Other proposals are busy; stale bases,
invalid strings and unavailable physical confirmation are rejected.

- SSID remains 1–32 printable ASCII bytes. A supplied password remains 8–63
  printable ASCII bytes. Blank password means preserve the existing password;
  it never enables an open AP. No trimming or implicit Unicode conversion occurs.
- Proposal validation and waiting occur in bounded RAM. Neither NVS staging nor
  radio work begins before physical **OK**.
- The shared-canvas physical popup shows the proposed SSID, quoted to delimit
  spaces, and whether the password changes. It never reads/displays a password.
  Ordinary single-press OK/CANCEL hitboxes are x20/y174/130x50 and
  x170/y174/130x50. A press already down when the popup opens cannot act on it.
- CANCEL, a detected initiating-session closure, or 60 seconds without approval
  discards the proposal. The deadline is wrap-safe; OK rechecks the deadline and
  session at the actual press. Expiration never approves. Navigation/rendering
  does not restart the window. There is no web approval command.
- After OK, the worker completes or recovers independently of browser connection.
  Reconnect never replays proposal/cancel mutations. Read-only snapshots restore
  the current state and never confer ownership on a replacement generation.
- Screen ownership, old LCD clip/transfer cleanup, complete six-page restoration,
  speaker deferral and press/release consumption follow the accepted shared-canvas
  approach. No additional framebuffer is allocated. Unchanged popup frames are
  not redrawn continuously.

`Bridge::ownerPoll()` services the authority before command processing. Physical
approval copies one job into the network worker's mutex-protected handoff. HTTP
callbacks validate/copy requests and retire their own sessions; they do not write
NVS, touch the display or mutate Controller state. The network worker alone runs
runtime credential persistence and AP reconfiguration. UI credential strings are
copied to an application-owned cache, rather than exposing worker-mutated buffers.

The popup is available in Sender mode, where the physical Settings UI lives.
Receiver operation is unchanged; configuration is unavailable when its physical
confirmation surface cannot be serviced. Maintenance and Wi-Fi configuration
are serialized, including screen-restoration ownership. No supply/battery/USB
condition was added.

## NVS transaction and interruption policy

Only `dmxwifi` is opened by the new store adapter:

| Key | Format and role |
|---|---|
| `ap1` | Existing 64-byte V1 or 108-byte V2 committed credential blob |
| `ap_try` | New 232-byte versioned undo journal |
| `factory` | Existing U8 physical factory-reset intent, unchanged |

The journal contains a magic/version, original blob length, exact original
credential bytes, complete V2 custom candidate and checksum. The checksum detects
corruption; it is not authentication or encryption. Length, version, provenance
and credential validation precede recovery. No arbitrary record is promoted.

After physical OK:

1. Read the committed `ap1` and write/commit/read back the complete `ap_try`.
2. Publish sanitized applying state. Wait up to one second for owner publication;
   then queue a bounded HTTP-task notice/retirement pump. Notification is best
   effort, without requiring acknowledgments from every client.
3. Retire current sessions in the HTTP task, using generation/epoch guards.
4. Stop/reconfigure/start the existing AP; retain the initialized driver/netif
   and existing HTTP server/registered handlers.
5. Verify full SSID/password readback, channel 6, WPA2-PSK/CCMP, four stations,
   netif up, IP/gateway/mask, DHCP running and router option disabled.
6. Obtain an HTTP-task readiness receipt, then write/commit/read back `ap1`.
7. Erase/commit/verify removal of `ap_try`, the durable transaction commit point.
8. Publish the new committed credentials to the application cache and advance
   the independent credential revision. No Controller reboot is invoked.

Netif down/up waits are each capped at 2.5 seconds; retirement and HTTP readiness
receipts each have a four-second deadline. Waits yield only the network worker.
There is no fixed reconnection delay and no wait for a client to join. SDK flash
and Wi-Fi calls themselves remain synchronous; these software deadlines do not
prove hard real-time bounds on those SDK calls.

The actual worker uses the same `wifi_credentials::execute()` coordinator that
fault-injection tests exercise. A failed application attempts one rollback:
restore/verify exact previous `ap1`, remove the journal, restore and verify the
previous AP/HTTP readiness. No retries loop indefinitely and no failure silently
selects factory credentials. A failed rollback remains explicit and stops the
network worker; physical UI and local recovery remain available.

At boot, a valid surviving journal restores the **previous** exact credential
record and then removes the journal. Thus power loss before durable journal
removal rolls back, including after promotion but before cleanup. After durable
removal, the new committed record is used. NVS errors may have ambiguous durability;
the implementation propagates failure, verifies recovery and does not claim that
an error return proves no flash change. Actual power-interruption testing remains
required. Corrupt/unknown journals fail closed and are preserved for deliberate
physical recovery rather than guessed at or automatically promoted.

Valid V1 records remain unchanged if this feature is unused. An explicit edit
creates V2 with custom provenance, including an SSID-only edit that preserves a
legacy password. It never persists a converted V2 record with legacy provenance.
Factory generation and its AP-MAC verification are unchanged.

The existing held RESET WIFI action still records factory intent for the next
explicit Restart. It invalidates an unapproved proposal and the next boot removes
any Wi-Fi journal before performing the existing verified factory replacement.
It cannot interrupt an approved worker transaction. `dmxpreset`, `dmxmeta`,
`dmxui`, their keys, preset blobs and device-settings formats are unchanged.
Older firmware does not understand `ap_try`: finish/recover/reset the transaction
before an intentional downgrade; do not claim older firmware implements recovery.

## Protocol and browser behavior

The extension is opt-in and retains version 1 and all existing Controller,
Settings and maintenance schemas. Requests are:

| Request | Additional fields |
|---|---|
| `wifi.get` | None |
| `wifi.propose` | `base`: decimal uint64 string; `ssid`: canonical Base64; `password`: canonical Base64 or empty string |
| `wifi.cancel` | `request`: original numeric proposal ID |

Every request retains `v`, shared monotonically increasing `id` and `type`.
Base64 is encoding only; it fits printable quotes/backslashes within the existing
escape-free inbound grammar. Maximum SSID/password plus maximum-width revision
and request ID fits in 227 bytes. The 256-byte inbound limit remains unchanged.
Noncanonical padding, controls/NUL, invalid ASCII, oversized or unknown/duplicate
fields are rejected. The generic 2,560-byte outbound bound remains unchanged.

`wifi.snapshot`, `wifi.update`, and `wifi.result` contain only public metadata:
`revision`, global `seq`, credential `base`, committed `ssid`, `proposed`,
`origin`, `phase`, `reason`, `request`, `remaining_ms`, `password_change` and
`available`. Results add `status`. Snapshots/results answer a request ID; updates
use id zero. Domain phases are idle/approval/applying/failed. Results distinguish
accepted/cancelled/no_change/invalid/conflict/busy/unauthorized/unavailable.
Accepted means waiting for physical approval, not applied or saved.

No password, encoded password, secret hash or credential blob enters an outbound
event, status resource, diagnostic or application log. C++ transient password
fields, worker copies and journals in owned RAM are cleared after use; this does
not claim secure erasure of SDK internals, flash wear-level history or immutable
JavaScript string copies. NVS remains the existing unencrypted storage scheme.

All current UI instances make one opt-in read after synchronization and repeat
only that read on reconnect. Older clients receive no new unsolicited message.
Existing stale-socket detection, silent health checks, complete Controller
snapshots, bounded queues/rates, final-value acknowledgments and no-replay behavior
are preserved. Numeric proposal IDs alone never establish frontend ownership;
ownership is cleared after completion/cancel and on socket generation changes.

The standalone Wi-Fi card is below the existing equal desktop Settings/Maintenance
pair and above separate Browser Preferences. It stacks at full width on mobile.
The card shows current SSID, editable SSID, masked optional password, explicit
browser confirmation, physical approval wait, applying/reconnect state and real
errors. Pending SSID stays visible but is not presented as committed. Password
drafts clear on submission, Back/cancel, interruption or leaving Settings, and
never enter localStorage. Routine successful completion produces no success prose.
The physical Settings local credential readout and Wi-Fi reset gesture are
retained. The follow-up polish removes the provenance/memory captions and labels
the four information rows explicitly; see below. The approval popup has no
plaintext credential readout.

Physical approval is the administrative boundary for this feature. A general
subscription or WPA2 association is not authorization to change credentials.
Existing maintenance authorization and execution are not redesigned. HTTP/WS
remain unencrypted at application level; Base64/physical approval do not provide
end-to-end secrecy against a capable associated attacker. Future STA/LAN access
still needs a separate transport-security/authorization design.

## Network, DMX and diagnostics

No DMX driver/transport/Controller/Dispatcher implementation, publication timing,
pins, blackout/preset semantics, partition table, USB setup, HTTP deadlines,
socket/byte budgets, radio initialization buffers or two-bundle composer changed.
Restart, original USB-connected Power Off and the accepted maintenance countdown
retain their exact execution/rendering methods; only conflicting admission is
coordinated with the new Wi-Fi operation and ephemeral slider-preview cancellation
at successful web maintenance admission (see the follow-up below).

The application continues polling DMX, Dispatcher, network state, battery and
speaker while approval/application UI owns the physical display. No AP/NVS wait
runs in that task or an ISR. Flash operations can still stall caches/other tasks;
placing them in the worker is not proof of zero DMX jitter. The immutable TX
snapshot, qualified completion and Stage 3E clash correction remain intact.

Intentional session retirement has an explicit `wifi_reconfigure` diagnostic
reason. It does not become the first unexpected-error marker. Actual earlier I/O,
send, queue and radio failures are retained. The additional retirement pump is
accounted for, including cancelled stale work. Its epoch is checked both before
and after a potentially blocking send; expired work cannot close a later job's
sessions. Readiness receipts are epoch-qualified as well.

The historical synchronous TCP-send stall remains a limitation. Notification and
HTTP-thread receipts are not proof that every browser received the warning or
that end-to-end HTTP/association succeeds on real hardware.

## Automated validation

- Five Wi-Fi host executables: authority/real Bridge/transaction, extracted actual
  popup, extracted actual SDK radio adapter, actual NVS adapter with SDK fakes,
  and extracted actual HTTP callbacks.
- Boundary, wrap, OK/CANCEL, expiry, wrong generation/base, competing proposals,
  disconnect-before/after approval, blank-password preservation, legacy/custom
  compatibility, interrupted staging/promotion and uncertain durable-write cases.
- Stage/commit/readback/erase/open/type/size failures, real coordinator AP/HTTP
  failure/rollback paths, every adapter SDK failure, netif timeout, and stale
  retirement work resuming after a simulated synchronous send stall.
- Six-page popup restore, clean clipping, speaker deferral, exact touch hitboxes,
  consumed release, unchanged frames and one shared canvas. Host display mocks
  do not establish real rasterization/DMA/panel timing.
- Existing 22 host cases, four maintenance/original-power/render cases, three
  Settings cases, frontend/navigation/live/protocol/loader/status suites, and ten
  pipeline/network/Dispatcher/infrastructure/patch regression checks.
- Seven native Chrome browser suites: header/presets/gestures, device Settings,
  twelve responsive viewports, lower blackout row, maintenance, green Confirm,
  and seven-width Wi-Fi interaction/layout/recovery cases.
- Deterministic generation twice, gzip round trips/hash/SRI manifests, per-asset
  16 KiB checks, four firmware builds and Git whitespace verification.

Historical freeze checks reconstruct only explicitly pinned/reviewed Wi-Fi
changes. Independent current-source checks compare protected power, maintenance,
startup, HTTP, domain and transport methods and exercise new behavior directly.
One historical guard now receives raw source rather than reconstructing an
already reconstructed file. The CRLF fixture normalizes existing line endings
before constructing CRLF, avoiding doubled CR on Windows; malformed inputs remain
rejected. Browser no-replay assertions permit the new read-only `wifi.get`, never
credential or DMX mutation replay.

Validation artifacts and screenshots live under ignored `dist/stage6b5c/`.
Prior historical logs/artifacts were not reused as writable test output folders.

## Pre-polish automated resources

All five Wi-Fi host executables, existing host/frontend/integration suites and
all seven browser suites passed on the final source. Mobile (390 px) and desktop
(1280 px) screenshots were visually inspected. These measurements precede the user-confirmed functional acceptance and the
follow-up polish; current measurements are recorded below.

| Environment | Result | Static RAM bytes | RAM delta vs baseline | Flash bytes | Flash delta vs baseline |
|---|---|---:|---:|---:|---:|
| cores3se_unit | PASS | 95,908 | +6,008 | 1,098,721 | +15,100 |
| cores3se_base | PASS | 95,908 | +6,008 | 1,098,697 | +15,112 |
| cores3se_unit_diagnostics | PASS | 98,620 | +6,008 | 1,103,221 | +15,020 |
| cores3se_base_diagnostics | PASS | 98,620 | +6,008 | 1,103,201 | +15,036 |

| Asset | Gzip bytes | Delta vs baseline |
|---|---:|---:|
| index.html.gz | 4,758 | +281 |
| style.css.gz | 5,057 | +127 |
| core.js.gz | 12,467 | +1,269 |
| ui.js.gz | 12,130 | +940 |

All assets remain below 16,384 bytes. Fixed application state and bounded queue
copies account for the additional static RAM; no new task stack or framebuffer
is added. Dynamic SDK/NVS allocations, free heap, fragmentation and task
watermarks require hardware measurement. Both 6 MiB application slots and the
16 MiB layout are unchanged.

Final compiled source fingerprint:
`8ff3e5af830f4807cafbf1687cfe40b6d22a17947fcba7edbdeb89f6128d39ae`.
Final asset bundle fingerprint:
`dd76339ccf46e6ea4bb91d3fa227f7e8c4ad398fced66a587a96bf426aca656b`.
All four receipts and each bootloader/table/app0/firmware component hash match.
Explicit regeneration twice is byte-identical to checked-in generated outputs;
the final browser mirror is byte-for-byte identical to the frontend and tests.

## Exact modified/added file inventory

The following 56 files are the complete Stage 6B.5C work. The 211 existing
unrelated historical/document files match their preserved hashes; the existing
stash is unchanged. HEAD/branch are unchanged, with an empty staging area.
No application changes were made during final reporting.

```text
docs/STAGE6B5C_SOFTAP_CONFIGURATION.md
include/network_service.h
include/websocket_bridge.h
include/websocket_diagnostics.h
include/websocket_service.h
include/wifi_configuration.h
include/wifi_credential_transaction.h
include/wifi_wire.h
src/network_service.cpp
src/src.cpp
src/view_sender.h
src/websocket_bridge.cpp
src/websocket_protocol.cpp
src/websocket_service.cpp
src/wifi_credential_store.h
tests/dispatcher/check_integration.py
tests/infrastructure/test_infrastructure.py
tests/maintenance/check_integration.py
tests/network/check_integration.py
tests/network/check_softap_integration.py
tests/network/check_status_telemetry.py
tests/network/check_websocket_integration.py
tests/network/maintenance_guard_support.py
tests/network/wifi_guard_support.py
tests/web/check_scope.py
tests/web/run_tests.mjs
tests/web/test_blackout_status_browser.cjs
tests/web/test_device_settings.mjs
tests/web/test_device_settings_browser.cjs
tests/web/test_header_browser.cjs
tests/web/test_live.mjs
tests/web/test_maintenance_browser.cjs
tests/web/test_responsive_layout_browser.cjs
tests/web/test_wifi.mjs
tests/web/test_wifi_browser.cjs
tests/wifi/check_integration.py
tests/wifi/fake/nvs.h
tests/wifi/run_tests.py
tests/wifi/test_callbacks.cpp
tests/wifi/test_configuration.cpp
tests/wifi/test_radio.cpp
tests/wifi/test_render.cpp
tests/wifi/test_store.cpp
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

## Physical acceptance checklist and remaining limitations

1. Unit and Base: boot normally without using configuration. Verify ordinary
   DMX, UI, Settings, presets, blackout, receiver separation and Wi-Fi clients.
2. Test SSID-only changes with a blank password and a supplied new password.
   Confirm the physical quoted SSID and change flag; no popup password display.
3. Confirm no AP/NVS change while waiting. Test fresh OK/CANCEL presses, held
   opening touches, outside hitboxes, 60-second expiration and session closure.
4. Repeat from Main, Settings, Precision, Keypad and both preset pages; include
   tones, display sleep/wake and cancellation near the deadline. Check clean
   restoration, no click-through, preserved drafts/selection/STEP and one canvas.
5. Test simultaneous clients, competing/stale requests, cancel then a foreign
   proposal with a reused numeric ID, disconnect after OK and reconnect without
   mutation replay. Verify latest Controller state after the fresh snapshot.
6. Rejoin the new SSID/password on iPhone Safari/Home Screen, Windows and macOS.
   A changed password for the same SSID may require updating/forgetting the OS
   saved network. Check four-station capacity, HTTP/status, WebSocket recovery,
   DHCP router suppression and iPhone cellular routing.
7. Verify V1/V2 persistence and factory Wi-Fi reset without losing presets,
   brightness, timeout or volume. Use the same held reset plus explicit Restart.
8. On a controlled test device, exercise power cuts before/after journal write,
   promotion and cleanup, plus storage/radio failure recovery. Do not infer real
   NVS crash behavior solely from host mocks. Avoid downgrade during a journaled
   transaction; recover/reset first.
9. Check heap/minimum/largest internal/DMA block, worker/HTTP stack watermarks,
   PSRAM and long multi-client operation. Static build sizes are not runtime RAM
   measurements. SDK call duration still needs actual hardware measurement.
10. Externally capture RS-485 packet length/coherence, break/MAB and frame gaps
    during approval, persistence, AP restart and rollback. Software logs and
    zero simulated TX errors do not establish electrical integrity.

No STA/AP+STA, Home Assistant, full Controller reboot, new partition or broader
authentication framework is implemented. Wait for the follow-up visual acceptance
before staging or committing.


## Final web and physical Settings UI polish — 2026-10-30

The user has accepted the functional Stage 6B.5C Wi-Fi configuration flow on the
CoreS3SE. This follow-up implements only the requested Settings presentation and
maintenance/slider interaction refinements. No upload or device NVS access was
performed in this pass. The physical appearance and iPhone interaction changes
remain pending user acceptance.

### Maintenance while adjusting settings

Restart and Power Off availability no longer depends on a local slider gesture
or a pending settings message. The click handlers follow the same rule; this is
not merely enabled styling. Existing offline/unsupported/actual maintenance
conflicts remain guarded. Existing browser confirmation and the five-second
physical CANCEL window are unchanged.

Confirm stores one bounded, page-local maintenance intent. It finishes the local
slider through the existing paced preview/final commit path, waits for its
matching result, then submits maintenance using the original serialized transport
admission checks. A pending preview is not a saved setting; commit success remains
explicit. Rejection, failed persistence, unavailable state, Back, navigation or
connection loss discard the deferred maintenance intent. Nothing is replayed on
reconnect. No arbitrary wait, new protocol or persistence retry is added.

On the application owner, real physical persistence remains an admission conflict.
An ephemeral remote preview alone does not block maintenance. Only after a web
maintenance request is accepted does Bridge cancel any remaining remote preview,
restore its entry value/persisted flag and broadcast the new settings revision.
Cancellation uses the existing DeviceSettings cancellation implementation and
writes no NVS. A commit processed first remains saved; a stale peer commit after
cancellation is rejected. Requests rejected for another conflict do not cancel a
preview. No execution/countdown/power method changed.

### Layout and password field

At the existing >=680 px breakpoint the Settings/Maintenance grid stretches its
cards to equal outer heights. Maintenance uses `align-content: start`, so rows
remain top-aligned instead of stretching their internal gaps. Below the breakpoint
the cards retain natural stacked heights. No Wi-Fi or Preferences card dimensions
were changed.

The New password input has an inline SVG eye/eye-off button in a 44 px-wide target,
with reserved input padding. Password starts masked. Keyboard activation keeps
button focus; pointer activation from the input keeps its focus/caret/selection.
The accessible label and pressed state describe the local visibility toggle.
Draft clearing, page exit, lost synchronization and Wi-Fi application remask it.
Blank still preserves the existing password. Visibility is never stored or sent;
the previous stored password is never fetched into the input. No new asset or
package dependency was introduced.

### Physical Settings rows and reset geometry

The dynamic status/client/error row is followed by `SSID:`, `PASSWORD:` and `IP:`.
The provenance, heap/history and `WIFI ONLY` captions have been removed. Maximum
32-byte SSIDs and 63-byte passwords are copied to bounded display-only buffers
and fitted with the current canvas font's measured width to <=240 px, adding
`...` when needed. Real credentials are unchanged. No extra framebuffer or
transmission-time allocation is added.

Information rows stay at content Y=316/334/352/370. Reset is now Y=394, x=24,
216x28; its draw position and touch region share `settings_wifi_reset_y`.
The next-boot notice is Y=436. Content height is 462 (maximum scroll 222), so all
four rows, reset button and notice can be seen together at the bottom. The reset
still requires a continuous two-second hold, cancels on movement/outside touch,
executes once per hold and applies on a later explicit restart. Changed network
state invalidates the Settings canvas while any of this section is visible.
The existing off-screen clear, speaker deferral, single-canvas handoff and fixed
power/back controls are preserved.

### Follow-up automated validation

- Frontend factory/navigation/live-state/settings/maintenance/Wi-Fi/Status suites
  pass, including existing 83 live-state and 12 asynchronous telemetry groups.
- Eight Chromium browser suites pass on shipped gzip assets; the focused polish
  suite covers widths 320/375/390/430/600/768/1280, all three sliders/both actions,
  delayed preview and commit acknowledgments, duplicate Confirm, persistence
  failure, deferred-action cancellation, password mask/focus/caret/keyboard/touch,
  reconnect remasking and desktop/mobile geometry. Synthetic browser sessions
  never contact the device.
- The 22 existing compiled host cases and Settings/preview/adapter, maintenance,
  original Unit/Base power and physical-canvas suites pass.
- Seven Wi-Fi host executables pass: unchanged proposal/journal/NVS/radio/callback
  checks plus actual Bridge maintenance/preview ordering and actual extracted
  physical Settings rows/reset. Tests cover long credentials, no stored-value
  mutation, two-second boundaries, movement cancellation and redraw. Font/pixel
  mocks cannot establish real LCD quality.
- Infrastructure, scope, protocol, station diagnostics, HTTP/WebSocket, Dispatcher,
  memory/startup, driver patch/state and fingerprint integration checks pass.
- Generation repeated twice is byte-identical, all four receipts match current
  source/assets, four 16 MiB builds pass and Git whitespace checks pass.

| Environment | Result | Static RAM bytes | RAM delta vs pre-polish | Flash bytes | Flash delta vs pre-polish |
|---|---|---:|---:|---:|---:|
| cores3se_unit | PASS | 95,908 | 0 | 1,099,357 | +636 |
| cores3se_base | PASS | 95,908 | 0 | 1,099,333 | +636 |
| cores3se_unit_diagnostics | PASS | 98,620 | 0 | 1,103,837 | +616 |
| cores3se_base_diagnostics | PASS | 98,620 | 0 | 1,103,809 | +608 |

| Asset | Gzip bytes | Delta vs pre-polish |
|---|---:|---:|
| index.html.gz | 4,888 | +130 |
| style.css.gz | 5,174 | +117 |
| core.js.gz | 12,467 | 0 |
| ui.js.gz | 12,454 | +324 |

All are below 16,384 bytes. Static RAM is unchanged; temporary bounded text fitting
uses stack space, not a persistent framebuffer. Runtime heap/stack watermarks,
electrical DMX integrity and real Safari/LCD appearance are not inferred from
host tests. Artifacts are in ignored `dist/stage6b5c-polish/`; prior artifacts,
211 unrelated historical files, the existing stash and HEAD are preserved.

### Final visual/interaction acceptance

1. On iPhone Safari and Home Screen, hold/rapidly adjust each slider, then request
   each maintenance action. Check confirmation, final setting value/persistence,
   the physical five-second CANCEL, cancellation and normal reconnect.
2. With a second browser, hold an uncommitted preview while requesting maintenance
   from the first. Check deterministic restoration and synchronization, no hidden
   save; check a completed commit remains saved.
3. Check equal desktop card edges with top-aligned Maintenance; natural mobile
   heights and unchanged separate Wi-Fi/Preferences widths/heights.
4. Type a synthetic password, toggle with touch/keyboard, check caret/text/mask and
   remasking on Back/navigation/reconnect. Submit a blank password only in an
   explicitly approved functional test; automated tests never alter device NVS.
5. On physical Settings, scroll through short/max-length credential readouts:
   check labels, readable fitting, no overlap/flicker/stale rows, fixed controls,
   HOLD RESET WIFI and next-boot notice. Avoid actually resetting credentials
   unless that functional reset is intentionally part of the hardware test.

### Exact follow-up file inventory

This list is relative to the preserved pre-polish uncommitted tree, not a new
commit or a replacement for the original Stage 6B.5C inventory above.

```text
docs/STAGE6B5C_SOFTAP_CONFIGURATION.md
include/device_settings.h
src/src.cpp
src/view_sender.h
src/websocket_bridge.cpp
tests/network/check_softap_integration.py
tests/network/wifi_guard_support.py
tests/web/test_header_browser.cjs
tests/web/test_maintenance_browser.cjs
tests/web/test_wifi_polish_browser.cjs
tests/wifi/check_integration.py
tests/wifi/run_tests.py
tests/wifi/test_settings_maintenance.cpp
tests/wifi/test_settings_render.cpp
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/generated/ui.js.gz
web/src/index.html
web/src/js/settings.js
web/src/style.css
web/src/ui.js
```


## Final desktop Settings card layout refinement

The user accepted Stage 6B.5C functionality and the preceding web/physical polish.
This final follow-up changes layout only; it needs visual acceptance before any
staging/commit. No JavaScript factory, firmware source/header, physical display,
network flow or persistence behavior changed.

Settings now contains two transparent, independent grids:

- Row 1: Device Settings / Device Maintenance.
- Row 2: Wi-Fi Configuration / Browser Preferences.

Both grids use the existing >=680 px two-column breakpoint, 16 px gaps and desktop
stretch behavior. Each of the four children retains its own `panel` background,
border, radius and padding. The second grid has no panel styling, background,
border or padding. Each row balances heights separately; content stays top-aligned
and there is no forced equal height across both rows. Wi-Fi inputs retain their
full available card width and original eye-icon layout. The existing 600 px form
cap is retained (it does not constrain a desktop half-width card).

Below 680 px, both grids have one column and preserve the accepted order and
natural card heights. The old Wi-Fi bottom margin is replaced by the same 16 px
grid gap; the lower grid has no trailing margin. This preserves mobile card,
input, footer, scroll and navigation geometry exactly against the accepted gzip
assets at widths 320/375/390/430/600/679 px. No viewport, safe-area or scrolling
rule changed.

The new `test_settings_card_layout_browser.cjs` verifies widths
320/375/390/430/600/679/680/768/1024/1280/1440/1920. It checks four independent
panels, a transparent lower grid, separate borders/backgrounds, two equal-width
and equal-height desktop rows with top-aligned content, 16 px gaps, full input
widths, eye bounds, no horizontal overflow and browser-only preference/eye
interaction. Mobile geometry is compared directly with preserved pre-change
assets. Existing Wi-Fi, maintenance and responsive test assertions now recognize
the second transparent grid rather than requiring Preferences to be a direct
child or full-width desktop block. Existing navigation/live, settings/Wi-Fi/
maintenance, blackout/header browser regressions, asset/scope/network integration
checks and all four builds pass. Deterministic generation repeated twice is
byte-identical; all build receipts match current sources and assets.

| Build | Result | Static RAM bytes | Flash bytes | Flash delta vs preceding polish |
|---|---|---:|---:|---:|
| cores3se_unit | PASS | 95,908 | 1,099,405 | +48 |
| cores3se_base | PASS | 95,908 | 1,099,381 | +48 |
| cores3se_unit_diagnostics | PASS | 98,620 | 1,103,885 | +48 |
| cores3se_base_diagnostics | PASS | 98,620 | 1,103,857 | +48 |

Static RAM is unchanged. Gzip sizes: HTML 4,906 (+18), CSS 5,199 (+25), Core
12,467 (unchanged), UI 12,454 (unchanged) bytes. All remain below 16,384 bytes.
Git whitespace checks pass. Tests and screenshots use synthetic credentials and
sessions, with no device or NVS access. Screenshots were inspected for distinct
cards and balanced inputs; automated Chromium checks do not replace the user's
final desktop/iPhone visual acceptance.

Exact files changed in this follow-up (prior uncommitted work is preserved):

```text
docs/STAGE6B5C_SOFTAP_CONFIGURATION.md
tests/web/test_maintenance_browser.cjs
tests/web/test_responsive_layout_browser.cjs
tests/web/test_settings_card_layout_browser.cjs
tests/web/test_wifi_browser.cjs
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/index.html
web/src/style.css
```

Artifacts and the preservation snapshot are in ignored `dist/stage6b5c-layout/`.
No staging, commit, push, merge, tag, firmware upload or NVS erase was performed.
The existing stash, historical files and unrelated work remain intact. Final
acceptance: inspect both desktop rows, input/eye spacing and Preferences at 50%;
confirm the unchanged four-card mobile stack on iPhone Safari/Home Screen.


## Deferred maintenance navigation correction and final closure — 2026-10-30

### Root cause and synchronous safety boundary

The accepted browser implementation could hide Settings immediately while
`app.js` deferred `settingsView.setActive(false)` until its next animation frame.
If an outstanding setting preview/commit result arrived in that interval,
`settleMaintenance()` could send the previously confirmed Restart/Power Off.
Leaving and returning before a render could skip deactivation entirely.

The fix is browser-only:

- The existing shell navigation callback now invokes Settings `setActive()`
  synchronously from actual document/view visibility, then schedules the usual
  batched visual render. A view exit clears confirmation/deferred intent and
  cancels/drains an uncommitted slider gesture before any subsequent ACK. Same-tab
  navigation remains unchanged. No nav DOM replacement or rendering delay is added.
- Confirmation invalidation increments an interaction generation. A confirmed
  intent captures its action and generation. Immediately before deferred dispatch,
  Settings verifies the generation, active Settings ownership, real view/document
  visibility, synchronized Sender state, idle/nonpending maintenance, absence of
  settings errors and completion of the local setting/commit. Invalid intents
  clear immediately; only valid intents wait for existing single-flight settings.
- Dispatch consumes the local intent once. Navigation after a command is already
  sent does not send a cancellation or reinterpret its acknowledgment. The
  original server acceptance and physical five-second CANCEL semantics remain.

In both observed orderings, leaving Settings synchronously invalidates the intent.
ACK before the next frame drains cancellation; ACK after cleanup sees the same
invalid intent. Leave/return and rapid tab changes cannot recreate the old token.
When ACK completes before navigation on an active view, the final setting commit
and one maintenance request proceed normally. Late old-generation messages cannot
replay an intent after disconnect or background/resume.

No firmware Restart/Power Off, physical display/countdown, Wi-Fi transaction,
protocol, NVS, DMX, preset, blackout or Settings layout implementation changed in
this fix. The accepted 61-file Stage 6B.5C set is retained; the lifecycle changes
add `web/src/js/app.js` and the focused browser regression to the inventory.
The historical source guard has explicit reviewed pins for this small delta;
actual production assets are exercised by the tests.

### Deterministic validation

`tests/web/test_maintenance_navigation_browser.cjs` runs 132 controlled cases:
Restart/Power Off x three sliders x pending preview/commit x eleven event
scenarios. Navigation and ACKs execute within one JavaScript task/microtasks for
the before-RAF case. The after-cleanup case awaits exactly a render frame;
no arbitrary delay is used to conceal the race. Actual outbound commands and
persistence/cancellation ordering are asserted, not only button appearance.

Cases include pre-ACK navigation, ACK after hiding/before RAF, after-cleanup ACK,
valid ACK-before-navigation dispatch, immediate return, repeated rapid tab changes,
Back, disconnect/reconnect, background/resume, a peer maintenance conflict,
hidden-view dispatch rejection and multiple preview/final-commit sequences.
No duplicate maintenance, silent cancellation of an accepted command, replay,
stuck controls or orphaned confirmation is permitted. Accepted commits remain
saved once; cancelled unsaved previews add no persistence writes.

The regression failed on the preserved vulnerable assets with one unexpected
Restart. It passed on the corrected assets, then the expanded 132-case matrix
passed three consecutive complete runs (396 cases). The formerly intermittent
Wi-Fi polish test also now sends navigation and the late ACK in the same task,
asserting outbound traffic before any animation frame.

Final results:

- Full frontend factory/navigation/live/settings/maintenance/Wi-Fi/Status suites:
  PASS, including 83 live integration and 12 asynchronous telemetry groups.
- All ten browser suites: PASS (nine established suites plus the repeated focused
  race suite). Prior desktop/mobile visual geometry, password toggle, Settings
  preview/commit, Wi-Fi approval projection and blackout/header behavior pass.
- 22 existing freshly compiled core host cases: PASS, including Controller,
  Dispatcher, real installed ISR/transport, HTTP/WebSocket and memory/station paths.
- Settings/preview/actual adapter, maintenance/shared canvas/original Unit/Base
  Power Off, seven Wi-Fi authority/journal/radio/NVS/callback/render/ownership
  executables: PASS. No physical stored presets were accessed by tests.
- Ten protocol/network/scope/infrastructure/asset/driver-patch regression commands:
  PASS. Asset generation repeated twice is byte-identical. Current source/asset
  receipts match all four builds; all gzip assets remain below 16 KiB.
- Working-tree and intended-file whitespace checks: PASS. Exact staged inventory,
  staged asset fingerprints and staged whitespace are checked before committing.

| Environment | Result | Static RAM bytes | Flash bytes | Flash delta vs accepted layout |
|---|---|---:|---:|---:|
| cores3se_unit | PASS | 95,908 | 1,099,581 | +176 |
| cores3se_base | PASS | 95,908 | 1,099,541 | +160 |
| cores3se_unit_diagnostics | PASS | 98,620 | 1,104,045 | +160 |
| cores3se_base_diagnostics | PASS | 98,620 | 1,104,017 | +160 |

Static RAM is unchanged. Gzip bytes: HTML 4,907 (+1), CSS 5,199 (unchanged), Core
12,467 (unchanged), UI 12,624 (+170). No new asset/dependency/task was introduced.
These are automated browser/host results, not a new physical flash or hardware
acceptance claim for the narrow correction.

### Closure inventory and preserved scope

The exact single-commit candidate contains these 63 paths:

```text
docs/STAGE6B5C_SOFTAP_CONFIGURATION.md
include/device_settings.h
include/network_service.h
include/websocket_bridge.h
include/websocket_diagnostics.h
include/websocket_service.h
include/wifi_configuration.h
include/wifi_credential_transaction.h
include/wifi_wire.h
src/network_service.cpp
src/src.cpp
src/view_sender.h
src/websocket_bridge.cpp
src/websocket_protocol.cpp
src/websocket_service.cpp
src/wifi_credential_store.h
tests/dispatcher/check_integration.py
tests/infrastructure/test_infrastructure.py
tests/maintenance/check_integration.py
tests/network/check_integration.py
tests/network/check_softap_integration.py
tests/network/check_status_telemetry.py
tests/network/check_websocket_integration.py
tests/network/maintenance_guard_support.py
tests/network/wifi_guard_support.py
tests/web/check_scope.py
tests/web/run_tests.mjs
tests/web/test_blackout_status_browser.cjs
tests/web/test_device_settings.mjs
tests/web/test_device_settings_browser.cjs
tests/web/test_header_browser.cjs
tests/web/test_live.mjs
tests/web/test_maintenance_browser.cjs
tests/web/test_maintenance_navigation_browser.cjs
tests/web/test_responsive_layout_browser.cjs
tests/web/test_settings_card_layout_browser.cjs
tests/web/test_wifi.mjs
tests/web/test_wifi_browser.cjs
tests/web/test_wifi_polish_browser.cjs
tests/wifi/check_integration.py
tests/wifi/fake/nvs.h
tests/wifi/run_tests.py
tests/wifi/test_callbacks.cpp
tests/wifi/test_configuration.cpp
tests/wifi/test_radio.cpp
tests/wifi/test_render.cpp
tests/wifi/test_settings_maintenance.cpp
tests/wifi/test_settings_render.cpp
tests/wifi/test_store.cpp
web/generated/core.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/generated/ui.js.gz
web/src/core.js
web/src/index.html
web/src/js/app.js
web/src/js/commands.js
web/src/js/protocol.js
web/src/js/settings.js
web/src/js/transport.js
web/src/style.css
web/src/ui.js
```

New closure logs and source snapshots are in ignored `dist/stage6b5c-final/`.
The completed migration audit at
`dist/stage6b5c-closure/V0_3_0_RELEASE_MIGRATION_AUDIT.md` is preserved byte-for-byte
as the historical audit; its identified navigation blocker is resolved by this
follow-up. Its migration/version/hardware gates remain separate. No packaging,
partition, migration, version-label, Wi-Fi-toggle or release change was made.
Historical development files, logs/backups and the existing stash remain intact.
No push, merge, tag, release, upload, hardware access or NVS erase is performed.
