# v0.3.0 — preset export responsiveness and progress correction

Status: automated validation passed; the user subsequently accepted Export/Import responsiveness, progress UI and final Backup behavior on real hardware. No device contacted/flashed, no NVS writes on hardware, no staging/commit/push/release/RC generation. Existing uncommitted work, history, stash and original RC archives/checksums preserved.

## Findings and confidence

The download is **not an ordinary HTTP response**. The page loads through the existing HTTP server, then `archives.transfer()` serially exchanges `presets.export`, many `presets.read` WebSocket RPCs and `presets.cancel`, finally creating a browser Blob/download. HTTP/WS upgrade failures and socket send deadlines remain handled by the existing transport. No separate HTTP download endpoint is introduced.

Confirmed source-level inefficiencies:

- Every original read returned at most 96 bytes and incurred an explicit 100 ms browser sleep *before* its network round trip. The supplied real file (`Downloads/CoreS3SE-DMX-presets.json`) was read-only validated: 4,357 bytes, format/version correct, P1–P4, 512 values each. It required 46 read RPCs plus prepare/release (48 total). Before receiving the complete file there were 47 serial RPCs and at least 4.7 seconds of deliberate sleep, before network/scheduling latency.
- Controller capture repeatedly asked storage to inspect and load slots. On an adopted bank this repeatedly read/CRC-checked the same 4,112-byte generation: nine reads for four occupied presets, seventeen for eight. Legacy capture also repeated namespace/key lookups.
- The initiating browser intentionally held mutation admission for the transfer. With no progress display, that looked unresponsive. Physical touch/DMX has no archive-wide admission lock in firmware. Source audit did **not** establish why the reported physical controller appeared unresponsive for the full interval.
- Snapshot bytes were already isolated from later mutations. Preset updates could still contribute socket backpressure/session closure through existing bounded queues; those are genuine failures, not evidence of shared snapshot corruption. The historical synchronous TCP send-stall limitation remains and is not hidden or redesigned here.

Exact sources: `web/src/js/archives.js::transfer/request`, `web/src/js/control.js::operationDelay`, `src/dmx_controller.cpp::captureArchive`, `src/preset_store.h::captureCollection`, `src/websocket_bridge.cpp::archiveRequest/archiveReply`, `src/websocket_protocol.cpp::encode`. Unchanged networking: `src/network_service.cpp::wsSend/sweep`, HTTP-task send/receive overrides and 20 ms worker pump. No domain/NVS/socket I/O is held under the Bridge lock.

## Measured breakdown (host simulation, not hardware latency)

Actual old and new JS factories were run with the same deterministic clock/network model, synthetic 4,357-byte/4-preset file and existing rate-budget implementation:

| Network model | Old prepare | Old receive | Old download initiation | New prepare | New receive | New download initiation |
|---|---:|---:|---:|---:|---:|---:|
| 300 ms per RPC | 400 ms | 18,400 ms | 18,800 ms | 300 ms | 2,700 ms | 3,000 ms |
| 20 ms per RPC (new path) | — | — | — | 20 ms | 180 ms | 200 ms |

New RPC count is 11 total / nine reads for this file. An empty 58-byte collection initiated its download in 40 ms in the 20 ms model. Eight maximum-valued presets (16,713 bytes) took 2,202 ms in that model, including rate-budget waits, with 35 total RPCs. Cleanup is after initiation and does not keep the modal open. These are deterministic regression results, **not promises for the ESP32, Safari or real Wi-Fi**. The 300 ms model reproduces the reported order of delay, but no hardware trace proves the actual RTT or how much of the reported 20 seconds was networking versus owner-task work.

Actual old/new native Controller and PresetStore code was also compiled against in-memory NVS and run with the user's real example (values were not copied into tracked fixtures):

| Four-preset snapshot | Old NVS get calls | New | Old bank reads | New | Old host capture | New host capture | Old/new host serialize |
|---|---:|---:|---:|---:|---:|---:|---:|
| Legacy layout | 33 | 14 | 0 | 0 | 95 us | 33 us | 23 / 32 us |
| Adopted bank | 18 | 2 | 9 | 1 | 1,085 us | 41 us | 23 / 26 us |

Eight adopted slots reduced 34 get calls / seventeen bank reads to two get calls / one bank read. Host times are single-run, unoptimized CPU/mock measurements and exclude real flash/RTOS latency; deterministic bank-read counts are the stronger evidence. The lightweight Preferences/NVS fakes do not model every extra vendor Preferences wrapper lookup; the legacy counts describe this harness, not a hardware trace. Both paths serialized exactly 4,357 bytes and preserved every supplied value. No hardware phase measurement is claimed.

## Focused implementation

1. Added a bulk read-only storage capture interface. The active generation is selected, read and CRC-verified once; legacy slots are read through one handle with logical deletion/length/error checks preserved. Controller captures it in one application-owner turn, before other mutations can run, then serializes once into the existing PSRAM buffer. No second database or persistent format change.
2. Export-only responses carry up to 512 raw bytes using the Event's existing payload; a separate uint16 length prevents 256-byte truncation. Base64 response capacity/strict frontend validation are updated within the existing 2,560-byte WS output bound. Import remains 96 bytes, inbound limit 256, file cap 32 KiB, schema/version unchanged.
3. Removed the export's per-request 100 ms sleep. It reserves from the **same** rolling command budget at a conservative 12 requests/window, accounting for recent channel traffic and leaving room below the unchanged server limit of 20. Normal controls remain at 18; import pacing remains unchanged. No queue enlargement, new task, TCP tuning, retry/coalescing, send-timeout increase or bridge rewrite.
4. Added immediate, generation-scoped local admission reservation before the first await. The initiating browser rejects duplicate exports and conflicting local actions; other clients and physical controls remain free. An old transfer cannot send requests into a reconnected generation after a budget wait. All later reads use frozen bytes and never reread NVS. Source tests verify actual Dispatcher Load/Save/Delete/channel/blackout commands between reads do not alter the downloaded snapshot or add export-triggered DMX publications.
5. Added a native dark-themed dialog: **Export in progress** / **Preparing preset backup. Please wait...**. It opens synchronously, is responsive at phone/desktop sizes and uses native modal focus/inert behavior. Escape cannot prematurely dismiss a still-running request. It closes when complete validated bytes have initiated the download, or on failure. It does not claim disk persistence. The server-token cleanup may finish afterward, with bounded admission blocking. Errors remain visible; disconnected controls remain disabled until fresh synchronization. No fixed-duration overlay timer.
6. Kept existing five-second per-request timeout and added a 30-second overall export admission deadline (one active request can run up to its existing deadline). Cancellation, disconnect, timeout, truncation and invalid data do not initiate partial downloads. No mutation/transfer replay on recovery. Backend token idle expiry still bounds abandoned snapshots.
7. No new allocation: existing 32 KiB PSRAM buffer, Controller collection and storage scratch reused. NVS handles close before any response send. Event lock copies remain bounded, comparable to ordinary state snapshots. Persistent transaction, Controller mutation/DMX publication paths, faders, Settings, Wi-Fi, partition layout and provenance generation are unchanged.

## Timing evidence for manual retest

Production retains only the last successful browser timing record, with no serial tracing (failed attempts leave that older record unchanged; do not treat it as timing for a failed attempt):

```javascript
JSON.parse(document.getElementById('presets-transfer-status').dataset.exportTiming)
```

`ready_ms` = accepted click to ready response, including any budget wait/transport/snapshot work; `receive_ms` = ready to complete bytes; `finish_ms` = validation/Blob/download initiation; `bytes` = file bytes. This does not measure OS disk save. The record is replaced each export, not accumulated.

Both standard diagnostics variants additionally queue two existing Stage 0 events per successfully captured snapshot:

- `EXPORT_SNAPSHOT_US`: `rc` field is snapshot duration in microseconds.
- `EXPORT_SERIALIZE_US`: `rc` field is JSON serialization duration in microseconds.

For these events, `preset` is occupied-slot count; `screen=archive`, `owner=read-only`, `canvas=none` refer to the archive operation, not an actual screen/canvas loss. Their generic blackout field is not a live-state assertion. Existing Stage 0 heap metrics accompany the events. They use the existing 32-entry bounded queue and availableForWrite drain; no synchronous serial formatting/printing in export, no new serial input commands, no HWCDC initialization, no ISR output. Normal Unit/Base firmware excludes both event strings, verified in binaries/ELFs. The initial SDK INFO macro was found compiled out (pinned SDK maximum level is ERROR), so final instrumentation deliberately uses the existing nonblocking Stage 0 path instead.

Optional manual diagnostic commands (not executed here):

```powershell
pio run -e cores3se_unit_diagnostics -t upload --upload-port COM8
pio device monitor --port COM8 --baud 115200
```

Collect these events, browser timing, `/status` owner gap/free heap/stack values, and existing `/ws-diagnostics` send/work/queue/closure data. Clear-boot measurements help separate old high-water values. Use production for final user-experience timing; diagnostics add bounded sampling/output work.

## Automated validation

- Actual parser/store/Controller/Dispatcher/Bridge and native-NVS-adapter suites passed, including existing atomic failure/interruption/reboot/uncertain-commit tests and unchanged import behavior.
- New read-only native snapshot test covers legacy/banked four/eight/empty collections, bank-read count, real example preservation and unchanged live/blackout/restore values.
- Actual Bridge test streams a frozen 4,357-byte file in nine reads while Dispatcher Load/Save/Delete/channel/blackout operations and two clients remain operational. Snapshot/serialization/timing callback are asserted outside the lock. Genuine queue saturation still explicitly closes the slow client; Controller continues.
- JS tests exercise old/new timings, full byte-valid download, all-eight/empty, repeated export protection, local admission conflicts, simulated independent physical changes, backpressure/error/disconnect/truncation/overall deadline, generation checks and modal cleanup.
- All frontend/navigation/live/Settings/maintenance/Wi-Fi/Status/loader tests passed. Thirteen browser programs passed, including six-width modal/ESC/focus/duplicate/concurrent preset behavior and actual browser download, plus real five-second held-response timeout/reconnect with no replay. Native Chrome file APIs were used against synthetic servers; actual Safari remains manual.
- Established 23-program core harness, actual patched transport/ISR fault injection, ten network/infrastructure/asset/patch-state groups, Settings/maintenance/Wi-Fi lifecycle and physical fader/navigation/isolation checks passed. Deterministic composition/gzip/manifest and 16 KiB asset limits passed.
- Exact historical scope reconstructions were updated only for reviewed deltas; the transaction, driver, physical UI, radio and partition guards retain their baselines.

## Final resources

Comparison: incoming source `82b0087ebc59b61e7cd43eba653a3fcf5a68531449b9d64837eb79ef4bd385a5`.

| Environment | Result | Static RAM bytes | RAM delta | Flash bytes | Flash delta |
|---|---|---:|---:|---:|---:|
| cores3se_unit | PASS | 106,636 | +0 | 1,119,237 | +1,620 |
| cores3se_base | PASS | 106,636 | +0 | 1,119,193 | +1,612 |
| cores3se_unit_diagnostics | PASS | 109,364 | +0 | 1,123,893 | +1,748 |
| cores3se_base_diagnostics | PASS | 109,364 | +0 | 1,123,869 | +1,756 |

PSRAM transfer reservation remains 32,768 bytes; no additional full-file server buffer. Base64 encoding has a 685-byte temporary array instead of 129 bytes (556 bytes more temporary scratch; actual stack high-water remains a hardware measurement); verify runtime HTTP stack headroom on hardware. Existing HTTP stack remains 8 KiB and guards are unchanged.

| Gzip asset | Bytes | Delta |
|---|---:|---:|
| index.html | 5,172 | +80 |
| style.css | 5,299 | +88 |
| core.js | 13,251 | +101 |
| ui.js | 15,125 | +400 |

All remain below 16,384 bytes. All four build receipts identify the same source/asset hashes. Final source fingerprint:

```text
bf1f2a8f23190be5fcdcbb43dad206ae01222134d87f05918158d8ad51c2de04
```

Build identifier: `0.3.0-rc.1+9fdf07ee0234.modified.bf1f2a8f2319`. Version remains 0.3.0-rc.1; no RC bundle was regenerated.

## Remaining physical acceptance / limits

1. On production Unit, export the real four-preset collection and record click/ready/receive/download-initiation durations. Test empty/all-eight and compare valid content/byte-for-byte unchanged re-export.
2. During export use physical faders, physical/other-client preset Load, Save/Delete with disposable slots, and blackout ON/OFF. Confirm input response and DMX continuity, frozen export coherence and unchanged import semantics. The source/host tests do not establish electrical frame timing or fully explain the previous perceived physical pause. If it persists, collect the phase/owner-gap evidence before further architecture changes.
3. Test latest iPhone Safari and Home Screen mode: modal visibility/readability, native Files/download initiation, focus recovery, interruption/background/resume and Retry. Check the browser did not merely save a partial file.
4. Force Wi-Fi loss/slow response; verify explicit error, dismissed modal and automatic resynchronization without old requests or mutations. Existing bounded slow-client closure and synchronous native TCP-send limitation are preserved, not claimed fixed.
5. Check heap/PSRAM and HTTP stack high-water marks, two/three-client operation and Base profile separately. Host timing cannot certify ESP32 latency, flash timing, USB console delivery or RS-485 electrical integrity.

Evidence: ignored `dist/preset-export-response-20261031/` contains before/after timing logs, real-example snapshot count/timing logs, browser/host/regression/build logs, source/ELF receipts and preservation inventory. No private preset values were added to tracked fixtures.

## Exact files changed in this follow-up

Compared with the incoming uncommitted implementation, not with Git HEAD:

Modified:

- `docs/PRESET_EXPORT_IMPORT.md`
- `include/preset_archive.h`
- `include/websocket_bridge.h`
- `src/dmx_controller.cpp`
- `src/dmx_controller.h`
- `src/preset_store.h`
- `src/websocket_bridge.cpp`
- `src/websocket_protocol.cpp`
- `src/websocket_service.cpp`
- `tests/network/preset_archive_delta.json`
- `tests/network/preset_archive_guard_support.py`
- `tests/preset_archive/fake/nvs.h`
- `tests/preset_archive/run_tests.py`
- `tests/preset_archive/test_adapter.cpp`
- `tests/preset_archive/test_bridge.cpp`
- `tests/web/run_tests.mjs`
- `tests/web/test_archives.mjs`
- `tests/web/test_archives_browser.cjs`
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/src/core.js`
- `web/src/index.html`
- `web/src/js/archives.js`
- `web/src/js/control.js`
- `web/src/js/protocol.js`
- `web/src/js/transport.js`
- `web/src/style.css`
- `web/src/ui.js`

Created:

- `docs/PRESET_EXPORT_RESPONSIVENESS.md`
- `tests/preset_archive/test_snapshot.cpp`
- `tests/web/test_export_response.mjs`
- `tests/web/test_export_response_browser.cjs`

Production changes are limited to bulk snapshot reads, export payload/delivery/admission, the progress dialog, bounded diagnostics and regenerated assets. Test guard pins reconstruct the exact earlier source for legacy scope checks. The rest are focused tests and documentation; persistent transaction code and unrelated accepted work remain byte-for-byte unchanged.

## Final acceptance and local closure

The user confirms all manual Preset Save/Delete, Import/Export, reboot persistence
and final Backup UI acceptance tests passed on the real controller. Earlier
“pending” statements and footprint tables describe their original checkpoints.
The final source is version 0.3.0; see [local closure](V0_3_0_LOCAL_CLOSURE.md).
No physical power-cut or injected-flash-error acceptance is inferred. Separate
Base migration, interrupted-upgrade recovery and electrical measurements remain
unverified. Host tests use synthetic data; no actual NVS was modified by automation.
