# Preset Save/Delete NVS lifecycle correction — 2026-10-31

Status: automated validation passed and **physical acceptance confirmed by the user** for Save/Delete after import, Import/Export, reboot persistence and final Backup UI. Finalized in the local v0.3.0 source closure.
No device access, erase, flashing, staging, commit, stash operation or release.
Incoming HEAD remains `9fdf07ee02340003e2ac4efc3daf61ce8c787062`.

## Confirmed cause

A first JSON import writes a 4112-byte collection to `dmxpreset/xbank0`, then
atomically selects it with `xactive`. Old `p1`–`p8` 513-byte blobs remained in
NVS indefinitely. Subsequent individual Save/Delete stages another full bank.
With eight legacy blobs and other valid NVS data, the current 20 KiB partition
cannot always allocate that second bank. Native `nvs_set_blob` returns
`ESP_ERR_NVS_NOT_ENOUGH_SPACE` (`0x1105`). Load/Export still read the selected
bank. A fresh application boot does not remove the stored condition.

The same final regression fails against the preserved incoming source at
`fresh boot physical Save after imported bank`, reporting native `0x1105`;
it passes against the correction. Earlier native probes also reproduced Delete
failure (`metadata_failed`). The user's confirmed earlier import makes this
persistent path relevant. The device's own exact NVS error has not been captured;
do not describe the host error code as an independently measured hardware code.
The accepted Export optimization did not change the transaction implementation.

## Small, guarded correction

- `Store::load` records validation only after selected bank size/header/version/
  CRC pass. Missing selector retains the legacy path. Invalid data fails closed.
- Before a **bank-backed mutation**, with a valid selected bank and allowed
  transaction gate, `publish` calls native `reclaimLegacy`. It removes only the
  eight exact keys `dmxpreset/p1` through `p8`, committing if any existed.
- Initial legacy import does not reclaim anything before adoption. Cleanup is
  deliberately deferred until the next mutation reloads and verifies the selected
  generation. Boot, Load and Export remain read-only. Missing legacy keys cause
  no cleanup commit on subsequent mutations.
- Cleanup never touches either generation, `xactive`, `dmxmeta/cleared`, other
  keys or other namespaces. A cut during cleanup leaves the complete selected
  bank recoverable. A cleanup error stops the transaction with an error.
- Existing inactive-bank erase/write/readback/CRC/final-gate/atomic-selector
  ordering is unchanged. The current bank is never erased by its replacement.
  Ambiguous selector results remain `uncertain`; no speculative rollback or retry.
- Failed/unpublished candidates are bounded by the same two fixed bank keys;
  the next replacement reclaims the provably inactive candidate. No new bank
  names, orphan-generation list or unbounded storage is introduced.
- Native mutation errors are retained per operation. `noSpace()` distinguishes
  real capacity exhaustion from corruption/flash/read errors. Existing legacy
  writes use native set+commit with identical blob/u8 representations so their
  exact errors are available too; no legacy schema or namespace changes.
- Controller result `storage_full` is appended as domain **14**; existing numeric
  results and `command.error/status=storage_error` remain unchanged. Imports use
  `presets.result/status=storage_full`. Current strict frontend parsers accept
  these specific errors. Save/Delete show `PRESET STORAGE FULL` physically;
  browser errors advise Export backup and checking NVS capacity, without reset.
  Error resynchronization remains read-only; mutations are not replayed.

No Controller mutation semantics, DMX publication, blackout snapshots, transport,
Wi-Fi, Settings, raw fader, touch configuration or partition changes were made.
Only the physical error text case changed in `view_sender.h` (verified by exact
inverse comparison). Export still captures one coherent collection, reads one
active bank, uses 512-byte chunks and the existing progress dialog/PSRAM buffer.

## Capacity and compatibility

The partition remains at `0x9000`, length `0x5000`: five 4096-byte pages, 126
entries/page. SDK garbage collection needs one spare page; reported free entries
include that reserved capacity and cannot be treated as a free-blob guarantee.
Each 4112-byte bank uses about 132 entries; eight old 513-byte blobs occupy about
152 entries. Keeping both formats indefinitely was the avoidable pressure.

The real-SDK constrained fixture includes all eight legacy presets, a 3072-byte
unrelated blob, device settings and a credential fixture. It fails before the
fix but sustains 48 Save/Delete cycles after reclamation, ending at 374 used /
256 free / 630 total entries. Two fixed generations are sustainable for this
workload in the existing partition; no progressive leak occurs. This is not a
promise for arbitrary unrelated NVS contents. A separate test genuinely fills
the remaining space, then verifies explicit Save/Delete/Import failure while
retaining the authoritative collection. No partition enlargement or NVS reset.

Existing v0.2 presets/tombstones remain readable and mutable before import;
import does not change their data until it has successfully adopted a complete
new collection. After verified reclamation, **older firmware cannot rely on the
obsolete legacy blobs** and cannot read generation-backed changes. Export JSON
before any downgrade; do not claim reverse-firmware preset compatibility.
No unrelated settings, credentials or battery data are migrated or erased.

## Actual SDK test, not a storage-capacity mock

`tests/preset_archive/run_native_nvs.py` compiles unmodified official
`espressif/esp-idf` **v4.4.7** NVS sources. All 35 supplied SDK files are verified
against pinned Git blob hashes. The host adapters replace only NOR flash (20 KiB
RAM, no 0→1 writes without erase), Linux locking/platform headers/CRC and Arduino
wrappers. SDK blob allocation, pages, garbage collection, recovery, atomic
entries and error codes are real. Physical handlers are extracted from the
chosen source; actual Controller/Dispatcher/Bridge/protocol are compiled.
See `tests/preset_archive/README.md` for reproducible commands and SDK inputs.

Passed:

- Red pre-fix / green corrected reproduction after import **and reboot**.
- Real physical Save/Delete and web commands/acknowledgments; two-client slot
  publication; Load and byte-exact Import/Export round trips.
- Export followed by physical mutations, cancelled import, invalid import,
  another import, 48 repeated Save/Delete cycles and reboot persistence.
- No live DMX publication or working/blackout/restore packet changes from storage
  mutations. Exiting blackout still restores the original snapshot.
- First-adoption transaction: 23 flash boundaries; bank-backed cleanup and
  replacement: 52 boundaries. At each: cuts before/after, half-program/half-erase
  interruption, and API errors before/after programming. **375 injected cases**.
  Reboot exposes a complete old or new collection, never mixed slots; interrupted
  staging/cleanup can be retried. Unrelated data remains byte-identical.
- True exhaustion after 42 additional 128-byte filler records: physical, web and
  import errors are explicit, original selected collection remains readable.
- Read-only capture, corrupt selector and invalid selected CRC never reclaim.
- Native v0.2 tombstones, Save/Delete/reboot and legacy mutations after a
  cancelled first-import candidate remain compatible. All eight obsolete keys
  are absent after reclamation; both fixed-size generations remain reusable.
- Existing portable archive/fault suites, Controller/Dispatcher/transport/ISR,
  HTTP deadlines, WebSocket/recovery/network/SoftAP, assets/infrastructure,
  Settings/Wi-Fi/maintenance/navigation/fader/isolation regressions. All 13
  shipped-asset browser programs passed.
- Frontend suites include actionable no-space error/no mutation replay and strict
  import-result schema. Browser suites exercise shipped assets and accepted UI.
- All four builds, deterministic assets, ELF isolation and whitespace checks.

Host injection models specific interruption patterns; it is not proof of every
physical flash fault, electrical durability or waveform correctness.

## Build/resource results

| Environment | Result | Static RAM | Flash | Flash delta |
|---|---|---:|---:|---:|
| cores3se_unit | PASS | 106,636 | 1,119,965 | +728 |
| cores3se_base | PASS | 106,636 | 1,119,921 | +728 |
| cores3se_unit_diagnostics | PASS | 109,364 | 1,124,613 | +720 |
| cores3se_base_diagnostics | PASS | 109,364 | 1,124,569 | +700 |

Static RAM delta: **0 bytes** in every build. Existing 32 KiB PSRAM transfer
buffer/chunk budgets are unchanged. No new task, timer or allocation was added.
Legacy reclamation performs at most eight key erasures on the first relevant
mutation; latency on real flash remains to be checked physically.

| Asset | gzip bytes |
|---|---:|
| index.html | 5,169 |
| style.css | 5,299 |
| core.js | 13,325 |
| ui.js | 15,176 |

All assets are below 16,384 bytes. Source SHA-256:

`7454ba2ea4cbf568be3fefe5f7d2be0d0b21a26f6af9d5929bf3cb2d6d5d752f`

Build ID: `0.3.0-rc.1+9fdf07ee0234.modified.7454ba2ea4cb`.

## Exact task inventory

These are changes relative to the saved incoming worktree, not all pre-existing
uncommitted development. Generated HTML changes only its content-hashed URLs.
Legacy freeze-guard deltas retain exact inverse source checks, not exemptions.
Native host shims and their manifest are test-only and never linked into firmware.

- `docs/PRESET_EXPORT_IMPORT.md`
- `docs/PRESET_SAVE_DELETE_NVS_FIX.md`
- `include/preset_archive_store.h`
- `src/command_dispatcher.cpp`
- `src/dmx_controller.cpp`
- `src/dmx_controller.h`
- `src/preset_store.h`
- `src/view_sender.h`
- `src/websocket_bridge.cpp`
- `tests/network/preset_archive_delta.json`
- `tests/network/preset_archive_guard_support.py`
- `tests/preset_archive/README.md`
- `tests/preset_archive/fake/nvs.h`
- `tests/preset_archive/native/Preferences.h`
- `tests/preset_archive/native/esp_attr.h`
- `tests/preset_archive/native/esp_dmx.h`
- `tests/preset_archive/native/esp_err.h`
- `tests/preset_archive/native/esp_log.h`
- `tests/preset_archive/native/esp_partition.h`
- `tests/preset_archive/native/esp_rom_crc.h`
- `tests/preset_archive/native/esp_spi_flash.h`
- `tests/preset_archive/native/sdk-source-manifest.json`
- `tests/preset_archive/native/sdkconfig.h`
- `tests/preset_archive/run_native_nvs.py`
- `tests/preset_archive/test_adapter.cpp`
- `tests/preset_archive/test_native_nvs.cpp`
- `tests/web/test_archives.mjs`
- `tests/web/test_live.mjs`
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/ui.js.gz`
- `web/src/core.js`
- `web/src/js/actions.js`
- `web/src/js/archives.js`
- `web/src/js/protocol.js`
- `web/src/ui.js`

## Evidence and physical acceptance still required

Ignored `dist/preset-save-delete-20261031/` retains the checkpoint, incoming hash
inventory/source copy, official SDK cache, red/green logs, build logs, ELF scans
and final verification. Original RC archives/checksums, historical files, stash,
HEAD and index are unchanged. No actual presets have been opened or modified by
a test; all NVS test data is synthetic RAM-backed flash.

Manual acceptance on the affected device (no erase):

1. Export a JSON backup before testing; record all occupied slots/known values.
2. Flash only by separate operator authorization; reboot with its existing NVS.
3. Save one changed channel to an occupied and an empty slot, via both physical
   and web controls. Delete occupied slots via both paths. Check both clients.
4. Load the remaining slots, Export and compare known values; repeat mutations,
   reboot and confirm persistence. Do not retry failed writes blindly.
5. Import a deliberate test backup, then Save/Delete again, reboot and Export.
6. Verify DMX continuity, blackout entry/edit/restore and Export progress/speed.
   Watch first-mutation latency and any NVS errors. Do not use destructive reset
   as a capacity workaround. Physical power-interruption testing belongs on a
   disposable backed-up test setup under separate authorization.

Stop for physical acceptance; no release readiness claim is made here.

## Final acceptance and local closure

The user confirms all manual Preset Save/Delete, Import/Export, reboot persistence
and final Backup UI acceptance tests passed on the real controller. Earlier
“pending” statements and footprint tables describe their original checkpoints.
The final source is version 0.3.0; see [local closure](V0_3_0_LOCAL_CLOSURE.md).
No physical power-cut or injected-flash-error acceptance is inferred. Separate
Base migration, interrupted-upgrade recovery and electrical measurements remain
unverified. Host tests use synthetic data; no actual NVS was modified by automation.
