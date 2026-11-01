# v0.3.0 preset export/import â€” implementation candidate

Status: hardware-accepted by the user and finalized for local v0.3.0 source closure. Preset Save/Delete, Import/Export, reboot persistence and final UI passed on the real controller. Original rc.1 archives remain unchanged and predate this implementation. Automated tests never access device NVS.

For the subsequent export responsiveness/progress correction and current measurements, see [PRESET_EXPORT_RESPONSIVENESS.md](PRESET_EXPORT_RESPONSIVENESS.md). The initial validation record below is retained as historical evidence.

## User behavior

The Presets actions area contains two separate equal desktop cards: existing Load / Save / Delete on the left, Export / Import on the right. Below 680 px they stack at full width. Existing tiles, double-tap load, confirmations, navigation and blackout controls are unchanged. Only two visible action buttons are added; the JSON picker is a hidden native input.

Export downloads `DMX-Presets-YY-MM-DD-HH-mm.json` using browser-local time, a stable snapshot of all logically occupied slots. Missing/deleted slots are absent. A corrupt or unreadable occupied slot fails the whole export instead of silently omitting it. Empty exports are supported. Export does not revise Controller state, activate a preset, or serialize Wi-Fi credentials, settings, battery records or raw NVS data.

Import opens the native picker, validates the complete UTF-8 file locally, then shows a custom confirmation that the entire P1–P8 collection, including empty slots, will be replaced. Cancellation sends no import command. Empty collection import intentionally clears logical occupancy after confirmation. Validation is repeated in firmware before any persistent write. Imported presets are never automatically loaded.

Both hardware profiles use the same device-independent data and format. No hardware pins or Unit/Base identifiers are encoded.

## Portable file schema (version 1)

Valid empty example:

```json
{"format":"cores3se-dmx-presets","version":1,"presets":[]}
```

Each occupied record has exactly `slot`, `name`, `values`. Slot is 1..8. Names are exactly P1..P8 matching the slot; this application has no editable preset names. Arbitrary long/non-ASCII names are rejected rather than truncated or invented. `values` contains exactly 512 unsigned decimal integers (0..255), in channel 1..512 order. Start code is implicit zero and is not a channel. Sparse *values* (mostly zero) are preserved, but sparse address maps and explicit channel 0/513 records are not part of this schema. Duplicate keys/slots, unknown fields, unsupported versions/formats, fractional/exponent/signed numbers, wrong lengths, malformed/truncated JSON and files larger than 32,768 bytes are rejected. JSON whitespace, key ordering and ASCII escapes are supported; an initial UTF-8 BOM is accepted. File decoding rejects invalid UTF-8.

A complete single-preset example can be generated with:

```javascript
JSON.stringify({format: "cores3se-dmx-presets", version: 1,
  presets: [{slot: 1, name: "P1", values: Array.from({length: 512}, (_, i) => i === 22 ? 112 : 0)}]})
```

Firmware export orders slots numerically, writes compact JSON and omits absent slots. Re-export without preset changes is byte-for-byte deterministic. This is validated data, not a signed/authenticated backup; an intentionally altered but valid file will import its altered values.

## Existing WebSocket transport, bounded chunks

No new HTTP routes, server task, filesystem or socket. All messages use the existing same-origin/Host/Origin policy, session generations, shared request IDs, inbox/outbox bounds and rate limit. This inherits existing preset-write authorization; it adds no WPA2-as-authorization claim or new authentication bypass. Browser confirmation prevents accidental replacement, not a malicious already-authorized client. Maintenance/Wi-Fi physical approval rules are unchanged.

All requests have `v:1`, positive monotonically increasing `id`, and:

| type | additional fields | result |
|---|---|---|
| `presets.export` | none | `ready`, token, total bytes/count |
| `presets.read` | token, offset | `data`, up to 512 bytes Base64 |
| `presets.import` | bytes (1..32768) | `ready`, token |
| `presets.chunk` | token, offset, data | `staged`, cumulative accepted offset |
| `presets.commit` | token | `applied` or a specific error |
| `presets.cancel` | token | `cancelled` |

Response shape:

```json
{"v":1,"type":"presets.result","id":1,"revision":"296","seq":"601","status":"ready","token":1,"offset":0,"bytes":1120,"count":1,"data":""}
```

`data` contains Base64 only for `data` replies. Tokens belong to one session generation. Import chunks must be nonempty and contiguous, with canonical Base64. Requests remain <=256 bytes and replies within the existing 2560-byte bound. Commit requires all declared bytes. Errors include invalid, busy, conflict, storage_error, uncertain, unavailable and unauthorized. Existing `command.error` covers admission/framing/rate failures.

Only one global transfer, one browser request in flight and one 32 KiB PSRAM buffer. Export freezes its snapshot; subsequent edits cannot tear download chunks. Imports retain their 96-byte chunks and 100 ms pacing. Exports use 512-byte responses without an added per-read sleep, reserving each request through the existing rolling command budget with a 12-request transfer ceiling (normal controls retain 18; server remains 20). Previous fader traffic counts against the same budget. Per-request deadline is 5 seconds; export also has a 30-second overall request-admission deadline. Server expiry remains 10 seconds idle / 120 seconds overall. Initiating-browser mutations are blocked during transfer; unsent local fader intents are discarded before the transfer. Previously sent commands retain their existing acknowledgment ordering. No mutation replay occurs on reconnect, lifecycle suspension or uncertain outcome. Keep the Presets page foregrounded during a transfer; iOS suspension may abort it safely.

Other clients and physical controls remain operational. A concurrent Save/Copy/Delete/import changing stored presets invalidates `presetRevision` before attempting storage mutation (even a failed API may have written data); an older upload fails with conflict at commit rather than overwriting that newer collection. Channel edits, preset loads and blackout changes do not conflict solely because live output changed. Export is read-only. A disconnected queued import is invalidated before Dispatcher execution; the storage gate rechecks session/mode/maintenance/Wi-Fi state immediately before the selector boundary. If the socket disappears *after* that atomic boundary, the import can already have committed; the browser must treat the outcome as unknown and inspect/export the current presets, never retry automatically.

## Controller and persistent transaction

Controller owns a fixed 4097-byte staging collection and an opaque staging token. The Dispatcher carries only that token, never a borrowed JSON pointer. `replace_presets` is application-owner-only and metadata-only: it cannot invoke the DMX publisher. Successful replacement invalidates loaded-slot provenance (including provenance attached to a blackout restore snapshot), because replaced slots no longer identify the previous loaded data. Working, output and restore *packet bytes*, blackout state and selected physical channel are not changed. The pre-blackout values still restore exactly. Existing full state broadcasts update occupancy and provenance for every client before the import result.

The current 0x9000/0x5000 NVS partition is unchanged. Existing legacy `dmxpreset/p1..p8` 513-byte blobs and `dmxmeta/cleared` tombstone format remain readable before adoption. The selected generation becomes authoritative after import; a later verified bank-backed mutation reclaims only redundant `p1..p8` blobs. Tombstone metadata is retained. Before the first import, existing saves/deletes use the legacy path. After adoption, all preset operations use a single logical collection in that same namespace:

- `xbank0`, `xbank1`: two bounded transaction generations, each 4112 bytes (header/version/mask/CRC32 plus eight 512-byte value arrays).
- `xactive`: one atomic NVS U8 selector.
- Missing selector means legacy format; invalid selector/bank/CRC fails closed, never silently falls back to stale legacy records.

After loading and validating an existing selected bank, replacement first reclaims obsolete legacy `p1..p8` blobs (not on initial legacy adoption). It then erases only the inactive bank, commits, writes the complete candidate, commits, reads back and verifies size/header/CRC, rechecks the gate, then atomically changes and commits `xactive`. The previously selected bank is never erased by that replacement. Subsequent save/copy/delete also use the generation mechanism, so normal edits remain durable after import. Repeated replacements reuse the inactive bank; keys do not accumulate.

Before the selector switches, cancellation, validation error, space exhaustion, write/commit/readback failure retain the selected old collection. After an ambiguous selector API result, the code distinguishes old-selected failure from an observed new selection and returns `uncertain` where durability cannot be confirmed. It never reports partial slots as success. Simulated reboot at each operation exposes legacy/old/new complete collection, never mixed occupancy/data. Both generations remain after a commit; there is no automatic rollback command or erase. NVS atomic-entry and blob recovery still depend on the pinned ESP-IDF implementation and functioning flash hardware. Physical power-cut validation remains required; host mocks do not establish electrical/flash durability.

Capacity is bounded but not guaranteed on every existing NVS. Credentials and unrelated data are retained; obsolete legacy preset blobs are reclaimed only after validating an authoritative generation. Real insufficient space returns `storage_full` without erasing active data. It does not reset NVS or reclaim unrelated records. A downgrade to v0.2 cannot rely on stale legacy preset blobs after reclamation and cannot read generation-backed edits. Export JSON before any downgrade. This feature does not change partitions, migrate Wi-Fi/settings/battery data or promise full reverse-firmware data compatibility.

Persistent operations execute synchronously on the existing application task, as previous preset writes did. Network callbacks never write NVS/Controller. Flash latency and resulting inter-frame gaps during import still require hardware measurement; this work does not claim wire timing/coherence from host tests. Stage 3E transport ownership, immutable active packets and error policy are unchanged.

## Automated validation and resource results

See the final report/record below for exact build sizes and source fingerprint. Tests exercise the production parser/serializer, portable generation store, actual PresetStore/native-NVS adapter, Controller/Dispatcher, WebSocket Bridge/protocol, frontend factories and shipped browser assets. Fault injection covers every transaction operation, errors before/after mutation, power interruption/reboot, CRC corruption, cancellation at the final gate, conflicting updates, session loss and no replay. Native adapter mocks preserve genuine legacy raw records and tombstones, then exercise post-import Save/Copy/Load/Delete and reboot. Browser tests exercise native file selection/download, rejection, cancellation/count confirmation, occupancy updates, no live commands and responsive cards at 320/375/390/430/768/1280 px.

All prior network, HTTP deadlines, WebSocket, Controller, Dispatcher, transport, Settings, maintenance, Wi-Fi lifecycle, physical fader/navigation and diagnostics-isolation regressions are retained. Historical whole-file freeze checks use an exact hash-pinned inverse of this feature, while current functionality is tested separately; protected driver/UI/partition files are not broadly exempted.

## Physical acceptance scope and extended checklist

Final user-reported acceptance covers normal physical/web Save/Delete,
Import/Export, reboot persistence and the final Backup UI on the tested controller.
The following list also contains **proposed extended checks**, not a statement
that every item/profile was physically passed. In particular, separate Base
migration, controlled power cuts/injected flash faults, upper-flash checks and
external DMX electrical/frame-gap measurements remain unverified. Normal
acceptance does not certify those. See the final
[coverage matrix](V0_3_0_DOCUMENTATION_COVERAGE.md) and
[known issues](../KNOWN_ISSUES.md).

### Extended operator checklist

1. On Unit and experimental Base, export empty, sparse and eight occupied slots; verify first/last channels and byte-for-byte re-export. Confirm no credentials/settings appear.
2. On iPhone Safari, Home Screen mode and desktop, check two cards, native download/save and native JSON picker. Cancel confirmation and verify presets unchanged.
3. Import into existing slots; confirm the entire-collection replacement warning. Verify all occupied/empty slots on physical UI and two/three web clients. Load each only by explicit action. Verify normal Save/Copy/Delete still work and survive reboot.
4. While live DMX and blackout are active, import different data. Verify no output change or automatic load; blackout release restores the previous snapshot. Check retained physical raw-fader behavior and Precision Editor.
5. During upload, mutate stored presets from another client/physical controls and expect conflict, not lost changes. Test disconnect/foreground suspension/Wi-Fi OFF before commit, resnapshot on return, and no mutation replay. After ambiguous commit inspect the result before retrying.
6. With disposable test data and a known JSON backup, test constrained NVS and controlled power interruption at transaction stages; reboot must expose one complete collection. Do not erase actual user data for an automated test.
7. Measure real heap/PSRAM headroom and uninterrupted DMX/frame gaps under import/flash latency. Existing Base electrical glitches and historical synchronous WebSocket send stalls remain separate known limitations.
8. Keep original RC bundles/checksums and stash unchanged. This work does not authorize flashing or a new release candidate.

## Initial import/export validation record â€” 2026-10-31

All four final production/standard-diagnostics builds succeeded against the same source fingerprint. Version remains 0.3.0-rc.1; no RC archives were rebuilt.

| Environment | Static RAM (bytes) | Change | Flash used (bytes) | Change |
|---|---:|---:|---:|---:|
| cores3se_unit | 106,636 | +10,712 | 1,117,617 | +15,364 |
| cores3se_base | 106,636 | +10,712 | 1,117,581 | +15,352 |
| cores3se_unit_diagnostics | 109,364 | +10,712 | 1,122,145 | +15,328 |
| cores3se_base_diagnostics | 109,364 | +10,712 | 1,122,113 | +15,324 |

Footprint comparison uses the accepted incoming Settings font/label build logs (`dist/settings-wifi-font-label-20261031/builds.log`). In addition, one 32,768-byte PSRAM transfer buffer is allocated at setup, never per chunk. Allocation failure leaves the rest of the application running and returns transfer unavailable. Runtime headroom/latency still need real hardware acceptance.

| Gzip asset | Bytes | Change |
|---|---:|---:|
| index.html | 5,092 | +142 |
| style.css | 5,211 | +12 |
| core.js | 13,150 | +683 |
| ui.js | 14,725 | +2,101 |

All four assets remain below 16,384 bytes. The two-bundle delivery, content hashes, manifest, deterministic generation and partition layout are preserved. Final ELF/receipt checks found the archive implementation in every firmware, Stage 0 capture in diagnostics only, and no THDIFF/fader/acquisition/configuration investigation symbols in any of the four environments.

Validation completed:

- Archive host suite: actual parser, generation store fault/power-cut model, Controller/Dispatcher, Bridge/protocol and PresetStore/native-NVS adapter. Positive/negative/empty/max/sparse files, legacy compatibility, full collection atomic visibility, cancellation, malformed chunks/files, conflict, Receiver isolation, frozen export, unknown commit, fail-closed corruption and no live publication.
- A deliberate pre-fix regression reproduced the failed-write conflict edge. The corrected counter invalidates uploads before any preset storage mutation attempt, without changing normal Controller revisions/notifications.
- Frontend/navigation/live state/Settings/maintenance/Wi-Fi/Status/loader suites passed, including shared IDs, single-flight, transfer rate budget after a channel burst, request deadlines, reconnect and no replay.
- Twelve browser programs passed: maintenance navigation, four-card Settings, Wi-Fi, maintenance, Wi-Fi polish, device Settings, Confirm styling, responsive layouts, blackout status, header/faders, preset actions and archive UI. Archive/preset action coverage includes 320/375/390/430/768/1280 widths. Chromium emulation does not establish actual iOS Files/download behavior.
- An older preset-action reconnect assertion failed identically on the untouched incoming baseline because it expected zero requests despite the accepted automatic `wifi.get`. It now requires exactly that read-only subscription, still rejecting every unexpected request or mutation replay. No production reconnect behavior was changed for this test.
- Established 23-program core host harness, Wi-Fi transaction/radio/render/callback/Settings suites, Settings adapter/live previews, maintenance/original Power Off, Wi-Fi runtime OFF/ON, complete Sender navigation, physical fader and default-off isolation suites passed.
- Ten infrastructure/network integration groups passed, including 15 asset tests, six infrastructure/packaging tests, patched transport ISR/fault injection and deterministic patch-state verification.
- `git diff --check` passed. HEAD and index unchanged; stash@{0}, historical files, original RC ZIPs/checksums and all protected touch/driver/network/partition/provenance inputs were verified unchanged.

Evidence is under ignored `dist/preset-archive-20261031/`: final build logs, host/frontend/browser/regression logs, final-verification.json, task-inventory.json and ELF symbol inventories. No device was contacted or flashed.

Source fingerprint:

```text
82b0087ebc59b61e7cd43eba653a3fcf5a68531449b9d64837eb79ef4bd385a5
```

Build identifier: `0.3.0-rc.1+9fdf07ee0234.modified.82b0087ebc59`. This identifies modified local source at HEAD `9fdf07ee02340003e2ac4efc3daf61ce8c787062`, not a new committed release.

## Exact task file inventory

This is the delta from the incoming working tree, not all earlier uncommitted stabilization work. No existing investigation, migration, Settings-layout, fader, driver or historical file was removed.

Modified:

- `include/controller_types.h`
- `include/websocket_bridge.h`
- `include/websocket_service.h`
- `scripts/web_ui_bundle.py`
- `src/command_dispatcher.cpp`
- `src/dmx_controller.cpp`
- `src/dmx_controller.h`
- `src/preset_store.h`
- `src/src.cpp`
- `src/websocket_bridge.cpp`
- `src/websocket_protocol.cpp`
- `src/websocket_service.cpp`
- `tests/network/wifi_power_guard_support.py`
- `tests/web/check_scope.py`
- `tests/web/run_tests.mjs`
- `tests/web/test_live.mjs`
- `tests/web/test_preset_actions_browser.cjs`
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
- `web/src/js/transport.js`
- `web/src/style.css`
- `web/src/ui.js`

Created:

- `docs/PRESET_EXPORT_IMPORT.md`
- `include/preset_archive.h`
- `include/preset_archive_store.h`
- `tests/network/preset_archive_delta.json`
- `tests/network/preset_archive_guard_support.py`
- `tests/preset_archive/fake/Preferences.h`
- `tests/preset_archive/fake/esp_dmx.h`
- `tests/preset_archive/fake/nvs.h`
- `tests/preset_archive/run_tests.py`
- `tests/preset_archive/test_adapter.cpp`
- `tests/preset_archive/test_bridge.cpp`
- `tests/preset_archive/test_core.cpp`
- `tests/web/test_archives.mjs`
- `tests/web/test_archives_browser.cjs`
- `web/src/js/archives.js`

Responsibilities: archive headers provide the strict portable codec and generation transaction; PresetStore provides the same-namespace NVS adapter; Controller/Dispatcher own metadata-only commit and conflict epochs; Bridge/protocol/service provide bounded generation-scoped transfer and PSRAM reservation; src.cpp adds only the one startup reservation call. The JS composer/factories/HTML/CSS add the two buttons and shared-socket workflow; generated bundles/gzip/manifest reflect those sources. Test-only guards reconstruct the exact incoming baseline for old protected-file comparisons and pin the new production headers/factory. Remaining files are executable regressions and this report.

## Save/Delete capacity correction (2026-10-31)

See [PRESET_SAVE_DELETE_NVS_FIX.md](PRESET_SAVE_DELETE_NVS_FIX.md) for the real ESP-IDF reproduction, guarded reclamation lifecycle, actionable capacity errors, interruption coverage and outstanding physical acceptance. Earlier validation/resource tables above describe their original implementation checkpoints.

## Import/Export UI follow-up (2026-10-31)

Import now uses an application-styled confirmation dialog with the existing
replace-all warning and Cancel/Escape support. No request is sent until Import
is confirmed. The shared dark transfer modal then shows an animated indicator,
“Importing presets...” and “Please wait while your presets are being imported.”
It remains visible through the backend commit result; success/error feedback is
shown in the existing live status area. Reduced-motion preference stops spinner
animation. File reading, confirmation and active transfer guard duplicate imports.
The existing authoritative update refreshes preset occupancy after success.

Export filename is `DMX-Presets-YY-MM-DD-HH-mm.json`, using the browser's local
24-hour date/time when the download is initiated. JSON bytes/schema are unchanged.
No backend, NVS, serialization, protocol, transfer pacing or rollback changes.

Validated: frontend suites plus focused confirmation/progress/held-commit,
rejection/storage-full/timeout/duplicate and filename regressions; shipped Chrome
browser checks at 320/375/390/430/768/1280 px; asset/network/transport integration;
portable and real ESP-IDF NVS archive suites. All four firmware builds passed.
Gzip bytes: HTML 5257, CSS 5398, Core 13325, UI 15542 (all below 16 KiB).
Static RAM is unchanged; flash increases by 544–560 bytes from the preceding fix.
Manual iPhone Safari/Home Screen acceptance remains outstanding. No flashing,
staging or commit was performed.

## Backup copy and tab-visibility findings (2026-10-31)

The card is now titled **Backup**. Import confirmation uses fixed wording:
“This will replace the entire saved P1–P8 preset collection, including empty
slots. Live output will remain unchanged.” It no longer displays the number of
occupied imported slots. Validation still checks every record; successful import
feedback may still report the actual result. Import/Cancel behavior is unchanged.
Confirmation and shared Import/Export progress dialogs all show:
“Keep this browser tab open and active until the operation completes.”
Existing modal typography and spacing are reused, with one paragraph margin rule.

### Confirmed source-level cause of interruption

- `web/src/js/app.js:67–70`: `visibilitychange` with `document.hidden`, or
  `pagehide`, invokes `pause()` and therefore `client.suspend()`. Rendering RAF
  and Status/Settings background activity are stopped as well.
- `web/src/js/transport.js`, `suspend()` → `retire()` → `cancelArchive()`:
  this immediately rejects a pending archive Promise, cancels its response
  timer, invalidates the session generation and closes the WebSocket. New archive
  requests reject while suspended. If visibility changes between chunks during
  a pacing wait, `archives.js` detects the changed generation before sending.
- `resume()` opens a fresh session/snapshot. It deliberately does not replay
  archive requests or mutation commands; this is interruption, not resumable
  transfer. The user must inspect the outcome and explicitly retry as appropriate.
- Server `Bridge::close/drop`, `archiveAllowed` and `ownerPoll` invalidate/abort
  uncommitted staging when the owning generation disappears. A commit already
  applied before its response was lost may have succeeded: no automatic rollback
  or Import retry is claimed. Fresh authoritative state/Export establish outcome.

A new deterministic regression runs the actual app visibility handlers with the
real transport factory. Import and Export requests reject as soon as the hidden
handler runs, **without advancing the timer clock**; foreground synchronization
opens a new generation and sends no transfer replay. Thus timer throttling is
not needed to reproduce the immediate interruption. The health probe is not the
initial cause here; suspension clears its timer. HTTP Status polling is separate
from the shared-WebSocket transfer.

Additional delay remains possible: import uses 100 ms `setTimeout` pacing and
both directions use timer-based command-budget waits. Browser background timer
throttling/suspension can delay them; no real-browser scheduling trace quantifies
that contribution. Existing per-RPC timeout is 5 s, frontend transfer deadlines
are 30 s Export / 120 s Import, and server staging expires after 10 s inactivity
or 120 s total. None of these timers or lifecycle policies was changed.

The direct application lifecycle path is established by source and deterministic
reproduction. Exact timing on the user's iPhone still needs device evidence; no
new background-transfer policy or backend change is proposed in this copy pass.

Validation: all frontend suites (including actual visibility cancellation and
no replay), shipped-browser confirmation/progress/Export tests at six widths,
asset/network/transport integration and portable archive regressions. All four
firmware builds and whitespace checks passed. Current gzip bytes: HTML 5360,
CSS 5411, Core 13325, UI 15486; all below 16 KiB. Backend/NVS/lifecycle source
is byte-identical to the incoming worktree. No upload, staging or commit.

## Final acceptance and local closure

The user confirms all manual Preset Save/Delete, Import/Export, reboot persistence
and final Backup UI acceptance tests passed on the real controller. Earlier
“pending” statements and footprint tables describe their original checkpoints.
The final source is version 0.3.0; see [local closure](V0_3_0_LOCAL_CLOSURE.md).
No physical power-cut or injected-flash-error acceptance is inferred. Separate
Base migration, interrupted-upgrade recovery and electrical measurements remain
unverified. Host tests use synthetic data; no actual NVS was modified by automation.
