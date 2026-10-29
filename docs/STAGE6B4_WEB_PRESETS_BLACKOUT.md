# Stage 6B.4 — Web Presets and Blackout

Baseline closure: 2026-10-29. Hardware acceptance: **PASSED**, confirmed by the user on the real CoreS3SE and current iPhone. The sections below retain chronological implementation history; their earlier pending/verification statements are superseded by the final acceptance section at the end.

## Resume baseline and scope

- Branch: `codex/stage6b-web-ui`.
- HEAD: `5b3cea7a5b2906a77b6a8f1c7dd66fded7d9e8e9`,
  `Stage 6B.3: Add active web DMX channel control`.
- Parent: `3eb72f31c61c53ae1617a3c7385d1327c884b5c8`.
- The interrupted run had already implemented preset controls, confirmations,
  blackout, operation tracking, gesture invalidation, focused Bridge observation
  fixes, tests and generated assets. It had not created this report.
- The first resumption reviewed and validated that implementation without further production edits, creating this report. That evidence remains in ignored `dist/stage6b4-resume-20261028/`. The subsequently requested loaded-preset/double-tap additions and their fresh validation are documented below, with logs in `dist/stage6b4-loaded-20261028/`.
- The index remained empty. No staging, commit, fetch/pull, reset, stash operation,
  upload, NVS operation on hardware, merge, push, tag or release was performed.
- `stash@{0}` remains
  `996a2f9378208540b0fd3b544e7d9908affe8e84`. Historical development files and
  `docs/STAGE3_NO_RECEIVER_REGRESSION.md` are unrelated untracked work and preserved.

The user has confirmed extensive original Stage 6B.4 physical testing with iPhone, Windows and MacBook: presets, blackout, synchronization and recovery work. The final loaded-preset and double-tap additions below are new and have not yet been tested on the physical device. No hardware was accessed during this automated review.

## Implemented behavior and ownership

The browser has a read-only authoritative state projection plus disposable input
previews. `actions.js` coordinates one pending preset/blackout operation and one
last result; it owns neither a preset store nor another DMX universe.

Flow: browser control → allowlisted encoder → existing native WebSocket endpoint
→ bounded Bridge inbox → application-task `CommandDispatcher` → `DmxController`
→ existing persistence/output publisher → ordered state publication and result.
Physical controls continue through their existing Dispatcher paths. HTTP/WS
callbacks do not mutate Controller or NVS. Dispatcher, preset adapter, DMX driver/patches, scheduling, power and battery logic, credentials, partition layout and firmware packaging are unchanged from Stage 6B.3. Controller additions are transient provenance only; physical UI additions are restricted to preset tile labels and double-tap loading.

### Presets P1–P8

| Authoritative slot code | Presentation | Load | Save | Delete |
| --- | --- | --- | --- | --- |
| 0 | Empty | Disabled | Immediate explicit click | Disabled |
| 1 | Occupied | Immediate explicit click | Confirm overwrite of named slot | Confirm delete of named slot |
| 2 | Corrupt | Disabled | Confirm replacement of named slot | Disabled |
| 3 | Storage error | Disabled | Disabled | Disabled |

All mutations also require a synchronized Sender connection, available command
budget, and no pending operation. Unsynchronized slots are Unknown. Selection
alone does not send a mutation. The UI has Load, Save and Delete; the latter maps
to existing `preset.clear`. Copy and rename are not introduced by this stage.

Confirmation is an inline group with explicit slot-specific text and Confirm /
Cancel controls. It is invalidated by slot selection, leaving Presets, connection
loss/resynchronization, authoritative bulk/blackout boundaries or changed slot
state. The send path rechecks consent synchronously. Focus uses `preventScroll`
and returns to the selected slot after Confirm/Cancel; no blocking native dialog
is used. Pending, failed and uncertain outcomes are shown separately from actual
slot occupancy.

- **Load:** replaces all working channel values from a valid occupied slot. The
  existing output publication path is retained. Invalid/empty/corrupt slots and
  receiver mode are rejected by native validation too.
- **Save:** captures current Controller working values at application time, not
  a browser preview or an output-zero substitute. Identical valid contents are
  a no-change success without a redundant NVS write. Saving does not change live
  channel values or trigger an output publication.
- **Delete:** clears logical occupancy through the existing cleared mask. It
  does not zero live channels, change blackout or erase the retained legacy blob.
  Corrupt-slot deletion remains unsupported by the existing Controller.

### Blackout

The header sends an explicit `blackout.set` boolean based on the latest
authoritative flag. It never sends 512 zero-valued channel commands or invents
local restore values. The active styling and `aria-pressed` follow Controller
state; pending target and unknown/error outcomes are distinct.

Existing domain semantics are preserved exactly:

1. OFF → ON captures the working universe as the restore-entry snapshot.
2. DMX output remains zero while ON; displayed working values remain meaningful.
3. Channel edits and preset loads during ON change working values only. Save
   during ON captures those working values, not the zero output packet.
4. ON → OFF restores the pre-entry snapshot, discarding working edits/loads made
   during blackout. Repeated ON does not recapture it; repeated same-state
   requests are no-change operations.

One operation at a time and a 350 ms disabled interval suppress accidental local
blackout bursts. There is no queue of delayed toggles and no coalescing of already
sent ON/OFF commands. Ordered transport blackout barriers remain unchanged.

## Existing WebSocket contract

No native command decoder, protocol version or server command schema was changed. The state projection gains the required `loaded` and `modified` fields described below. The
browser newly uses these existing v1 requests:

```json
{"v":1,"id":7,"type":"preset.load","slot":1}
{"v":1,"id":8,"type":"preset.save","slot":8}
{"v":1,"id":9,"type":"preset.clear","slot":8}
{"v":1,"id":10,"type":"blackout.set","active":true}
```

`id` is a positive, increasing uint32 shared with channel commands and read-only
`state.request` within a socket generation. Slots are integer 1–8 on the wire
(zero-based internally); `active` must be a JSON boolean. Existing rejection of
unknown/duplicate/extra fields, invalid types, bounds and message size remains.
The frontend writer exposes only channel.set, these four operations and the
read-only snapshot request. It does not expose arbitrary Controller commands.

Results use the existing envelopes, for example:

```json
{"v":1,"type":"command.ack","id":8,"revision":"300","seq":"920","status":"applied","domain":0}
{"v":1,"type":"command.error","id":9,"revision":"300","seq":"921","status":"storage_error","domain":10}
```

These are illustrative complete result envelopes, not captured device logs.
`domain` is the existing numeric `DmxController::Result`; in these examples 0 is
`ok` and 10 is `metadata_failed`. Other outcomes include `no_change`, `busy`,
`unsupported_mode`, `rejected` and `transport_error`, with optional native reason.
An acknowledgment confirms application/no-change, not electrical DMX delivery.

Initial readiness still requires hello plus a complete valid `state.snapshot`.
Snapshots and bulk `state.update` carry all 512 working values, eight preset codes, blackout, Sender state, and loaded metadata. Channel and mode updates also carry loaded metadata so an edit immediately marks the correct preset Modified on every client.
`revision` and `seq` are decimal uint64 strings; IDs are numbers. Revision
describes domain changes, while sequence orders publications; sequence gaps and
equal-revision bulk updates are valid. Acknowledgments do not fabricate state.

### Narrow Bridge publication corrections

The original Stage 6B.4 firmware changes are the following focused Bridge corrections. The final usability additions also touch the Controller, Event/encoder and physical preset tiles as documented below:

1. Successful preset load publishes the existing bulk update even if values are
   unchanged. This supplies a newer sequence boundary to invalidate old remote
   gestures; it does not fabricate a Controller revision or increment the
   revision-change diagnostic counter.
2. Preset persistence errors publish observed working/slot state before the error
   result. Partial persistence is possible; the result remains an error.
3. A slot-query storage error is not cached forever. A subsequent snapshot can
   retry that read. If refreshed occupancy differs, ready peers receive a bulk
   update before the requesting client's higher-sequence snapshot. Healthy
   cached metadata remains reusable. There is no continuous NVS poller.

The regression scope check reconstructs these exact reviewed transformations
from Stage 6B.3 and compares the whole Bridge source. The approved provenance/tile inputs now have exact reviewed hash exceptions plus functional tests. All other protected transport, storage, physical UI and configuration inputs remain frozen. Fixed native inbox/outbox capacities and thread boundaries are unchanged.

## Pending commands, conflicts and connection loss

- `control.js` drops unsent fader intents and advances a gesture epoch before
  sending a local preset/blackout operation. Old held range events and numeric
  drafts cannot later overwrite its result; a fresh deliberate gesture is needed.
- The same invalidation occurs on remote full-state boundaries (including no-op
  loads), blackout flag changes and a resync discovering a changed revision.
  Bulk shape does not identify command type, so invalidation is conservative.
- Already transmitted/admitted channel writes cannot be recalled. They retain
  server ordering. Another client's later valid command may legitimately follow
  a load or blackout; this stage does not introduce distributed locking.
- Channel and preset/blackout traffic share the existing 18-mutation rolling
  one-second browser budget. Active fader cadence remains 60 ms. Exhausted action
  credit reports not sent, then re-enables controls; it never queues that intent
  for automatic retry.
- The browser tracks at most one channel request, one operation and one snapshot
  request concurrently, within the server's advertised four-outstanding limit.
  Operations have an independent bounded 5 s acknowledgment deadline. Competing
  actions/faders are disabled while an operation remains outstanding.
- Matching applied/no-change results must be covered by authoritative state.
  Missing publication, timeout or connection loss retires/resynchronizes the
  generation. Uncertain mutations are never replayed. A fresh complete snapshot
  is required before new deliberate edits.
- Storage/transport errors may have changed state. The UI keeps the failed result
  and requests read-only resynchronization. Neither an occupied slot nor a fresh
  snapshot proves which contents an unacknowledged Save wrote; uncertainty is
  retained rather than converted into success.
- Existing 15 s idle liveness probes, 10 s probe deadline, automatic recovery,
  manual Retry and background/foreground handling remain. Lifecycle resync does
  not admit a second operation while an earlier descriptor is still outstanding.

Confirmations use the latest observed slot/boundary, not a native compare-and-swap
token. Concurrent requests are serialized by application order; the last applied
Save can overwrite a slot another client has just saved. A post-send concurrent
change cannot be prevented by the local confirmation UI. No schema redesign or
new persistence policy is implied.

## NVS compatibility

The adapter is byte-for-byte unchanged from the committed baseline. Controller storage operations and byte format remain compatible; only transient load provenance is added. Existing `dmxpreset/p1` … `p8` entries remain 513-byte packet blobs
(start code plus 512 channels), and `dmxmeta/cleared` remains a uint8 bit mask.
Load forces start code zero. Logical clears retain the blob; later Save writes
the packet and clears its mask bit. Missing keys remain normal; wrong lengths,
type errors and genuine storage failures remain distinguishable.

Blob and metadata writes are separate existing operations, not an atomic
multi-key transaction. A write failure can leave persistent data changed without
the complete logical operation succeeding. Native errors and browser uncertainty
are preserved; there is no automatic migration, erase or retry.

Host cases use in-memory storage with read/write/metadata failures. They establish
Controller behavior and format-preservation/source checks, not actual flash
power-loss durability. Automated validation did not connect to or modify device
presets, credentials or settings.

## Validation results

Final logs for the usability additions: `dist/stage6b4-loaded-20261028/` (ignored local evidence). The earlier resource baseline remains in `dist/stage6b4-resume-20261028/`.

| Check | Result |
| --- | --- |
| `node tests/web/run_tests.mjs` | PASS: navigation/focus/scroll, factory/preferences/banks, 78 live integration groups |
| Fresh C++11 `-Wall -Wextra -Werror` host cases | PASS: all 21 compiled suites |
| `tests/web/check_pipeline.py` | PASS: 12 cases plus exact firmware scope guard |
| SoftAP, WebSocket, canvas-memory, startup-trace and network integration guards | PASS |
| Dispatcher/transport integration guards | PASS |
| `tests/infrastructure/test_infrastructure.py` | PASS: 6 cases |
| `tests/transport/test_patch.py` with preserved pristine v2.02 copy | PASS: fresh/repeated application and invalid-state rejection |
| Two explicit `web_asset_pipeline.py --generate` runs | PASS: composed JS, manifest and all gzip outputs byte-identical to reviewed inputs |
| `git diff --check` | PASS |

Host suites cover Controller bounds/start-code/blackout/preset metadata;
Dispatcher admission/publication; native WS request validation, generation and
reply ordering; actual patched ISR fault injection, safe retirement and ordered
blackout; HTTP assets/policy; SoftAP/DHCP/station handling; framebuffer allocation
and diagnostic defaults. Stage 6B.4 cases cover P1/P8, empty/occupied/corrupt/error
states, same-value saves/loads, explicit confirmations, canceled consent, partial
storage failures, metadata recovery publication, multiple clients, outstanding
commands across disconnect, no mutation replay, old gestures/drafts, shared
credits and ordered channel/preset/blackout transitions.

Frontend tests execute real factories with mock DOM, sockets and timers. Network
tests use SDK/socket/RTOS/storage substitutes or source integration checks.
Neither proves real Safari gestures, flash durability, RF behavior, RS-485
electrical correctness, break/MAB timing or wire packet coherence.

### Original Stage 6B.4 resource baseline (before usability additions)

Compared with the Stage 6B.3 closure measurements for commit `5b3cea7`:

| Environment | Build | Static RAM, bytes | RAM delta | Flash, bytes | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,508 | 0 | 1,062,897 | +2,900 |
| `cores3se_base` | PASS | 87,508 | 0 | 1,062,865 | +2,900 |
| `cores3se_unit_diagnostics` | PASS | 90,220 | 0 | 1,067,429 | +2,900 |
| `cores3se_base_diagnostics` | PASS | 90,220 | 0 | 1,067,397 | +2,896 |

Build receipts identify the reviewed source digest and asset bundle. Layout is
still `cores3se-16m-ota6-v1`; no packaging or partition migration was performed.
These are PlatformIO linked RAM/flash figures, not measured runtime heap. No new
driver buffer, task, queue capacity or authoritative universe is allocated.
The metadata comparison adds an eight-byte owner-task stack array, not another
large Event. Browser operation/confirmation state is bounded. Runtime browser
heap, device heap/PSRAM, network and touch latency have not been freshly measured.

### Original Stage 6B.4 assets (before usability additions)

| Asset | Raw bytes | Gzip bytes | Gzip delta vs 6B.3 | Headroom to 16,384 bytes |
| --- | ---: | ---: | ---: | ---: |
| `index.html.gz` | 10,257 | 2,886 | +75 | 13,498 |
| `style.css.gz` | 14,251 | 3,945 | +87 | 12,439 |
| `app.js.gz` | 58,833 | 14,609 | +2,505 | 1,775 |

All three are strictly below 16 KiB. Total gzip: 21,440 bytes (+2,667).
Bundle SHA-256:
`89c8987cb3a5e67f0305d00f0ffdc74743b405d810ccbc3de7ebcda1fea0ae65`.
Existing content hashes, manifest verification and HTTP/CSP/cache rules remain.


## Final usability additions: authoritative loaded preset and double-tap

### Controller provenance, revisions and blackout

`DmxController::loadedPreset()` returns 0 for unknown or 1–8 for the loaded slot.
`loadedPresetModified()` is a conservative dirty-history flag, not an expensive
continuous equality check against NVS. Both current and blackout-entry metadata
are RAM-only; reboot/reset starts Unknown. No NVS key or write is added to retain
this UI state, and no preset is inferred by comparing channel values.

- A successful native load sets its slot and clears Modified. A different identity
  or a cleared dirty flag advances Controller revision even when all values were
  already equal. Loading the same unmodified slot again is no-change; the existing
  full-state load publication still provides a newer sequence boundary.
- A successful value-changing channel/adjust/patch operation marks the known
  identity Modified in O(1); invalid or no-change edits do not. Returning a value
  to its old number does not claim an exact match or clear the dirty history.
- Selection is still UI-local. Selecting P5 cannot remove the P3 Loaded/Modified
  badge. Loading another slot replaces it in actual Dispatcher application order.
- Save never creates a Loaded identity. Saving another slot preserves the known
  identity. A byte-identical Save is still a no-op. Replacing the known slot
  invalidates that identity (and any saved blackout-entry identity for that slot).
  Delete invalidates it too. After a potentially partial failed replacement or
  clear, identity is conservatively invalidated; the operation still reports its
  real storage error. Metadata-only changes publish without changing DMX values.
- Blackout ON captures provenance with the existing working-value snapshot; mere
  output suppression leaves it intact. Edits/load while ON describe the current
  working state, while output remains zero. OFF restores the entry values **and
  their provenance**, discarding intervening working edits/loads. Deleting or
  replacing a slot also invalidates a saved entry identity, so it cannot reappear
  falsely after restoration. An Unknown entry returns to Unknown on OFF.
- A domain load may succeed while the existing transport publication reports an
  error. Provenance records the domain state; the transport error is not hidden
  and the badge is not proof of a physically delivered DMX frame.

`CommandDispatcher` and DMX publication rules are unchanged. Metadata updates run
on the application task and use the existing observer/completion paths. No new
512-channel comparisons, NVS queries, dynamic allocation or networking calls are
introduced into the per-channel mutation path.

### State schema addition

Every `state.snapshot` and `state.update` (bulk, channel or mode) now includes:

```json
{"loaded":3,"modified":false}
```

These are additional fields of the existing full envelope, not a separate wire
message. `loaded` is an integer 0–8; `modified` is boolean and must be false when
`loaded` is 0. Initial snapshots require both fields. The browser validates them
before applying any state and exposes them only while synchronized. Reconnect
snapshots restore the actual Controller identity; the browser never derives it
from its local selection, an acknowledgment or occupied-slot matching.

The Event copies contain two small metadata fields; queue capacities and existing
request/result schemas remain unchanged. Required state fields mean the new UI
must be served with the matching firmware. Old strict-schema cached UI versions
are not a compatibility target: reload the app after an authorized firmware update;
content-hashed assets and the existing HTML cache policy supply the matching UI.
The read-only local fixture was updated to explicitly publish Unknown provenance.

### Compact presentation

Web tiles append Loaded or Modified to their existing slot-state text and get a
restrained blue left accent. Selection retains its separate outline/aria-pressed.
Physical tiles retain their geometry, white selection borders and green occupancy
square, with a small cyan LOADED or MODIFIED label inside the associated tile.
No permanent banner, slider geometry or other physical control is changed.

### Release-qualified double-tap gestures

Both implementations recognize two completed taps on the same tile, with no
blocking delay or queued mutation. Tap duration is at most 350 ms, inter-tap gap
at most 350 ms, and movement tolerance 12 pixels on each axis (including between
taps). A successful pair is consumed; a third tap starts a new pair. Load is
attempted only through the existing occupied-slot load path and Dispatcher.

Web recognition is scoped to preset tiles: pointer down/move/up/cancel validate
movement and release, then click performs the ordinary selection and, for a valid
pair, the existing guarded `ask("preset.load")` operation. No native dblclick
listener is installed, so it cannot duplicate the operation. Canceled/dragged or
long presses, intervening tiles/actions, page leave, state boundaries and loss of
readiness invalidate the candidate. Keyboard activation still selects; explicit
Load remains available and is also the fallback on browsers without Pointer Events.
No global gesture cancellation, zoom limits or touch preventDefault is added;
the existing scoped `touch-action: manipulation` retains pinch zoom.

Physical recognition uses `PresetDoubleTap`, a fixed-size header-only state
machine fed the installed M5Unified 0.2.21 touch detail. `wasPressed()` begins and
`wasClicked()` release qualifies a tap; movement, drag/hold release and other
controls reject the pair. COPY/options/page transitions reset it. The selected
slot branch loads once and draws once, retaining the explicit LOAD button and
existing long-hold Save/Clear protections. No per-frame allocation or extra NVS
read is used to recognize taps; occupancy is checked only before an actual load.

### Focused automated coverage and remaining uncertainty

New frontend groups cover selected versus loaded styling, Modified/blackout
projections, reconnect identity, strict metadata schema, one pair/one command,
triple taps, explicit Load/keyboard fallback, empty/offline tiles, move/cancel/hold,
intervening controls and stale candidates. The live total is 78 groups.
Controller cases cover equal-value identity changes, sticky Modified, save/delete,
partial-failure invalidation, reboot and blackout provenance restoration.
The actual physical gesture core is host-tested for releases, holds, movement,
other controls, reset, triple taps and uint32 clock wrap. Native Bridge tests use
two clients, ordered competing loads, local edits, encoded snapshot metadata,
reconnect, blackout restoration and deletion.

Automated gesture tests do not emulate real Safari synthesis or the physical
M5Unified sampling/coordinates. These additions still require iPhone Safari,
standalone Home Screen and CoreS3SE physical acceptance.

An initial build was correctly rejected by the existing source-fingerprint gate
because the physical draw/load path was refined during compilation. A concurrent
host asset-table compile also encountered the transient absent generated file.
Those initial logs are preserved; the final validations below run against frozen
production inputs and completed generated assets.

### Final usability-addition validation and resources

All four builds below passed against frozen production inputs; their source and
asset receipts match the final tree. All 21 freshly compiled host suites passed,
including the new physical gesture core and provenance cases. Frontend navigation
and 78 live integration groups passed, as did all nine Python regression commands
(12 asset-pipeline cases, six infrastructure cases, network/Dispatcher scope and
integration checks, and deterministic driver patch-state checks). Two final
explicit asset generations were byte-identical. Git whitespace checks passed.
No hardware was accessed and no device NVS was modified.

Deltas are against the hardware-accepted original Stage 6B.4 resource baseline
above (not against Stage 6B.3):

| Environment | Result | Static RAM | RAM delta | Flash | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | +288 B | 1,064,377 B | +1,480 B |
| `cores3se_base` | PASS | 87,796 B | +288 B | 1,064,349 B | +1,484 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | +296 B | 1,068,937 B | +1,508 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | +296 B | 1,068,905 B | +1,508 B |

Relative to the committed Stage 6B.3 baseline, total Stage 6B.4 flash growth is
4,380 / 4,384 / 4,408 / 4,404 bytes respectively; RAM growth is the same as above.
The static increase includes Event padding across existing fixed queue entries
and bounded physical gesture state. No queue capacity, task or framebuffer is
added. Runtime heap/PSRAM and touch latency still require physical measurement.

| Final asset | Raw bytes | Gzip bytes | Gzip delta vs original 6B.4 | Headroom to 16 KiB |
| --- | ---: | ---: | ---: | ---: |
| HTML | 10,257 | 2,883 | -3 | 13,501 |
| CSS | 14,387 | 3,977 | +32 | 12,407 |
| JavaScript | 61,052 | 15,169 | +560 | 1,215 |

Total gzip: 22,029 bytes (+589). All three assets remain strictly below 16,384
bytes. The HTML source is unchanged by the usability additions; injected
content-hashed CSS/JS URLs account for its generated-byte differences.
Final bundle SHA-256:
`b76d315b8eb00b46cd4b3632dbb251560b6d92c8a8a9802b79de007b11c058d6`.

### Additional physical acceptance

The user-approved original Stage 6B.4 tests remain accepted. For these additions:

1. Load known safe P3 on iPhone; verify all browsers and the physical tile show P3
   Loaded. Select P5 in each UI without loading: P3 must remain marked.
2. Make a safe value-changing physical or web edit: all clients show P3 Modified.
   Return the number to its original value: the conservative Modified flag stays.
3. Double-tap occupied P5 on the physical tiles, then on iPhone Safari/Home Screen
   and desktop. Verify one operation per pair and all clients switch identity.
   Check single selection, triple taps, alternating tiles, empty tiles, scrolling,
   dragging, holds, keyboard selection and the explicit Load button.
4. Use only an approved disposable slot for overwrite/Delete. Other-slot Save
   must not invent a load. Identical Save preserves identity; replacing/deleting
   the known slot invalidates it. Check long-hold physical Save/Clear still works.
5. Verify blackout remains independent: load/edit during ON, confirm fixtures stay
   dark, then OFF restores the entry values and provenance. Delete/overwrite of an
   entry slot must not resurrect its badge. Also test an Unknown entry.
6. Reconnect and resume with the correct identity; pair taps while disconnected
   must not replay after connection recovery. With two clients loading different
   safe looks, the last actual Controller application must win on every screen.
7. Verify existing pending-fader invalidation, physical and web blackout, DMX
   continuity, speaker/battery/sleep, touchscreen selection outlines and preset
   labels. Do not sweep unknown fixture channels or erase stored data.

## Complete uncommitted Stage 6B.4 inventory

33 tracked files modified against Stage 6B.3:

```text
include/websocket_bridge.h
scripts/web_ui_bundle.py
src/dmx_controller.cpp
src/dmx_controller.h
src/network_service.cpp
src/view_sender.h
src/websocket_bridge.cpp
src/websocket_protocol.cpp
tests/controller/test_controller.cpp
tests/network/check_integration.py
tests/network/check_websocket_integration.py
tests/network/test_websocket.cpp
tests/web/check_pipeline.py
tests/web/check_scope.py
tests/web/fixture_server.py
tests/web/run_tests.mjs
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/commands.js
web/src/js/control.js
web/src/js/fader.js
web/src/js/presets.js
web/src/js/protocol.js
web/src/js/state.js
web/src/js/status.js
web/src/js/transport.js
web/src/style.css
```

Seven intended new files:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
include/preset_double_tap.h
tests/network/preset_loaded_guard_support.py
tests/network/check_status_telemetry.py
tests/web/test_status.mjs
tests/web/test_header_browser.cjs
web/src/js/actions.js
```

During this final usability request, the following existing files changed:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
include/websocket_bridge.h
src/dmx_controller.cpp
src/dmx_controller.h
src/view_sender.h
src/websocket_bridge.cpp
src/websocket_protocol.cpp
tests/controller/test_controller.cpp
tests/network/check_integration.py
tests/network/check_websocket_integration.py
tests/network/test_websocket.cpp
tests/web/check_scope.py
tests/web/fixture_server.py
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/js/app.js
web/src/js/presets.js
web/src/js/protocol.js
web/src/js/state.js
web/src/style.css
```

New during this request: `include/preset_double_tap.h` and
`tests/network/preset_loaded_guard_support.py`. The actions factory and this report
already existed at entry and remain untracked. Historical development files, the
unrelated Stage 3 regression report, ignored logs/backups and the existing stash
are preserved. The index is empty; nothing has been committed or staged.


## Status page completion — read-only telemetry (2026-10-28)

This increment remains inside the uncommitted Stage 6B.4 review. Existing presets,
loaded/modified provenance, double-taps, blackout and multi-client control are
preserved. The Status page is observational; it does not send Controller commands,
access NVS or change connection health based on HTTP success or historical errors.
No device upload or new physical acceptance is claimed.

### Actual sources and mappings

| Display | Actual source | Interpretation |
| --- | --- | --- |
| Connection / synchronization | Existing frontend transport phase and authoritative `ready` | Same connection-label function as the main header; readiness requires a complete valid WS snapshot |
| Revision / sequence | Authoritative WS state | Decimal uint64 strings preserved without Number conversion |
| Sender / blackout | Authoritative WS flags | Current working-domain state; distinct from TX fail-stop |
| SoftAP IP, stations, station limit | `/status`: `ip`, `stations`, `max_stations` | Existing native station reconciliation; Wi-Fi association count is not WS clients |
| Active WS clients | `/status`: `websocket.clients` | Existing active Bridge-client count, not HTTP connections |
| DHCP default-router advertised | `/status`: `dhcp_router_option` | Boolean; No means the router option is suppressed |
| TX errors / fail-stop | `/status`: `tx_errors`, `tx_stopped` | Existing application-owner transport counters; fail-stop displayed as a separate latch |
| Maximum owner-loop gap | `/status`: `max_owner_gap_us` | Historical software owner-service gap in microseconds; not measured DMX frame timing |
| Network phase / component stage | `/status`: `phase`, `stage` | READY/etc and the existing network component marker 5; **not** a claim that firmware development is still Stage 5 |
| Firmware source ID | New `/status`: `build` | Existing immutable `web_assets::bundle().source_sha256`, displayed as first 12 hex digits; full 64 digits remain in JSON |
| Hardware profile / flash layout | New `/status`: `profile`, `layout` | Existing immutable bundle metadata; no new configuration or build-pipeline step |
| Device uptime | New `/status`: `ms` | Existing request-time `esp_timer_get_time()` converted to uint32 milliseconds |
| Memory sample uptime | `/status`: fixed `samples` entry with `event == 92`, field `ms` | Worker current sample, normally refreshed at the existing 1 Hz maintenance; never substitute the last ring entry or event 90/91 |
| Free / historical minimum / largest internal 8-bit heap | Event 92: `internal`, `minimum`, `largest` | Existing internal + 8-bit capability queries; minimum is historical since boot |
| Free DMA heap | `/status`: `dma_free` | Actual free internal DMA-capable heap measured during that HTTP request |
| Largest DMA block | Event 92: `dma` | **Largest block**, not free DMA heap; existing internal + DMA query |
| Free PSRAM | Event 92: `psram` | Existing `MALLOC_CAP_SPIRAM` free-size query; no invented total |
| Worker / HTTP minimum free stack | `/status`: `worker_stack_hwm`, `http_stack_hwm` | Installed ESP-IDF FreeRTOS API returns bytes, unlike upstream word-based variants |
| WS opened / closed, sent updates, outbox/high-water, queue-full, send failures, Wi-Fi disconnects | `/ws-diagnostics`: matching scalar keys | Cumulative counters, apart from current `outbox`; exact integers |
| Commands accepted / applied-or-no-change / rejected, current inbox, slow closes | `/status`: `websocket` metrics | Existing Bridge counters; applied includes no-change acknowledgments |
| HTTP rejections | `/status`: `rejected` | Existing cumulative native rejection counter |
| First failure / last closure, time, rc and errno | `/ws-diagnostics`: `first.closure.reason`, `last_close.*` | Historical context, not a permanent current-offline indicator; explicit null first failure means None recorded |
| Browser socket, close/clean, retry/classification, message age, received/rendered revision, background and probe state | Existing frontend `health` | Retained local diagnostics; no HTTP request is needed for these |

Memory/stack values are displayed in KiB or MiB with one decimal; raw bytes remain
in the endpoints. Counters and device/sample millisecond timestamps remain exact.
Device millisecond counters wrap as uint32 after about 49.7 days; browser freshness
uses its own monotonic receipt clock, not subtraction of those device counters.

### Deterministic identity and unavailable information

The normal endpoint had no reliable release-version field. The Stage 0 diagnostic
banner still has its historical hard-coded version and compile date; it is not
reused or changed here. The existing generated source digest provides a genuine
deterministic build-input identifier, including the pending source/assets, without
introducing timestamps, Git commands or new build-pipeline behavior. It is a source
fingerprint, not a semver/tag or electrical validation claim.

PSRAM total/initialization status, battery data, CPU load and detailed completed
DMX-frame/slot/break/MAB measurements are not exposed by these endpoints. No values
are guessed from previous hardware measurements. Raw server diagnostics still
provide more detailed rejection buckets and session data than this compact page.
No credentials, station MACs or private logs are added to responses.

The only native production change extends the existing `/status` JSON prefix with
`build`, `profile`, `layout` and request uptime `ms`. Exact source-reconstruction
checks permit only that change and its matching argument list. Tests interpret the
actual C format strings and construct a maximum-width response: 2,634 bytes, below
the unchanged 4,096-byte formatter buffer. Existing radio settings, HTTP limits,
security/cache headers, task cadence, WS bridge/protocol, Controller and DMX sources
are unchanged by this increment.

### Polling, cleanup and health presentation

- Entering visible Status starts asynchronous same-origin GET `/status`, then GET
  `/ws-diagnostics`. No other route or mutation is fetched. Responses use no-store
  caching and redirect rejection; default same-origin credential behavior remains.
- One cycle/request at a time per browser. Next cycle is scheduled 10 seconds after
  completion; slow/failing cycles do not accumulate or catch up in a burst.
- Each request, including body reading, has a 3.5-second deadline. AbortController
  cancels the request; a separately rejected deadline promise prevents a hanging
  fetch/body from holding the poller indefinitely. JSON is bounded to 4,096 decoded
  characters from the existing 4,096-byte native response and validated before use.
- Tab exit, visibility loss and pagehide cancel the timer and active request.
  Generations reject late callbacks. The poller also checks actual document/tab
  visibility before each request, covering the short interval before a queued
  navigation render updates its active flag. Re-entry/resume starts a fresh cycle.
- Each endpoint retains its own data, receipt time and validity. An endpoint failure
  cannot erase the other's successful result. Missing/invalid individual fields
  show Unavailable; wrong root schema/invalid JSON fails that endpoint.
- Old values are explicitly suffixed stale after failure/pause or 20 seconds without
  a successful refresh. Each endpoint has its own update-age/status line. The
  device/sample uptime fields identify when memory was actually sampled; HTTP
  receipt time is not presented as a fresh heap measurement.
- Historical counters remain visually secondary in a native collapsed details
  section. The page never derives current WS health from past errors or HTTP data.
  LIVE/synchronized remain stable while the existing routine probe is pending;
  the advanced probe field can say Yes. A genuine WS timeout immediately uses the
  existing stale/reconnecting path. No health-check timing or transport change.
- Telemetry stays in memory only. Rendering updates existing text only when changed;
  it does not rebuild navigation, focus headings, change scrolling or restart WS.

Three Status clients produce six GETs per successful cycle, with at most three
simultaneous HTTP requests in the simulated budget test. Three active WS sessions
plus those requests fit the existing six native client-session slots. The global
12-request/second guard and Connection: close are unchanged. Concurrent asset loads,
four clients or a slow native TCP send can still cause admission failure or delay;
the UI marks data stale and retries conservatively, rather than increasing limits.
These are software-budget checks, not a physical three-client performance claim.
The historical synchronous native send-stall limitation remains.

### Automated validation and resource impact

Logs: ignored `dist/stage6b4-status-20261028/`.

- Frontend navigation, 78 live integration groups and nine asynchronous Status
  groups passed. Cases cover success, individual failures, partial/invalid/oversize
  JSON, body/fetch timeout, cancellation, stale data, resume generation isolation,
  actual hidden-tab checks, three-client budgets, quiet probes, WS failure/recovery
  and the unchanged preset/blackout mutation tests.
- All 21 freshly compiled C++ host suites passed, including Controller/Dispatcher,
  physical preset gestures, transport/ISR faults, WS, HTTP, SoftAP and memory guards.
- Ten Python regression commands passed: 12 pipeline cases, six infrastructure
  cases, native Status format/scope checks, existing network/integration guards and
  reproducible driver patch-state tests.
- Two explicit asset generations were byte-identical. Four builds passed with
  matching final source/asset receipts. Git whitespace checks passed.

Deltas below are against the immediately preceding loaded-preset/double-tap build:

| Environment | Result | Static RAM | RAM delta | Flash | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 0 | 1,066,733 B | +2,356 B |
| `cores3se_base` | PASS | 87,796 B | 0 | 1,066,725 B | +2,376 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 0 | 1,071,293 B | +2,356 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 0 | 1,071,261 B | +2,356 B |

| Asset | Raw bytes | Gzip bytes | Gzip delta |
| --- | ---: | ---: | ---: |
| HTML | 14,903 | 3,886 | +1,003 |
| CSS | 14,545 | 4,002 | +25 |
| JavaScript | 65,146 | 16,364 | +1,195 |

All remain strictly below 16 KiB. JavaScript has **only 20 bytes of headroom**.
The initial implementation hit the existing cap; reusing the field renderer for
local and server diagnostics and removing duplicate/default code made it fit.
No limit, gzip settings, asset architecture or safety behavior was changed. Future
frontend additions require an explicit budget/asset plan before implementation.
Total gzip is 24,252 bytes (+2,223). Bundle SHA-256:
`8481295d216f18ddb3f30deeb48b88fc1ef192e073259beb183768aa1af84d3c`.
Runtime heap/PSRAM with three active Status pages and native network latency still
need physical measurement; static build figures do not establish those properties.

### Exact Status increment file inventory

14 existing files modified:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
src/network_service.cpp
tests/web/check_pipeline.py
tests/web/run_tests.mjs
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/status.js
web/src/style.css
```

Two new test files:

```text
tests/network/check_status_telemetry.py
tests/web/test_status.mjs
```

### Physical verification checklist

1. Open Status on real Unit firmware with iPhone Safari/Home Screen, Windows and
   MacBook. Verify IP, distinct station/WS counts, suppressed DHCP router option,
   READY phase, current profile, source ID and sensible timestamped memory values.
   Compare the displayed source ID with that environment's infrastructure receipt.
2. Leave an idle connection visible for multiple 15-second WS health checks. Primary
   connection and synchronization must remain LIVE; only advanced probe-pending
   may change. Actual Wi-Fi loss must show stale/reconnecting, followed by a fresh
   authoritative snapshot after recovery.
3. Check about-10-second refreshes and independent freshness labels. Switch tabs,
   background/foreground the iPhone and rapidly navigate near a refresh boundary;
   verify hidden pages stop HTTP polling and resume without duplicate cycles.
4. Run three clients with Status open while safely changing channels/loading an
   approved preset/using blackout from physical UI and other clients. Check WS
   synchronization, native queue/send/rejection counters, real fixture operation
   and physical responsiveness. Do not sweep unknown fixture channels or erase
   presets. Repeat Base separately with its existing experimental status.
5. If one HTTP endpoint is unavailable or throttled, its values must become stale
   while unrelated telemetry and live WS state remain useful. Historical failures
   must not leave the primary indicator permanently offline. Inspect advanced
   counters without clearing genuine failures to improve appearances.
6. Verify phone/landscape/desktop internal scrolling, keyboard focus, native details
   expansion and copying diagnostic text. Observe runtime memory with three clients.
   Zero software TX errors alone does not prove electrical packet integrity.


## Compact single-row mobile header (2026-10-28)

Superseded by the consolidated polish below after user review; retained as validation history.

This is a markup/CSS-only increment within the pending Stage 6B.4 review. No
JavaScript, native firmware source, WS protocol, Controller/DMX logic, polling,
preset or blackout behavior changed. The JavaScript bundle is byte-identical to
entry, retaining its 20-byte gzip headroom.

Below 480 px the old wrapping header is replaced by one grid row: flexible compact
branding plus a fixed 170 px action area (72 px connection / 6 px gap / 92 px
blackout). The decorative subtitle is hidden, logo becomes 20 px and title 13 px.
The header is 68 px high without additional top safe inset; the 390 px Chromium
baseline was 151 px, reclaiming 83 px. Top padding still respects the top safe
area, and header side padding now respects left/right insets. The `100svh` frame,
internal content scroller and persistent bottom navigation are unchanged.

Healthy mobile state shows the blue dot and a short Live span, marked aria-hidden.
The original full connection text remains in a visually clipped span and names
the `role=status` wrapper through aria-labelledby. Non-live states show their full
text in the same fixed slot. Desktop keeps its original full label and subtitle.
No health-check logic or label generation changes: routine probes stay LIVE and
actual reconnect/error states remain visible and accessible.

Blackout keeps its existing button, dynamic complete text, disabled/pending state,
aria-busy, aria-pressed, descriptive label and command handler. Its mobile target
is fixed at 92 × 44 px, with enough room for two-line pending/unavailable captions.
No clipping/ellipsis obscures its state, and fixed slots prevent status changes
from moving it. Pinch zoom remains enabled; no gesture handler or global touch
restriction is added.

Validation:

- Offline headless Chrome, with actual decompressed generated assets, synthetic
  WS data and intercepted local HTTP only: 320/375/390/430 px, all four sections,
  seven non-live labels, LIVE, pending and ON/OFF, actual touch emulation, full
  accessible names, no overlap/clipping/horizontal overflow and persistent nav.
- Header and navigation geometry at 600/768/1280 px compared against the preserved
  entry assets: unchanged with zero safe insets. Screenshots were visually reviewed.
- 78 live groups, nine async Status groups, navigation/factory tests, 13 pipeline
  cases, six infrastructure cases and the existing ten regression commands pass.
  Relevant HTTP and embedded-asset C++ host tests pass with the final asset table.
- Two explicit generations are byte-identical; all four builds and whitespace
  checks pass. Native/controller/gesture source files and the existing stash are
  byte-preserved. Browser tests do not establish real Safari toolbar/safe-area or
  Home Screen behavior; those remain physical acceptance checks.

The optional browser test uses an already-installed Playwright/Chromium selected
with `DMX_PLAYWRIGHT_MODULE` / `DMX_CHROME_PATH`; it adds no firmware or production
frontend dependency. Run `node tests/web/test_header_browser.cjs --output dist/NEW`
with optional `--baseline dist/ENTRY/before` for pre-change geometry comparison.
Local evidence is retained in ignored `dist/stage6b4-header-20261028/`.

| Environment | Result | Static RAM | Flash | Flash delta vs Status increment |
| --- | --- | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,066,941 B | +208 B |
| `cores3se_base` | PASS | 87,796 B | 1,066,933 B | +208 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,071,517 B | +224 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,485 B | +224 B |

Static RAM is unchanged. Final gzip: HTML 3,909 B (+23), CSS 4,200 B (+198), JS
16,364 B (unchanged). All remain below 16 KiB. No new asset architecture or limit.

Changed by this increment:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
tests/web/check_pipeline.py
web/src/index.html
web/src/style.css
web/generated/index.html.gz
web/generated/style.css.gz
web/generated/manifest.json
```

New: `tests/web/test_header_browser.cjs`. The earlier Stage 6B.4 modifications,
logs/historical files and stash are preserved; no staging, commit or upload.

Before acceptance, verify the actual iPhone in Safari and Home Screen mode:
portrait/landscape safe areas, stable navigation and scrolling through all four
views, readable full error/reconnecting states, quiet routine health checks and
blackout OFF/ON/pending/unknown feedback. Confirm pinch zoom and reliable taps on
the 44 px target. No device was flashed during this change.


## Consolidated Web UI polish — pending physical acceptance (2026-10-28)

The user reports the existing channel/preset/blackout, Loaded/Modified, double-tap,
recovery and telemetry functionality working on iPhone, MacBook and Windows.
This focused increment addresses their presentation/touch feedback. It remains
uncommitted on `codex/stage6b-web-ui`, HEAD `5b3cea7`, with an empty index.
No native source, physical slider, NVS, Wi-Fi, HTTP telemetry poller or WS transport
module changed. Existing Settings appearance work was inspected and byte-preserved;
no new Settings functionality was added.

### Preset selection analysis and correction

Logical selection was already exclusive: each selection handler sets aria-pressed
on exactly one tile and clears all others. Loaded/Modified is a separate
Controller-derived state and can legitimately identify a different tile.
There is no evidence of two actual logical selections in the supplied screenshots.

Two concrete source problems could produce the observed experience:

1. The click handler selected a tile unconditionally even after pointer movement,
   cancellation or an invalid release had cleared its gesture state. The prior
   tests asserted no unwanted Load but did not assert selection stayed unchanged.
2. Global button:hover background/border styling applied without hover/pointer
   capability gating, including preset cards. Sticky touch hover can look like a
   second selection. Its specificity could also override the real selected fill.

Without on-device DOM/pointer traces, the exact contribution of sticky Safari hover
versus synthetic clicks to a particular screenshot remains uncertain; both source
issues are addressed, without assuming iOS itself has a counting/state defect.

The existing recognizer now remembers whether Pointer Events are in use and rejects
pointer-originated clicks lacking a valid release **before** changing selection.
Mouse/touch clicks, including touch PointerEvent clicks with detail zero, require
that release. Keyboard/assistive activation and the explicit Load button remain.
The existing 12 px movement and bounded tap/pair timing, cancellation, occupied-slot
checks, pending-action safeguards, no-replay behavior and one-load-per-pair path
are retained. Non-Pointer-Events fallback selection remains possible. No global
preventDefault or pointer capture is added on tiles, so ordinary page scrolling
and pinch zoom are preserved. Physical double-tap code is unchanged.

Hover decoration now runs only for fine pointers that support hover. Preset cards
are excluded from the generic hover fill; their unselected mouse hover changes
only a subdued border. Real Selected fill/outline and the independent Loaded or
Modified label/accent retain different meanings. Keyboard focus stays visible.

### Native web fader acquisition and scrolling

The previous input had a 44 px surface, a 32 px thumb and no touch-action rule to
exclude browser panning. That allows touch arbitration to scroll/cancel while the
user tries to acquire the narrow native slider. This is a web input issue, not a
change to physical CoreS3SE slider sampling or mapping.

Only the actual `.fader input[type=range]` surface now has touch-action:none.
On widths below 680 px its hit width is min(56 px, available column width), and the
thumb is 52 px where available, shrinking on exceptionally narrow columns. At
320 px, each of four columns has about 59.5 px available; a 56 px input and 52 px
thumb fit without overlap. Both WebKit and Firefox thumb rules are included;
WebKit offset is centered on the unchanged six-pixel track. Thumb height, track,
colors, native value mapping and vertical travel are unchanged.

Column/gap/number/step areas do not inherit the range's restriction and remain
scrollable; pinch zoom is retained outside the input. Native range pointer/cancel
handlers, final-value publication, 60 ms pacing/coalescing, acknowledgments and
Controller synchronization are unchanged. Headless touch tests move the native
range and verify both a delivered final value and an unchanged content scroll
position, then drag outside it and verify scrolling without channel commands.
Real iPhone native-range behavior still requires physical acceptance.

### Header, footer and Editor detail

The single-row compact header was rejected by user review and is superseded.
Below 480 px, original 33 px logo, 19 px title, subtitle and original 24/22 px
padding are restored. Branding and a 100 × 44 px blackout button share row one.
The full connection message occupies an 18 px second row with four-pixel spacing,
spanning the width so words never split. Healthy state is Connected / Live;
there is no short Live substitute or CSS-clipped connection text. The full
accessible status name remains. The header is 112 px at zero safe insets, with
stable geometry across pending/offline/reconnecting states. Side/top safe-area
handling remains. Desktop/tablet header geometry at 600/768/1280 px is unchanged.

Footer parts are consecutive, left-aligned on one line with compact 16 px top
padding, retaining the existing `Web control · 6B.4` span. No dynamic version
binding exists in this footer; the existing current-stage identifier is retained,
not replaced by the unrelated native network-component stage 5. Bottom-navigation
clearance and the 100svh/internal-scroll architecture are unchanged.

Only the redundant normal-case Controller working values detail is removed.
Blackout state, pending confirmations, successful-operation feedback and errors
still use their existing text paths. Shared clock/timer bindings in app bootstrap
replace repeated equivalent bindings to keep the bundle compact; their function
bodies/deadlines and all consumers' behavior are unchanged. An explanatory
preferences comment was removed, without changing the optional-storage catch.
No dependencies, asset architecture or compression limits were changed.

### Status structure and client-count consistency review

Connection & Network remains the primary full-width card. DMX Output is a natural-
height full-width card rather than an equal-height neighbor of the taller System
card. System & Memory is split into System & Firmware and Memory & Resources,
with two internal columns at desktop widths and one column on mobile. All existing
telemetry field/source/unit declarations were retained and compared in browser
tests. Major cards have 16 px gaps (20 px internal desktop System gap). Advanced
Diagnostics remains closed by default, full-width, in normal flow with a 16 px top
margin. No fixed height, negative margin or positioning workaround is used.

The reported zero-server-clients / local-LIVE discrepancy cannot be diagnosed
uniquely from a screenshot. Source tracing establishes:

- `/status.websocket.clients` is the lock-protected active Bridge counter copied
  by the HTTP task; open/drop lifecycle updates that count. It is not the Wi-Fi
  station count and is not inferred from browser readiness.
- The local indicator describes the last valid authoritative WS state and browser
  lifecycle/health checks. A server-side close may precede the browser event or
  probe timeout. Independently sampled/delayed HTTP data can also precede a newer
  WS handshake/snapshot while arriving later at the UI.
- No current counter/mapping defect was found. Exact incident attribution needs
  correlated `/status.ms`, browser receipt/message timestamps and WS diagnostic
  uptime/session-generation/closure data. Timing consistency is not proof of one
  particular cause, and the known native synchronous send-stall limitation remains.

The label now explicitly says Active WebSocket clients (HTTP sample). Reported
zeros are displayed faithfully; a regression case keeps zero visible alongside a
valid local LIVE state, without inventing a minimum count or forcing a false
connection state. Existing HTTP freshness/timestamps, polling, WS recovery and
health-check logic are byte-preserved. Historical incidents still do not become
permanent current-offline indicators.

### Validation, resources and exact increment inventory

Local evidence: ignored `dist/stage6b4-polish-20261028/`.

- Frontend: navigation/factories/preferences, 80 live groups and 10 async Status
  groups PASS. New assertions cover unchanged single selection after moved,
  cancelled or cross-tile gestures and valid/cancelled zero-detail touch clicks.
- Headless Chromium touch emulation PASS at 320/375/390/430 px across all four
  views: original branding bounds, seven unbroken status labels, blackout
  pending/ON/OFF, preset vertical scrolling without selection/load, exact pair
  loading, empty slot, hover distinction, range-only dragging, outside scrolling,
  single-line footer and navigation clearance. Desktop/tablet at 600/768/1280 px
  retains header geometry; all telemetry declarations remain, cards have clear
  spacing, and diagnostics expands/collapses without clipping. Screenshots reviewed.
- All 21 compiled C++ host suites PASS, including Controller, Dispatcher, presets,
  blackout, patched ISR/transport, WS/HTTP and SoftAP/memory. Existing ten Python
  integration/regression commands PASS (13 pipeline and six infrastructure cases).
- Deterministic generation repeated twice with byte-identical outputs. Four builds
  PASS against final matching source/assets. Whitespace checks PASS.

| Environment | Result | Static RAM | Flash | Delta vs preceding header build |
| --- | --- | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,067,053 B | +112 B flash / 0 RAM |
| `cores3se_base` | PASS | 87,796 B | 1,067,045 B | +112 B flash / 0 RAM |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,071,629 B | +112 B flash / 0 RAM |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,597 B | +112 B flash / 0 RAM |

| Asset | Raw bytes | Gzip bytes | Gzip delta |
| --- | ---: | ---: | ---: |
| HTML | 15,109 | 3,944 | +35 |
| CSS | 15,621 | 4,262 | +62 |
| JavaScript | 64,701 | 16,368 | +4 |

All are strictly below 16 KiB. JS has **16 bytes** headroom; the unchanged cap
initially rejected the additional guard, and the narrow bootstrap reuse and
redundant text/comment cleanup brought it back under. Future additions require
an explicit asset budget plan, not an increased limit. Total gzip: 24,574 bytes.
Static figures do not establish runtime radio/heap performance or electrical
DMX integrity. Browser tests use synthetic local data, not hardware measurements.

Fourteen existing files modified by this increment; no new files:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
tests/web/test_header_browser.cjs
tests/web/test_status.mjs
tests/web/check_pipeline.py
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/presets.js
web/src/style.css
```

The native code, physical UI/gesture code, Settings section/module, telemetry
poller, WS protocol/transport, preset data format, history, stash and unrelated
files are preserved. Nothing was staged, committed, pushed, merged, tagged,
flashed or erased.

### Concise physical acceptance checklist

1. iPhone Safari and Home Screen, portrait/landscape: check full-size branding,
   full unbroken connection states, blackout ON/OFF/pending and safe areas; repeat
   quiet health checks and a real reconnect. Check desktop header unchanged.
2. Scroll and move across preset cards: selection must remain singular and stable.
   Intentional single taps select; an occupied double-tap loads once; empty slots
   do not load. Verify independent Loaded/Modified, keyboard focus, physical preset
   double-tap and confirmation controls still work. Use only approved spare slots
   for persistence tests; do not overwrite user presets casually.
3. Drag each native web slider from near its wider thumb on a four-column phone:
   only values should move. Outside the range, page scrolling and pinch zoom must
   remain available. Check final value, +/- and numeric entry with multiple clients.
4. Visit all views and scroll to the footer: one left-aligned line, no clipping or
   fixed-nav overlap. Check the Status System subcolumns and diagnostics expansion.
5. Compare sampled station/client counts and timestamps with actual clients. If
   LIVE/count mismatch recurs, retain correlated raw diagnostics rather than
   changing the number. Verify ongoing DMX, preset/blackout restoration and touch
   responsiveness; no claim of electrical waveform integrity follows from logs.

Stop for physical acceptance; do not commit or begin another stage.

## Final responsive refinement — 2026-10-28

This increment follows the user-approved consolidated layout. The measurements
below supersede the preceding polish's 52 px mobile thumb / 56 px range target;
the earlier section remains as implementation history. No application JavaScript,
HTML source, Settings, native Controller, protocol, DMX or physical UI was changed.

### Native faders and mobile spacing

- One `--fader-control-width: 44px` now controls the visible thumb and +/- width
  at every breakpoint. Both WebKit and Mozilla thumb rules use border-box sizing,
  44 x 18 px, with the existing colors, border and rounding. The track stays 6 px.
- The native range input is `min(44px + 10px, 100%)`: 54 px wide where available,
  a conservative 5 px per side beyond the thumb. It is constrained to its column;
  no overlay or custom gesture layer was introduced. Native input height/travel
  remains 160..300 px below 680 px, 200..330 px on desktop, with the existing
  150 px short-landscape override. The vertical native hit surface spans that
  travel; this does not promise a 54 px thumb hit box on every browser engine.
- At tested widths 320/375/390/430 (4 columns), 600 (6) and 768/1280 (8), Chromium's
  actual native shadow thumb measured 44 x 18 px, buttons 44 px wide and the
  range 54 px wide. Columns do not overlap. Native drag, keyboard, fine adjustment,
  numeric entry, pacing and reconciliation remain governed by unchanged code.
- In the existing phone breakpoint, `max-width: 479px`, view margin-top changes
  from 28 to 4 px. Together with the unchanged 22 px header bottom padding, the
  connection-row-to-content clearance becomes 26 px instead of 50 px: **24 px
  less** on all four views. No negative margins or additional competing override.
- The approved mobile two-row header remains 112 px high in zero-inset browser
  tests, with its 33 px logo, 19 px title, subtitle, padding and 100 x 44 px
  blackout control unchanged. Tablet/desktop header and page spacing are unchanged.
  The 100svh frame, internal scroller, safe areas and persistent nav are preserved.
- The Editor's lower Blackout ON/OFF row is retained in the identical HTML source
  and location. Offline browser tests scroll it clear of the fixed nav and verify
  authoritative ON/OFF snapshots update it while the main header is offscreen.

### Double-tap coverage and remaining browser uncertainty

Inspection found a coverage gap, rather than removal of the prior bank-button
fix: manipulation was scoped to bank/fader/preset/blackout controls. Ordinary
content and the actual internal scrolling element retained `auto`. A manipulation
rule on only a document ancestor is insufficient coverage for gestures resolved
at an intervening scroller. `body, .app-container { touch-action: manipulation; }`
now covers the application background and that scroller. Existing scoped rules
remain; only actual range inputs keep `touch-action: none` to prevent simultaneous
slider dragging and page scrolling. There is no new pointer/touch interception,
zoom-limiting viewport setting, delay or gesture JavaScript.

On supporting browsers manipulation permits normal panning and pinch zoom while
excluding native double-tap zoom. This is the identified source-level gap; the
particular Safari gesture sequence still needs a real-device retest. The custom
preset recognizer was not changed: occupied double-tap loads once, empty double-tap
does not load, and scrolling does not select/load. Repeated bank taps still work.
Copyable Status/error data and editable fields retain their selection exceptions.

### Automated validation and resources

PASS:

- Frontend navigation/focus/preferences/factory checks; 80 live interaction groups
  and 10 asynchronous Status groups, including quiet health probes and recovery.
- 14 pipeline/scope/gesture tests; 6 infrastructure tests; deterministic composer,
  manifest, content hashes, gzip round trips and two identical regenerations.
- Ten Python regression commands covering telemetry, SoftAP/stations, WS, canvas,
  startup traces, native integration, Dispatcher and reproducible driver patch.
- All 21 freshly compiled C++ host suites, including Controller, Dispatcher,
  transport fault injection, WS adapter/bridge, station policies and asset API.
- Offline Chromium at 320/375/390/430/600/768/1280: all four view gaps, seven
  connection labels, no overlap/horizontal overflow, persistent navigation,
  preset tap/scroll behavior, native drag versus page scroll, lower blackout row,
  unchanged Status fields/units, footer clearance and desktop baseline geometry.
  Actual native thumb dimensions were inspected through the browser's shadow DOM.
- Browser double-tap kept scale at 1. Two-contact touch streams increased scale
  to about 1.024 and trackpad-style pinch to about 1.25. Chromium's higher-level
  touch-pinch helper was inert on both the unchanged entry baseline and new CSS;
  it was not used as evidence of a regression or as the passing pinch test.
  These checks do not establish Safari/Home Screen gesture behavior.
- All four PlatformIO builds and Git whitespace checks passed. Source/asset
  receipts match the final inputs. Screenshot review covered phone and desktop.

Resource deltas are relative to the local consolidated-polish tree at this task's
entry, not Stage 6B.3. Values are PlatformIO static usage, not runtime heap readings.

| Environment | Result | Static RAM | Flash | Delta RAM / flash |
| --- | --- | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,067,037 B | 0 / -16 B |
| `cores3se_base` | PASS | 87,796 B | 1,067,029 B | 0 / -16 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,071,597 B | 0 / -32 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,565 B | 0 / -32 B |

| Asset | Raw bytes | Gzip bytes | Gzip delta |
| --- | ---: | ---: | ---: |
| HTML (rendered content-hash URLs) | 15,109 | 3,942 | -2 |
| CSS | 15,567 | 4,247 | -15 |
| JavaScript | 64,701 | 16,368 | 0 |

Total gzip is 24,557 B (-17 B). All three remain below 16,384 B. JavaScript still
has only **16 B** compressed headroom. Its composed source, every JS module and
its gzip are byte-identical to task entry, as are the HTML source and native code.
The generated HTML gzip changes only because it references the new CSS hash URL.

Exactly seven existing project files changed in this refinement:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
tests/web/check_pipeline.py
tests/web/test_header_browser.cjs
web/src/style.css
web/generated/index.html.gz
web/generated/style.css.gz
web/generated/manifest.json
```

Local ignored validation evidence is under
`dist/stage6b4-refinement-20261028/`. Prior pending Stage 6B.4/Status/Settings work,
untracked historical files, logs, backups, Git HEAD, empty index and stash remain
preserved. Nothing was staged, committed, pushed, merged, tagged, flashed or erased.

### Physical acceptance still required for this refinement

1. iPhone Safari and Home Screen, portrait and landscape: verify the 44 px thumb
   matches +/-; acquisition from its modest side margin remains reliable, only
   the engaged range prevents scrolling, and scrolling outside it stays natural.
   Check mouse and keyboard use on desktop and six-column tablet geometry.
2. Check branding, subtitle and full connection states on all four tabs; observe
   the reduced gap, safe areas, stable bottom nav, quiet LIVE probes and reconnect.
3. Rapidly double-tap ordinary content, bank arrows, +/- and numeric-entry controls:
   no native page zoom or unintended command, while each legitimate tap retains
   its normal meaning. Verify pinch zoom outside ranges and copyable Status data.
4. On Presets, intentional occupied double-tap loads exactly once, empty slots
   do not load and a scrolling gesture does not select. Use known safe values.
5. Scroll Editor so the header is offscreen: the unchanged lower Blackout row
   must remain visible when reached and follow authoritative ON/OFF, including
   another client's command. Verify blackout restoration and final fader values
   across clients. Do not overwrite stored presets for this CSS-only retest.

Stop for physical verification; no commit or next-stage work is authorized here.

## iOS double-tap investigation and stable blackout — 2026-10-28 follow-up

The user confirms the preceding refinement was uploaded and tested on an
up-to-date iPhone and native Safari double-tap zoom still occurs. Deployment age
is not the diagnosis. This section supersedes the earlier assumption that root
coverage alone would resolve the reported gesture regression.

### Historical comparison and diagnosis limits

- Committed `3eb72f3` (accepted Stage 6B.2) placed manipulation directly on
  `.bank-buttons .icon-button`. Its 100svh/internal-scroller architecture is
  still present. Neither those bank rules nor the mobile scroller were removed.
- Committed `5b3cea7` added active channel controls and range-only `none`, plus
  explicit manipulation on +/- and value controls. These remain unchanged.
- Pending Stage 6B.4 added nested preset text, pointer-based same-slot double-tap
  loading and explicit preset/blackout manipulation. The recognizer checks
  primary pointer, distance, duration, pointer cancellation and valid release;
  this follow-up does not change it or install another double-tap command.
- At this task's entry, body and `.app-container` had manipulation, but computed
  styles on headings, nested preset text, SVG paths, navigation text and ordinary
  inputs were `auto`. There was no later rule overriding the explicit bank or
  preset controls. `touch-action` is not inherited: those descendant computed
  values alone do **not** prove a standards-level bug, because ancestor policy
  should also be considered. Our previous Chromium tests did not establish
  native iOS gesture correctness.
- WebKit documents an iOS recognizer defect: manipulation did not prevent native
  double-tap zoom, although none did. Its upstream correction is
  [WebKit 504edb5 / 317443@main](https://github.com/WebKit/WebKit/commit/504edb5c563f9ad8d0436f60b5a608f0ec384f42).
  The older [WebKit fast-tap documentation](https://webkit.org/blog/5610/more-responsive-tapping-on-ios/)
  describes the intended ancestor behavior. The correction's existence does not
  identify the engine shipped on this particular iPhone, and an upstream landing
  is not proof of availability in an installed iOS release. The precise trigger
  for the user's observed regression remains unconfirmed without target-device
  inspection. It must not be presented as a removed CSS rule or proven cache issue.

### Focused source changes

The CSS policy is now `.app-container, .app-container * { touch-action:
manipulation; }`. This extends the last working direct-target approach to actual
app descendants as well as the scroller, including nested button text and SVG,
and removes the body-wide declaration. Browser/document gutters are left alone.
More specific range CSS still sets none only on the actual fader input. No root
viewport zoom restriction, new touch/pointer interception or global cancellation
is introduced. Form selection/editing and copyable diagnostic exceptions remain.

This is explicit target-policy hardening, not a claim that CSS can force an iOS
engine with the documented recognizer defect to comply. Scroll, pinch and native
range behavior continue to follow the existing architecture.

The blackout presentation defect was confirmed in `web/src/js/app.js`: pending
text used the **requested** state plus the word pending, and content-sized wider
buttons changed width. Now the caption is only the synchronized authoritative
`Blackout OFF`/`Blackout ON`. No requested state is presented as applied. Pending
still disables duplicate activation, sets aria-busy and supplies accessible
awaiting-confirmation text. Existing lower-row feedback, rejection/uncertain
results and recovery are reused. Loss of synchronization shows unavailable and
removes aria-pressed; no stale ON/OFF is represented as fresh.

The button reserves 128 x 44 px on wider layouts, including 600 px tablet, with
unchanged horizontal padding and a 12 px line-height; the approved phone override
remains 100 x 44 px with its existing padding. OFF, ON and two-line unavailable
text fit without resizing. Wider OFF previously measured about 120.89 px;
reserving 128 px is intentional. Header/brand/nav and button geometry stay stable
through pending, confirmation and genuine socket loss. The mobile header design,
24 px gap reduction, fader dimensions and lower Blackout row remain preserved.

Only two authored production files changed: CSS and the app rendering module.
The composed JS and four generated pipeline outputs were regenerated. No change
to protocol, operations state machine, preset recognizer, Controller, transport,
NVS, physical UI, Settings or status telemetry.

### Validation and final resource figures

PASS: frontend navigation/focus/factory checks, 80 live integration groups,
10 asynchronous Status groups, 14 pipeline checks, 6 infrastructure tests,
ten integration/regression commands, and all 21 freshly compiled C++ host suites.
The bootstrap presentation test now covers authoritative OFF during pending ON,
confirmed ON, ON during rejected OFF, busy reset/error feedback, operation timeout,
unavailable state and fresh synchronization without mutation replay. Existing
multi-client/protocol/blackout safety regression coverage remains intact.

Offline Chromium checks passed at 320/375/390/430/600/768/1280 px. They inspect
computed touch-action on all app elements and representative ancestor chains,
check body/html remain auto, exercise content/heading double taps in all four
views, nested-text preset pairs loading once, empty slots not loading, scroll
cancellation, native drag versus page scrolling and pinch gestures. Separate fresh
contexts test actual pending/confirmation/socket-loss rendering at every width:
100 x 44 phone and 128 x 44 wider buttons, no clipping or header/brand/nav movement.
The existing snapshot/mutation mock is offline; no device preset was modified.
These are Chromium results, not real Safari or Home Screen acceptance.

All four builds passed, with source/asset receipts and component hashes verified.
Deltas below are from this task's entry tree. Static usage is not runtime heap.

| Environment | Result | Static RAM | Flash | Delta RAM / flash |
| --- | --- | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,067,085 B | 0 / +48 B |
| `cores3se_base` | PASS | 87,796 B | 1,067,077 B | 0 / +48 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,071,661 B | 0 / +64 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,629 B | 0 / +64 B |

| Asset | Raw bytes | Gzip bytes | Gzip delta |
| --- | ---: | ---: | ---: |
| HTML | 15,109 | 3,942 | 0 |
| CSS | 15,728 | 4,311 | +64 |
| JavaScript | 64,578 | 16,356 | -12 |

Total gzip 24,609 B (+52 B). All stay below 16,384 B; JS now has **28 B** headroom.
Two repeat regenerations were byte-identical; manifest, composer, content hashes,
source fingerprints and Git whitespace checks pass. No firmware upload occurred.

Exactly eleven existing project files changed in this follow-up:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
tests/web/check_pipeline.py
tests/web/test_header_browser.cjs
tests/web/test_live.mjs
web/src/style.css
web/src/js/app.js
web/src/app.js
web/generated/index.html.gz
web/generated/style.css.gz
web/generated/app.js.gz
web/generated/manifest.json
```

Ignored evidence is under `dist/stage6b4-gestures-20261028/`. Prior Stage 6B.4 work,
historical files, other untracked files, logs, backups, HEAD, empty index and stash
are preserved. No staging, commit, push, merge, flash or NVS operation.

### Real iPhone verification and bounded fallback proposal

- Safari and Home Screen, portrait/landscape: repeat taps on headings, empty areas,
  Editor controls, Settings, Status, nav arrows and nested preset text. Check both
  normal and zoomed scale, scrolling, long press/copy, inputs and pinch outside
  ranges. Observe no viewport/nav movement. Check the unchanged 44 px thumbs.
- Presets: same-location single tap selects; two taps on one occupied tile load
  once; empty slots never load; scroll/movement/cancellation does not select/load.
  Do not overwrite/delete presets for this test.
- Blackout: observe OFF while ON is pending and ON while OFF is pending; no
  duplicate activation, then authoritative confirmation on both clients. Exercise
  rejection and network loss/recovery, verify unavailable fits, and confirm lower
  Editor feedback and restoration behavior remain correct.
- If native zoom persists, record exact iOS version, Safari versus standalone,
  hit-tested node and ancestor computed styles with Safari Web Inspector, plus
  visualViewport.scale before/after. Distinguish double-tap zoom from input-focus
  auto-zoom. The same inspection must cover the known working bank buttons.

If that proves an engine limitation despite correct direct-target policy, the
narrow fallback proposal is a guard attached **only to the confirmed failing
control/surface**, canceling only a recognized second short, stationary,
single-touch tap. It must exclude editable/copyable content, multi-touch and
scroll/canceled sequences and preserve the preset recognizer and ordinary click
semantics (no synthesized duplicate activation). This needs separate target-iOS
evidence and regression review: canceling touchend can suppress compatibility
clicks or change assistive behavior, and would require explicit asset-budget work.
It is not implemented here. A blanket document/body touch handler or none/pinch
restriction is not an acceptable fallback. Stop for physical verification.

## Fixed-scale iOS candidate and UI cleanup — 2026-10-28

The latest user correction supersedes every earlier pinch-preservation acceptance
criterion in this report: this application is now intended to stay at scale 1.
The user has uploaded each preceding build to the actual CoreS3SE and reports
that manipulation-only suppression still failed on current iOS. This is accepted
hardware evidence, not an assumed deployment/cache problem. This implementation
is a new **candidate**, awaiting real Safari and Home Screen verification.

### Gesture investigation and chosen compatibility policy

The comparison with accepted Stage 6B.2 and Stage 6B.3 is recorded above. Their
explicit bank-control rules were not removed; the 100svh/internal-scroller shell
is unchanged. Pending Stage 6B.4 adds pointer-based preset pairs with distance,
time, valid-release and scroll cancellation. That recognizer is byte-identical
to this task's entry. Neither source inspection nor Chromium establishes the
precise native gesture path responsible on the user's iPhone. The documented
[WebKit recognizer correction](https://github.com/WebKit/WebKit/commit/504edb5c563f9ad8d0436f60b5a608f0ec384f42)
is evidence of an engine limitation, not proof of the installed engine's behavior.

The candidate uses three complementary layers rather than another manipulation
selector adjustment alone:

1. Viewport: initial/minimum/maximum scale 1, user-scalable=no, with the existing
   width=device-width and viewport-fit=cover. This is the user's explicit revised
   policy. Safari may override author zoom constraints; this layer is not a proof.
2. Existing app-only root/descendant and interactive CSS uses `pan-x pan-y`, not
   manipulation: allow one-finger panning, omit pinch/double-tap browser zoom.
   More specific native range inputs alone retain none. Body/document gutters
   have no blanket CSS suppression; the viewport setting applies to this page.
3. In `shell.mount`, only devices reporting navigator.maxTouchPoints > 0 install
   three delegated, non-passive listeners on `.app-container`: gesturestart,
   gesturechange and dblclick. Cancel only cancelable events. The dblclick guard
   excludes inputs, textareas, editable elements, diagnostic DD values and the
   connection error so normal editing/copy selection retains its default behavior.
   The WebKit gesture hooks implement the explicit no-pinch policy even over inputs.

The [Apple gesture-event guide](https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariWebContent/HandlingEvents/HandlingEvents.html)
describes gesture cancellation and how touch cancellation affects emulated clicks.
It is an archived guide, not a guarantee for current Safari. No touchstart,
touchmove or touchend listener, synthetic click, second preset recognizer, new
application double-tap action, timing delay or zoom polling was added. In
particular, canceling a second touchend without target-iOS evidence risks losing
the click used by the existing preset recognizer; this candidate avoids that.
Dblclick cancellation is supplementary, not assumed sufficient by itself.

Callbacks are installed once with the persistent shell. Navigation still resets
the actual scroller and preserves keyboard/AT focus and same-tab position. The
scroller element lookup is shared rather than repeated. Normal input fonts remain
16 px or larger (numeric draft 18 px); input focus and selection are not canceled.
No page script can promise to prevent OS accessibility zoom or browser-UI zoom
outside its control. Safari-specific gesture delivery remains the physical gate.

### Stable Editor feedback and accessible blackout state

- Removed per-drag preview and routine channel-success strings from control.note.
  Neither local previews nor channel intent/pump/release/ack/reconciliation logic
  changed. Keeping the old per-ack success text would have reintroduced a wrapping
  helper during dragging, so normal channel-success feedback is now silent too.
  Existing rejection, transport-error, budget-busy and uncertain feedback remains.
- The fader footer is empty during normal ready/drag/release operation; unavailable
  controls show a compact unavailability message. Error and connection information
  remains available. The footer no longer alternates between variable-length
  informational messages as channel acknowledgments arrive.
- Routine successful blackout confirmation strings are filtered out of the
  visible lower detail. Pending, failed, rejected and connection-loss feedback
  remains. No change to operations acknowledgment handling or blackout semantics.
- The lower Editor Blackout ON/OFF row remains in place and now has role=status
  and aria-atomic=true. It announces the authoritative state without a second
  verbose success message. The stable OFF/ON header caption, aria-pressed,
  aria-busy, pending disabling and 100/128 x 44 px sizing remain as before.

### Human Device Uptime

`status.duration(ms)` reuses the existing unsigned-32 validation helper. It floors
milliseconds to whole seconds and returns null for missing/invalid/negative,
fractional or out-of-range values. Examples: 0 -> 0s, 37,000 -> 37s,
312,000 -> 5m 12s, 11,880,000 -> 3h 18m, 188,280,000 -> 2d 4h 18m.
Boundary tests include 59s, 60s, 59m59s, 1h and 24h. Only the Device Uptime row
uses duration units; diagnostic/sample timestamps retain their explicit raw units.

The existing native field is uint32_t(esp_timer_get_time()/1000): it wraps after
about 49.7 days. Maximum renders 49d 17h 2m; a reported rollover zero renders 0s.
The browser does not infer elapsed wraps or a longer lifetime. Failed HTTP samples
retain only the explicitly stale old duration, or Unavailable without a valid
sample. There is no new per-second timer; existing sequential endpoint polling,
10 s cycle delay, cancellation and freshness behavior are unchanged.

### Actual active SoftAP SSID

The old `/status` had no SSID field. A read-only field now comes from
`esp_wifi_get_config(WIFI_IF_AP, &active)` in the existing HTTP request task, not a
hardcoded default or cached browser preference. It queries once per existing
/status request. No Wi-Fi set/start/association call or credential/NVS operation
was added. `/ws-diagnostics` returns before this query as before.

`include/status_ssid.h` encodes only the named SSID bytes, bounded to 1..32 bytes,
using a 67-byte output for worst-case quote/backslash escaping. It follows the
current credential policy's printable ASCII constraint. Failed queries, empty,
invalid-length or unsupported-byte names serialize as null, not invented text.
The frontend displays unavailable for missing/invalid/over-32-character values and
uses textContent, preserving literal quotes/markup safely. Supporting non-ASCII
SSID configuration later requires revisiting the existing credential policy and
encoder together; no such change is made here. Password fields are never encoded
or logged. The driver API copies wifi_config_t temporarily on the HTTP stack;
added bounded stack storage is sizeof(wifi_config_t) plus 67 bytes, with no new
static buffer/task or application heap allocation. Runtime stack/heap margins
still require hardware observation; static build usage is a separate metric.

### Final automated validation

- Frontend shell/navigation/focus/preferences/factory tests and 80 live groups.
- 12 asynchronous Status groups: readable duration boundaries, uint32 rollover,
  stale HTTP failure, invalid values, safe SSID text and existing socket budgets.
- All 21 existing compiled host suites plus the new actual SSID encoder case.
- 14 pipeline checks, six infrastructure tests, ten integration/patch commands.
  Exact native reconstruction permits only the read-only SSID addition. Maximum
  actual formatted /status payload, including escaped 32-byte SSID: 2708/4096 B.
- Offline Chromium at 320/375/390/430/600/768/1280: direct and ancestor computed
  pan policy, scale remains 1 after double taps and two-contact/trackpad pinch,
  cancelable WebKit-event hooks, editable/copy exclusions, input font sizing,
  occupied nested-text preset pairs loading once, empty no-load, scroll/movement
  rejection, native fader final delivery, outside scrolling and silent footer
  geometry sampled during drag. Max-width synthetic SSID does not overflow.
  All ON/OFF/pending/unavailable button geometry and authoritative lower-row
  tests remain. These are synthetic/offline browser results, not iOS acceptance.
- Deterministic composer/gzip repeat, content hashes, manifest and the unchanged
  16,384-byte per-asset rejection gate. No minifier or extra runtime asset needed.
  Shared uint validation/scroller lookup and removed routine UI messages offset
  the additions. A redundant navigation-history comment was moved to this report
  and retained tests for budget; navigation behavior itself is unchanged.

| Environment | Result | Static RAM | Flash | Delta RAM / flash |
| --- | --- | ---: | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,067,453 B | 0 / +368 B |
| `cores3se_base` | PASS | 87,796 B | 1,067,421 B | 0 / +344 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,072,013 B | 0 / +352 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,981 B | 0 / +352 B |

Deltas are relative to this task's starting tree. All receipts match the final
source/asset fingerprints and component hashes. Static usage does not establish
runtime HTTP-task stack, radio heap or electrical DMX integrity.

| Asset | Raw bytes | Gzip bytes | Gzip delta |
| --- | ---: | ---: | ---: |
| index.html | 15,292 | 3,987 | +45 |
| style.css | 15,724 | 4,311 | 0 |
| app.js | 64,402 | 16,365 | +9 |

Total gzip 24,663 B (+54 B); JavaScript headroom is **19 B** under the unchanged
16 KiB limit. Future growth needs an explicit asset budget/splitting review; this
increment adds no asset, endpoint or dependency. No last-minute transport tuning.

### Exact increment inventory and preservation

20 existing files modified and two added:

```text
docs/STAGE6B4_WEB_PRESETS_BLACKOUT.md
include/status_ssid.h
src/network_service.cpp
tests/network/check_status_telemetry.py
tests/network/test_status_ssid.cpp
tests/web/check_pipeline.py
tests/web/check_scope.py
tests/web/test_header_browser.cjs
tests/web/test_live.mjs
tests/web/test_navigation.mjs
tests/web/test_status.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/control.js
web/src/js/shell.js
web/src/js/status.js
web/src/style.css
```

Entry HEAD 5b3cea7a5b2906a77b6a8f1c7dd66fded7d9e8e9 and branch
codex/stage6b-web-ui are preserved. The index remains empty; existing stash and
all unrelated/historical files, backups, migration bundles and logs are preserved.
Ignored evidence is under dist/stage6b4-fixedscale-20261028/. Controller, Dispatcher,
DMX driver/transport, preset recognition/storage, Wi-Fi configuration and WebSocket
transport modules remain byte-identical to entry. No commit/push/merge/tag/upload,
flash or NVS erase/write was performed.

### Minimal physical iPhone acceptance gate

1. Safari and Home Screen: repeat double taps and pinch in Editor, Presets,
   Settings, Status, header and empty content. Scale must remain fixed. Check
   portrait/landscape and expanded/collapsed Safari controls. If zoom remains,
   identify exact target and whether native gesture events reach the app; do not
   claim the engine issue solved merely because Chromium passed.
2. One-finger scrolling stays natural outside the native range. Drag a fader:
   smooth preview/final value, no simultaneous page scroll, no helper text or
   footer height change. Check numeric focus/keyboard/selection, bank taps,
   diagnostic copying and two-client authoritative reconciliation.
3. Presets: same-location occupied pair loads exactly once, empty pair does not
   load, single selection works, scrolling/movement does not select/load. Use
   existing known safe presets; no save/delete is needed for this UI retest.
4. Blackout: stable OFF/ON label through pending, disabled repeat activation,
   confirmed authoritative state on both clients and lower row without routine
   success prose. Verify error/rejection and reconnect warnings still appear.
5. Status: SSID equals the Core's actual AP name, no password exposed; uptime
   reads sensibly, polling failure is unavailable/stale rather than invented.

Stop for physical verification. The iOS no-zoom requirement remains unaccepted
until this candidate passes Safari and Home Screen testing.

## Remaining limitations and deferred work

- Historical native synchronous TCP/WebSocket sends can stall on a slow/lost
  client and delay other clients. Recovery handles loss but is not a proof that
  the send stall is permanently fixed. Queues/timeouts/task architecture were
  not enlarged or rewritten here.
- Preset confirmations are not cross-client locks, and NVS writes are not atomic
  across blob/metadata. A disconnected operation may already have applied.
- Preset naming, browser Copy and broader program/scene functionality are out of
  scope. No new web authentication is added; existing local-access security and
  predictable factory-password tradeoffs still apply to active control.
- Physical touchscreen slider precision remains the Stage 6B.3 deferred issue,
  independent of Wi-Fi activity. No physical slider changes were made.
- Unit is the known stable hardware profile; DMX Base remains experimental.
  Software test success does not resolve its historical output glitches.
- After the fixed-scale/UI-cleanup candidate, JS gzip headroom is only 19 bytes; retain the asset limit and review the asset budget before further frontend work.
- Original Stage 6B.4 physical acceptance is user-confirmed. The loaded-preset/double-tap additions and external electrical measurements still require verification. No hardware-validation claim is made for these new additions.

## Recommended physical acceptance checklist

Use a safe fixture setup and operator-chosen values; do not sweep unknown
channels (movement, shutter/strobe and other hazardous functions may be mapped).
First inventory existing P1–P8 without mutation. Preserve/record any presets the
user needs. Save/overwrite/Delete tests must use only an explicitly approved spare
slot or slot the user has consented to replace. If no such slot is available,
defer destructive tests; automated review has not modified any device slot.

1. On Unit + Battery Bottom, connect iPhone Safari/Home Screen, MacBook and Windows
   clients. Verify fresh full snapshot, eight consistent slot states, bank/touch
   navigation, physical UI and continuous DMX. Repeat relevant tests on Base
   separately with its external PSU; retain its experimental classification.
2. Load an occupied slot containing known safe values. Check all 512 projected
   values on every client and physical display; verify selected-channel value and
   real fixture response. Empty/corrupt/error slots must not become loadable.
3. Save into an approved empty slot. Check authoritative occupancy on all clients
   and physical Presets. Confirm Save leaves DMX unchanged. Load it after making
   a deliberate safe change to verify contents came from Controller working data.
4. In the approved slot, request overwrite and Delete, then Cancel each: no write,
   output change or occupancy change. Confirm the named slot only after explicit
   consent, check cross-client occupancy and unchanged live output. Optional
   user-controlled restart checks persistence; do not erase/migrate NVS.
5. Open confirmation, then change slots/tabs or have another client load a preset
   or change blackout. Confirm stale consent disappears. Check keyboard focus,
   touch selection prevention, pinch zoom and persistent mobile navigation.
6. Set a safe working look; enter blackout from web and then from physical UI.
   All clients should report ON and fixtures remain dark. Edit/load during ON,
   optionally Save only to the approved spare slot; check output remains zero.
   OFF must restore the exact pre-entry look, not those intervening edits. Repeat
   ON must not recapture; rapid local taps must not queue delayed toggles.
7. During a fader drag/numeric draft, trigger a local operation or a second
   client's preset/blackout boundary. Old unsent gestures must not reappear after
   the operation. Check final ordering of already-sent writes, and deliberately
   begin a new gesture to verify controls re-enable normally.
8. With multiple clients, exercise conflicting actions only in the approved
   disposable slot. Observe serialized outcomes/latest authoritative occupancy;
   do not expect distributed locking or preservation of an overwritten look.
9. Disconnect or background a client during a pending operation. Verify controls
   become unavailable and the outcome remains uncertain when appropriate.
   Reconnect must require a fresh full snapshot and must not replay mutations.
   Check manual Retry, quiet idle health checks and foreground recovery too.
10. Check transport/WS diagnostic counters, sustained responsiveness and current
    occupancy after testing; retain actual failures rather than clearing them to
    make results appear clean. Record any runtime heap/PSRAM changes. Logs alone
    cannot establish 513-slot wire coherence, break/MAB or frame-gap correctness;
    those require a separate external RS-485 capture/logic-analyzer plan.

Stop after automated validation. Commit remains pending user approval and final
physical acceptance; do not begin another stage.

## Final Stage 6B baseline acceptance — 2026-10-29

The user confirms the completed controller and final UI refinements are working
on the physically uploaded CoreS3SE firmware: fixed-scale iOS without unwanted
zoom, preset double-tap, reliable mobile faders, responsive Editor/Presets/Settings/
Status, stable blackout, authoritative WebSocket synchronization, readable uptime
and the actual SoftAP SSID. This acceptance supersedes the earlier candidate and
pending-hardware statements retained as implementation history above.

No production source changes were made during closure. The tested input digest
remains `35e9518abbd613cb10ac9f72ee461095a25ce0059b1f7bc16d6e08d483a24f9a`.
The HTML/CSS/JavaScript gzip assets are 3,987/4,311/16,365 bytes; JS headroom is
19 bytes under the existing 16 KiB gate. Existing limitations, including historical
synchronous send stalls, local-access security, non-atomic multi-key persistence,
physical slider precision and experimental Base behavior, remain documented;
acceptance does not establish external electrical DMX measurements.

Final automated validation passed on the unchanged production inputs:

- Frontend navigation/focus/factory checks, 80 live-state groups, 12 asynchronous
  Status groups and touch-device gesture policy tests.
- Responsive offline Chromium at 320/375/390/430/600/768/1280 pixels, including
  fixed-scale gestures, preset single/double-tap/scroll, fader final values,
  silent footer geometry, blackout pending/offline and Status rendering.
- All 21 existing C++ host suites plus the bounded SSID encoder case.
- Ten integration/patch commands, 14 pipeline tests and six infrastructure tests.
- Two byte-identical asset regenerations and manifest/content-hash verification.
- All four PlatformIO builds and final Git whitespace/staged-inventory checks.

| Environment | Result | Static RAM | Flash |
| --- | --- | ---: | ---: |
| `cores3se_unit` | PASS | 87,796 B | 1,067,453 B |
| `cores3se_base` | PASS | 87,796 B | 1,067,421 B |
| `cores3se_unit_diagnostics` | PASS | 90,516 B | 1,072,013 B |
| `cores3se_base_diagnostics` | PASS | 90,516 B | 1,071,981 B |

The local baseline commit includes only the 44 reviewed Stage 6B files. Historical
untracked development files, `docs/STAGE3_NO_RECEIVER_REGRESSION.md`, existing
logs/backups and stash are excluded and preserved. No upload, NVS operation, push,
merge, tag or release was performed. Stage 6B.5 has not started. The subsequent
architecture audit is read-only and delivered in the conversation, without a new
report file or further repository edits.
