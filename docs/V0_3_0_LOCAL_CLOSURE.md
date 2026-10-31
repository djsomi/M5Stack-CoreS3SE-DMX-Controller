# v0.3.0 local source closure

## Scope and release identity

The user accepted Preset Save/Delete, Backup Import/Export, reboot persistence
and final UI on the real controller. The user explicitly authorized including
previously accepted RC/stabilization, captured-fader, fullscreen-navigation and
local Wi-Fi ON/OFF prerequisites in one reviewed v0.3.0 source closure.

Parent: `9fdf07ee02340003e2ac4efc3daf61ce8c787062`.
Branch: `codex/stage6b-web-ui`. Version: **0.3.0**, promoting the already documented
0.3.0 target from rc.1. Existing annotated v0.1.0/v0.1.1/v0.2.0 conventions support
an annotated **v0.3.0** tag on this local commit. That tag was absent locally and
on origin at review. No earlier tag or commit is rewritten. Source provenance
marks a clean stable build `stable`; modified inputs/RC builds remain candidates.

No hosted publication, push, main-branch integration, new distribution bundle,
flashing, actual NVS access or register operation is part of closure. Original
RC archives/checksums remain unchanged and are not stable binaries.

## Accepted persistence and Backup behavior

- Existing NVS stays at 0x9000/0x5000; no enlargement or destructive reset.
- Legacy p1–p8 blobs are reclaimed only before a bank-backed mutation after the
  selected generation's size/header/version/CRC validation. Initial import,
  read-only Load/Export, current generation and atomic selector are protected.
- Inactive-bank reuse remains bounded. Failures preserve a recoverable complete
  collection; real insufficient-space errors are explicit. No unrelated keys,
  settings, credentials, namespaces or active generation are erased.
- Version-1 JSON format, staged validation, conflict gates, dual generations and
  atomic selection are retained. v0.2 records remain readable before adoption.
  Export before downgrading: old firmware cannot read imported banks or depend
  on reclaimed obsolete legacy keys. No reverse-firmware compatibility promise.
- The section is **Backup**. Import confirmation names the entire saved P1–P8
  collection, including empty slots, without a count. Live output is unchanged.
- Custom Import/Cancel confirmation, spinner/progress through the backend result,
  duplicate protection and success/error feedback are retained. Both progress
  modes and confirmation warn to keep the browser tab open and active.
- Export uses `DMX-Presets-YY-MM-DD-HH-mm.json`, browser-local date and time.
- Hiding the tab explicitly suspends the WebSocket and cancels transfers. The
  deterministic visibility regression needs no clock advancement. Foreground
  recovery obtains a new snapshot and never replays an Import/mutation. Timer
  throttling can add delay but is not needed to reproduce cancellation.

## Manual acceptance and limits

User-reported **PASS**: physical/web Save/Delete, Import/Export, reboot persistence
and final UI. Accepted earlier prerequisite behavior is retained. This is not a
claim that hardware power cuts, injected flash faults or external DMX waveforms
were physically tested. Unit is recommended; Base remains experimental.

Separate Base migration, interrupted-upgrade recovery, upper-flash write/alias,
OTA health rollback, shutdown-zero frame guarantees and electrical measurements
remain unverified. No OTA updater or accessory/profile interlock is added.
Predictable factory Wi-Fi passwords, shared associated-client control and the
historical synchronous WebSocket send-stall limitation remain documented.

Slow continuous touchscreen fader movement may exhibit intermittent coordinate
stalls originating at or before the touch-controller acquisition layer. Historical
upstream code contains the same relevant acquisition mechanisms. No application-
level cause was demonstrated. The behavior is not proven to have occurred on the
original firmware. Captured raw coordinates are retained, without smoothing.
THDIFF writes remain suspended; normal and standard diagnostics exclude all touch
investigation implementations. Stage 0 diagnostics remain available.

## Automated closure validation

Passed before committing:

- All frontend live/navigation/Settings/maintenance/Wi-Fi/Status/loader tests,
  strict import/export schemas, local filename and actual visibility cancellation.
- Portable archive, actual PresetStore, Controller/Dispatcher/Bridge and real
  **ESP-IDF 4.4.7 NVS** tests. Forty-eight repeated Save/Delete cycles, reboot,
  legacy tombstones, constrained/genuinely full NVS and unrelated data retention.
- 375 before/after/torn-write/erase/error injections across 23 first-adoption and
  52 adopted cleanup/publication flash boundaries. Old-or-new complete collection,
  interrupted cleanup/retry, CRC/selector fail-closed and actionable no-space.
- Existing Controller, Dispatcher, transport/ISR fault injection, HTTP, SoftAP,
  network/WS integration, patch-state, Settings, Wi-Fi lifecycle, maintenance,
  fullscreen navigation, fader/acquisition/snapshot/isolation and release tests.
- Eleven provenance/archive release tests, including stable-clean versus modified
  candidate status. Source fingerprint/build receipt equality in all four builds.
- Deterministic asset regeneration twice equals the checked-in generated bytes;
  all gzip assets stay below 16,384 bytes. ELF scans prove no THDIFF, fader,
  acquisition or configuration investigation implementation in any standard build.
- All 13 browser programs passed: 11 unchanged suites plus corrected archive
  and Export follow-up, including six-width dialogs/filename and recovery checks.
- Whitespace, exact explicit staging inventory and sensitive-content review.

Validation harness corrections (no firmware changes): the navigation invocation
needed its required preserved pre-fix `--before` fixture. A final browser test
exposed a pre-existing mojibake expected literal (`P1` plus wrongly decoded dash)
while actual HTML was correct. Only that exact expected text was changed to
`P1\u2013P8`, preserving the strict assertion. No assertion was weakened and no
UI, backend, persistence or transport behavior changed during closure.

### Pre-commit build footprint

These builds include the stable version promotion but still name the parent Git
revision and modified input flag. They are validation artifacts, not distribution.
After committing, all four must be rebuilt/rechecked against the new exact commit
and clean provenance before the local annotated tag is finalized.

| Environment | Result | Static RAM bytes | Flash bytes |
|---|---|---:|---:|
| cores3se_unit | PASS | 106,636 | 1,120,557 |
| cores3se_base | PASS | 106,636 | 1,120,513 |
| cores3se_unit_diagnostics | PASS | 109,364 | 1,125,205 |
| cores3se_base_diagnostics | PASS | 109,364 | 1,125,177 |

| Asset | gzip bytes |
|---|---:|
| index.html | 5,360 |
| style.css | 5,411 |
| core.js | 13,325 |
| ui.js | 15,486 |

Source SHA-256 (firmware build inputs):

`cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232`

## Commit inventory

The explicit list below includes only the approved feature and accepted production
prerequisites, build hooks, test inputs, generated assets and relevant documentation.
Untracked historical source, seven detailed investigation notes and the unrelated
Stage 3 regression note are excluded and preserved. Logs, backups, migration/RC
bundles and host toolchains are never staged. Existing stashes are untouched.

- `CHANGELOG.md`
- `README.md`
- `docs/MAIN_FULLSCREEN_NAVIGATION_FIX.md`
- `docs/PRESET_EXPORT_IMPORT.md`
- `docs/PRESET_EXPORT_RESPONSIVENESS.md`
- `docs/PRESET_SAVE_DELETE_NVS_FIX.md`
- `docs/SETTINGS_WIFI_POWER.md`
- `docs/V0_3_0_INSTALLATION.md`
- `docs/V0_3_0_LOCAL_CLOSURE.md`
- `docs/V0_3_0_MIGRATION_ACCEPTANCE.md`
- `docs/V0_3_0_RELEASE_NOTES_DRAFT.md`
- `docs/V0_3_0_STABILIZATION_PHASE1.md`
- `include/captured_fader_touch.h`
- `include/controller_types.h`
- `include/firmware_version.h`
- `include/network_service.h`
- `include/physical_fader_diagnostics.h`
- `include/preset_archive.h`
- `include/preset_archive_store.h`
- `include/thdiff_experiment.h`
- `include/touch_acquisition_diagnostics.h`
- `include/touch_configuration_snapshot.h`
- `include/websocket_bridge.h`
- `include/websocket_service.h`
- `scripts/build_provenance.py`
- `scripts/embed_web_assets.py`
- `scripts/package_release_candidate.py`
- `scripts/touch_acquisition_trace.py`
- `scripts/web_asset_pipeline.py`
- `scripts/web_ui_bundle.py`
- `src/command_dispatcher.cpp`
- `src/dmx_controller.cpp`
- `src/dmx_controller.h`
- `src/hardware_diagnostics.cpp`
- `src/network_service.cpp`
- `src/physical_fader_diagnostics.cpp`
- `src/preset_store.h`
- `src/src.cpp`
- `src/thdiff_experiment.cpp`
- `src/touch_acquisition_diagnostics.cpp`
- `src/touch_configuration_snapshot.cpp`
- `src/view_sender.h`
- `src/websocket_bridge.cpp`
- `src/websocket_protocol.cpp`
- `src/websocket_service.cpp`
- `tests/dispatcher/check_integration.py`
- `tests/fader/fake/Arduino.h`
- `tests/fader/fake/M5GFX.h`
- `tests/fader/fake/M5Unified.hpp`
- `tests/fader/fake/esp_timer.h`
- `tests/fader/run_acquisition.py`
- `tests/fader/run_tests.py`
- `tests/fader/test_acquisition.cpp`
- `tests/fader/test_captured.cpp`
- `tests/fader/test_disabled.cpp`
- `tests/fader/test_history.cpp`
- `tests/fader/test_isolation.py`
- `tests/fader/test_owner.cpp`
- `tests/fader/test_recorder.cpp`
- `tests/fader/test_runtime.cpp`
- `tests/fader/test_states.cpp`
- `tests/maintenance/test_power.cpp`
- `tests/navigation/run_tests.py`
- `tests/navigation/test_navigation.cpp`
- `tests/network/captured_fader_guard_support.py`
- `tests/network/check_integration.py`
- `tests/network/check_softap_integration.py`
- `tests/network/fader_guard_support.py`
- `tests/network/fullscreen_navigation_guard_support.py`
- `tests/network/preset_archive_delta.json`
- `tests/network/preset_archive_guard_support.py`
- `tests/network/release_guard_support.py`
- `tests/network/thdiff_guard_support.py`
- `tests/network/wifi_guard_support.py`
- `tests/network/wifi_power_delta.json`
- `tests/network/wifi_power_guard_support.py`
- `tests/preset_archive/README.md`
- `tests/preset_archive/fake/Preferences.h`
- `tests/preset_archive/fake/esp_dmx.h`
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
- `tests/preset_archive/run_tests.py`
- `tests/preset_archive/test_adapter.cpp`
- `tests/preset_archive/test_bridge.cpp`
- `tests/preset_archive/test_core.cpp`
- `tests/preset_archive/test_native_nvs.cpp`
- `tests/preset_archive/test_snapshot.cpp`
- `tests/release/test_candidate.py`
- `tests/release/test_provenance_api.cpp`
- `tests/thdiff/fake/Arduino.h`
- `tests/thdiff/fake/M5Unified.hpp`
- `tests/thdiff/run_tests.py`
- `tests/thdiff/test_experiment.cpp`
- `tests/touch_config/fake/M5Unified.hpp`
- `tests/touch_config/fake/esp_timer.h`
- `tests/touch_config/fake/freertos/FreeRTOS.h`
- `tests/touch_config/fake/freertos/task.h`
- `tests/touch_config/run_tests.py`
- `tests/touch_config/test_snapshot.cpp`
- `tests/web/check_scope.py`
- `tests/web/run_tests.mjs`
- `tests/web/test_archives.mjs`
- `tests/web/test_archives_browser.cjs`
- `tests/web/test_export_response.mjs`
- `tests/web/test_export_response_browser.cjs`
- `tests/web/test_import_progress.mjs`
- `tests/web/test_live.mjs`
- `tests/web/test_preset_actions_browser.cjs`
- `tests/wifi/check_integration.py`
- `tests/wifi/run_tests.py`
- `tests/wifi/test_settings_render.cpp`
- `tests/wifi_power/run_tests.py`
- `tests/wifi_power/test_lifecycle.cpp`
- `tests/wifi_power/test_offline_controller.cpp`
- `tests/wifi_power/test_slider_alignment.cpp`
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/style.css.gz`
- `web/generated/ui.js.gz`
- `web/src/core.js`
- `web/src/index.html`
- `web/src/js/actions.js`
- `web/src/js/app.js`
- `web/src/js/archives.js`
- `web/src/js/commands.js`
- `web/src/js/control.js`
- `web/src/js/protocol.js`
- `web/src/js/transport.js`
- `web/src/style.css`
- `web/src/ui.js`

## Remaining distribution steps

1. Review main-branch integration and publication separately; this task neither
   merges nor pushes the development branch or tag.
2. Use new production-only Unit/Base builds from the clean exact v0.3.0 tag.
   Verify commit, source fingerprint, version/profile and component receipts.
3. Prepare new individually addressed sparse-image bundles, manifests and SHA-256
   checksums. Do not reuse/rename old RC ZIPs or create padded NVS-overwriting FULL
   images. The existing candidate helper remains a local verification tool;
   inspect its candidate labeling before any future stable distribution workflow.
4. Retain concise fresh-install/upgrade instructions, NVS exclusion, Unit/Base
   distinction, downgrade backup warning and remaining migration limitations.
5. Artifact-specific distribution smoke checks and publication require separate
   authorization. No release binary/asset has been uploaded or published here.

Local evidence, exact staged diff/inventory, full host/browser/build output and
preservation hashes are kept under ignored
`dist/v0.3.0-local-closure-20261031/`. This report records only actual automated
results and user-reported acceptance, not invented hardware measurements.
