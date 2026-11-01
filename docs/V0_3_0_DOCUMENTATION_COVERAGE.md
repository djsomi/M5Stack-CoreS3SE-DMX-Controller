# v0.3.0 documentation coverage and evidence audit

Audit date: 2026-11-01. Documentation-only reconciliation; no firmware rebuild,
flash, NVS access, dependency change or publication was performed.

## Scope, provenance and evidence rules

- Previous public source: annotated `v0.2.0`, commit
  `32c0571049fa57c584167f5a4f6f07e67668430d`.
- Frozen accepted source: annotated `v0.3.0`, commit
  `59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`; tag object
  `1c9c388388a31be701c4021bdacb3d70e89906b9` remains unchanged.
- Firmware input SHA-256:
  `cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232`.
- Later publication/packaging/notice documentation is **not** in that firmware
  tag. It needs a separate reviewed commit and its own digest/checksums.
- Git history, actual current frozen source, tests and chronological reports are
  primary repository evidence. A test file establishes what is tested; historical
  PASS claims below come from recorded validation, not a new execution here.
- **H** = user-reported physical acceptance; **A** = recorded automated validation;
  **S** = source/history inspection; **P** = proposal/unperformed test. These are
  different evidence classes. Browser emulation is not real Safari verification;
  simulated ISR/NVS faults are not electrical or physical power-cut qualification.
- The exact AX200 driver/workaround is additional user-verified evidence supplied
  with this audit request. It was not previously established by a repo test.
- Live GitHub release metadata could not be fetched during the audit. The prior
  public version is established by local annotated tags and the release/migration
  records, not a claim that current hosted download contents were re-downloaded.

## Development history since v0.2.0

The first-parent release range has 18 commits. Designs/temporary investigations
are not additional shipped features. Stage 3E was integrated by source-equivalent
changes; the separate hotfix commit need not be an ancestor to establish inclusion.

| Commit | Delivered scope |
|---|---|
| `0e43640` | Authoritative Controller; Stage 0/1 hardware diagnostics |
| `4c97300` | Unified serialized command dispatcher |
| `e41d8ee` | Immutable/qualified safe DMX transport and reproducible driver patch |
| `f31411a` | 16 MiB layout/embedded assets; production Stage 3E correction integrated |
| `7f09d4c` | SoftAP/HTTP, PSRAM framebuffer, offline usability |
| `294b8b8` | Native WS Controller integration, bounded pump/diagnostics |
| `2c69aa5` | Responsive non-live web foundation, later activated by subsequent work |
| `3eb72f3` | Authoritative live state, stale-socket recovery and mobile touch polish |
| `5b3cea7` | Active web channel controls and final Editor interactions |
| `0924c63` | Web presets/blackout, Loaded/Modified and responsive baseline closure |
| `3f9230c` | HTTP sub-millisecond deadline edge correction |
| `03872b9` | Stable blackout header presentation, compact channel status |
| `4e6fbbe` | Deterministic Core/UI JavaScript delivery split |
| `ce1b9f6` | Stable preset action button presentation during pending work |
| `487fce1` | Shared Device Settings, preview/commit and responsive polish |
| `3730ea4` | Device Maintenance and original Power Off path |
| `9fdf07e` | Physically approved SoftAP credential configuration/recovery |
| `59d73dd` | Backup, NVS lifecycle correction and accepted production stabilization |

Stage 3E reference: `f513287bf4f6fd0c3717d98ea702748f18f8f291`;
see [hotfix](STAGE3E_PRODUCTION_HOTFIX.md) and [integration](STAGE4B_IMPLEMENTATION.md).
A separate cherry-pick or library upgrade is not claimed.

## Requested release coverage

| Requested area | Evidence rows below |
|---|---|
| 1. DMX engine and hardware | U01–U02, U12, E01–E06, K01/K08/K09/K12 |
| 2. Physical touchscreen UI | U02–U06, U18–U19, K02 |
| 3. Web UI and responsive interface | U07–U18, U21–U22, K06 |
| 4. Wi-Fi SoftAP/networking | U06, U19–U20, K03/K05/K13 |
| 5. WebSocket/HTTP infrastructure | E08–E11, U21, K07 |
| 6. Preset Save/Delete/persistence | U10–U11, E12–E15 |
| 7. NVS atomicity/cleanup/recovery | E13–E15, K11, M03–M05 |
| 8. Backup/progress dialogs | U14–U16, E16, K04 |
| 9. Diagnostics/fault isolation | E02–E04, E18–E20, K08/K14 |
| 10. Performance/memory | E07, E09, E16–E17, K07 |
| 11. Hardware compatibility | K01–K03/K12, M01, U02 |
| 12. Security/operations/accessibility | K03–K07/K09–K13 |
| 13. Migration/installation | M01–M05, K11, P01 |
| 14. Packaging/source availability | P01–P04, E05/E06/E17/E19 |

## User-visible release claims

Each row maps the corresponding changelog/release-note claim to implementation,
regression evidence and the scope of physical acceptance. Tests are repository
paths, never invented executable names.

| ID | Claim | Implementation / history | Tests and acceptance evidence |
|---|---|---|---|
| U01 | 512-channel state, zero start code, bounded values; physical/web authority | [Controller](../src/dmx_controller.cpp), `0e43640` | [Controller tests](../tests/controller/test_controller.cpp); [Stage 1](STAGE1_CONTROLLER.md), [Stage 2](STAGE2_DISPATCHER.md): S/A/H Unit |
| U02 | Existing grid/scroll, Precision/keypad/STEP, battery/sleep/sound retained, not all new | [view](../src/view_sender.h), [application](../src/src.cpp); compare `v0.2.0` | [closure](V0_3_0_LOCAL_CLOSURE.md), Stage 1/2 preservation; S/A/H. Battery threshold fix predates v0.3 (`v0.1.1`) |
| U03 | Captured Main fader raw coordinates bypass processed movement dead zone | [capture helper](../include/captured_fader_touch.h), [view](../src/view_sender.h), `59d73dd` | [captured tests](../tests/fader/test_captured.cpp), [isolation](../tests/fader/test_isolation.py); [stabilization](V0_3_0_STABILIZATION_PHASE1.md), closure: S/A/H; slow acquisition stalls not fixed |
| U04 | Fullscreen navigation cancels momentum and prevents stale same-iteration Main render | [view](../src/view_sender.h), `59d73dd` | [navigation regression](../tests/navigation/test_navigation.cpp); [fix report](MAIN_FULLSCREEN_NAVIGATION_FIX.md): S/A/H |
| U05 | Physical Settings x=10..240 alignment, matching button fonts, footer/network details | [view](../src/view_sender.h) | [layout](../tests/wifi/test_settings_render.cpp), [alignment](../tests/wifi_power/test_slider_alignment.cpp); [Wi-Fi Settings report](SETTINGS_WIFI_POWER.md), closure: S/A/H |
| U06 | Wi-Fi local 1 s hold ON/OFF, default ON, persistence; RESET WIFI separate 2 s next-boot intent | [application](../src/src.cpp), [network](../src/network_service.cpp), [view](../src/view_sender.h) | [lifecycle](../tests/wifi_power/test_lifecycle.cpp), [offline domain](../tests/wifi_power/test_offline_controller.cpp); Settings report/closure: S/A/H |
| U07 | Active web banked faders, +/- and numeric input, confirmed final values | [control](../web/src/js/control.js), [fader](../web/src/js/fader.js) | [live tests](../tests/web/test_live.mjs); [Stage 6B.3](STAGE6B3_ACTIVE_WEB_CONTROL.md): S/A/H iPhone/multiple clients |
| U08 | Responsive four views, compact mobile header, stable 100svh internal scroller/navigation, tab reset | [HTML](../web/src/index.html), [CSS](../web/src/style.css), [shell](../web/src/js/shell.js) | [navigation tests](../tests/web/test_navigation.mjs), [responsive browser](../tests/web/test_responsive_layout_browser.cjs); [foundation](STAGE6B1_WEB_UI_FOUNDATION.md), [responsive](STAGE6B_RESPONSIVE_UI_POLISH.md), [6B.5C](STAGE6B5C_SOFTAP_CONFIGURATION.md): S/A/H |
| U09 | Fixed-scale final gesture policy, text-selection exceptions, preset double-tap once | [CSS](../web/src/style.css), [HTML viewport](../web/src/index.html), [recognizer](../include/preset_double_tap.h) | [live tests](../tests/web/test_live.mjs), [Stage 6B.4](STAGE6B4_WEB_PRESETS_BLACKOUT.md); S/A/H; earlier unrestricted-pinch plans superseded, see K06 |
| U10 | Occupied-slot load, save/overwrite/delete confirmation; physical copy/clear retained | [Controller](../src/dmx_controller.cpp), [presets UI](../web/src/js/presets.js) | [live tests](../tests/web/test_live.mjs), [Controller](../tests/controller/test_controller.cpp); Stage 6B.4 + closure S/A/H |
| U11 | Authoritative Loaded/Modified tracking incl. imported collection invalidation | [Controller](../src/dmx_controller.cpp), [archive](../include/preset_archive.h) | [archive bridge](../tests/preset_archive/test_bridge.cpp), Stage 6B.4, [Backup](PRESET_EXPORT_IMPORT.md): S/A/H |
| U12 | Blackout zero output, pre-entry restoration, discard edits/loads on release | [Controller](../src/dmx_controller.cpp), [transport](../src/dmx_transport.cpp) | Controller/transport tests; Stage 1/3/6B.4 acceptance S/A/H; wire zero-frame proof separate |
| U13 | Stable blackout labels, compact CH status, stable preset actions, lower ON red | [app renderer](../web/src/js/app.js), [CSS](../web/src/style.css), `03872b9`/`ce1b9f6`/`487fce1` | [action browser](../tests/web/test_preset_actions_browser.cjs), [blackout status](../tests/web/test_blackout_status_browser.cjs); [action report](STAGE6B_PRESET_ACTION_PRESENTATION.md), responsive report: S/A/H |
| U14 | Whole P1–P8 version-1 JSON Backup, empty slots, validation, import leaves live output unchanged | [archive parser](../include/preset_archive.h), [store](../include/preset_archive_store.h), [UI](../web/src/js/archives.js) | [core](../tests/preset_archive/test_core.cpp), [adapter](../tests/preset_archive/test_adapter.cpp), [browser](../tests/web/test_archives_browser.cjs); Backup/closure: S/A/H |
| U15 | Custom Import/Cancel confirmation without count; Import/Export progress through result; duplicate protection | [HTML](../web/src/index.html), [archives UI](../web/src/js/archives.js) | [import progress](../tests/web/test_import_progress.mjs), [export response](../tests/web/test_export_response_browser.cjs); [Export report](PRESET_EXPORT_RESPONSIVENESS.md), closure: S/A/H; no fake percentages |
| U16 | Timestamped local-time filename, exact foreground warning in confirmation/both modals | [archives UI](../web/src/js/archives.js), [HTML](../web/src/index.html) | [archives](../tests/web/test_archives.mjs), import-progress tests; closure S/A/H; lifecycle caveat K04 |
| U17 | Five-stop settings previews, release persistence, sync, Always On direction, meaningful failure feedback | [DeviceSettings](../include/device_settings.h), [settings UI](../web/src/js/settings.js) | [settings](../tests/settings/test_settings.cpp), [preview](../tests/settings/test_preview.cpp), [frontend](../tests/web/test_device_settings.mjs); [Stage 6B.5A](STAGE6B5A_DEVICE_SETTINGS.md): S/A/H |
| U18 | Restart/Power Off web confirmation, 5 s physical CANCEL; USB Power Off original path | [maintenance](../include/device_maintenance.h), [view](../src/view_sender.h), [application](../src/src.cpp) | [power](../tests/maintenance/test_power.cpp), [maintenance](../tests/maintenance/test_maintenance.cpp); [Stage 6B.5B](STAGE6B5B_DEVICE_MAINTENANCE.md): S/A/H. Restart preparation differs from original Power Off |
| U19 | Wi-Fi physical approval/60 s expiry, blank-password keep, journal/readback recovery, safe local reset | [configuration](../include/wifi_configuration.h), [transaction](../include/wifi_credential_transaction.h), [store](../src/wifi_credential_store.h) | [configuration tests](../tests/wifi/test_configuration.cpp), [store](../tests/wifi/test_store.cpp); Stage 6B.5C S/A/H; no physical power-cut claim |
| U20 | SoftAP offline UI, four-station config, DHCP no-default-router, tested cellular coexistence | [SoftAP policy](../src/softap_policy.cpp), [network](../src/network_service.cpp) | [policy tests](../tests/network/test_softap_policy.cpp); [Stage 5C](STAGE5C_SOFTAP_USABILITY.md), [Stage 5](STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md): S/A/H, OS-specific routing |
| U21 | Status metrics with stale/unavailable indication, browser-only preferences | [Status UI](../web/src/js/status.js), [preferences](../web/src/js/preferences.js), [network](../src/network_service.cpp) | [Status tests](../tests/web/test_status.mjs); Stage 6B.4/closure S/A/H. Telemetry fetch only while Status is active, not a new control authority |
| U22 | Desktop four independent Settings cards; mobile stack, password eye, stable confirm layout | [HTML](../web/src/index.html), [CSS](../web/src/style.css) | [card layout](../tests/web/test_settings_card_layout_browser.cjs), [Wi-Fi polish](../tests/web/test_wifi_polish_browser.cjs); Stage 6B.5B/5C S/A/H |

## Internal engineering and performance claims

| ID | Claim | Source / history | Tests, reports and qualification |
|---|---|---|---|
| E01 | Single state owner and serialized Dispatcher; no networking-task NVS/display/DMX writes | [Controller](../src/dmx_controller.h), [Dispatcher](../src/command_dispatcher.cpp), [Bridge](../src/websocket_bridge.cpp) | [Dispatcher tests](../tests/dispatcher/test_dispatcher.cpp), [integration](../tests/dispatcher/check_integration.py); Stage 1/2/6A S/A/H |
| E02 | Immutable active TX, 513-byte qualified completion, safe retirement; ordered blackout barriers | [transport](../src/dmx_transport.cpp), [driver patch](../patches/esp_dmx_v202/tx_service.h) | [transport tests](../tests/transport/test_transport.cpp), [patch tests](../tests/transport/test_patch.py); [Stage 3](STAGE3_TRANSPORT_IMPLEMENTATION.md), [ownership design](STAGE3_BUFFER_OWNERSHIP_DESIGN.md): S/A; physical Unit function H, electrical qualification P |
| E03 | Sender-only CLASH correction, real incomplete/timeouts still faults; receiver separation | [patch generator](../scripts/esp_dmx_tx_patch.py), tx_service above | Stage 3E report + Stage 4B integration; transport/ISR fault injection S/A/H no-receiver evidence; not Base-glitch cure |
| E04 | Three actual consecutive failures fail-stop; success resets count, safe retry latest state | [transport](../src/dmx_transport.h), [implementation](../src/dmx_transport.cpp) | Transport tests + Stage 3 report S/A; no unlimited recovery guarantee |
| E05 | Retained espressif32 6.7.0 / Arduino 2.0.16 / M5Unified .21 / M5GFX .28 / esp_dmx v2.02 | [PIO](../platformio.ini), [notices](../THIRD_PARTY_NOTICES.md) | [upgrade evaluation](ESP_DMX_UPGRADE_EVALUATION.md), publication inventory; v4.1.0 design/build evaluation is not a shipped upgrade |
| E06 | Reproducible exact-source compatibility and TX patches, fresh libdeps/idempotence checks | [compatibility hook](../scripts/patch_esp_dmx_v202.py), [TX hook](../scripts/patch_esp_dmx_tx_ownership.py) | Patch-state tests, Stage 3/closure S/A; no permanent manual .pio edits |
| E07 | Shared 153600-byte PSRAM canvas, about 153.6 KB internal recovery resolves observed Wi-Fi NO_MEM | [canvas policy](../include/fullscreen_canvas_memory.h), [view](../src/view_sender.h) | [canvas test](../tests/network/test_canvas_memory.cpp); [Stage 5A](STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md), [5B](STAGE5B_PSRAM_FRAMEBUFFER.md): S/A/H. Historical measurements are not current final runtime heap |
| E08 | Native bounded HTTP/WS, strict request validation, session generations, coherent snapshot/revision ordering | [bridge](../include/websocket_bridge.h), [protocol](../src/websocket_protocol.cpp), [wire](../include/websocket_wire.h) | [WS tests](../tests/network/test_websocket.cpp), [adapter](../tests/network/test_websocket_adapter.cpp); [Stage 6A](STAGE6A_WEBSOCKET_IMPLEMENTATION.md): S/A/H |
| E09 | Corrected under-draining pump, FIFO fairness/bounded work, no blind queue increase | [pump](../include/websocket_pump.h) | [seven-update report](STAGE6A_WEBSOCKET_DISCONNECT_INVESTIGATION.md); synthetic red/green queue reproduction A, original two hardware closes not retrospectively proven overflow |
| E10 | Application idle probe 15 s/10 s, one pending, stable LIVE, generation invalidation/fresh snapshot, no replay | [transport UI](../web/src/js/transport.js), [state](../web/src/js/state.js) | [live tests](../tests/web/test_live.mjs); [Stage 6B.2](STAGE6B2_LIVE_WEBSOCKET_STATE.md): S/A/H |
| E11 | Reject HTTP budgets below 1000 us before timeval truncation; preserve 3 s absolute/byte budgets | [network io](../src/network_service.cpp), [HTTP policy](../src/http_policy.cpp), `3f9230c` | [HTTP tests](../tests/network/test_http.cpp); 0/1/500/999/1000 us and larger boundaries A; WS path remains separate |
| E12 | Missing legacy slots no longer cause routine NOT_FOUND log spam; genuine errors retained | [PresetStore](../src/preset_store.h) | Stage 1/2 and Controller/storage tests S/A/H; absence query is not blanket error suppression |
| E13 | Dual fixed 4112-byte banks/xactive atomic U8; CRC/readback/gate, old-or-new recovery | [generation store](../include/preset_archive_store.h), [native adapter](../src/preset_store.h) | [actual SDK test](../tests/preset_archive/test_native_nvs.cpp), [test guide](../tests/preset_archive/README.md); [NVS fix](PRESET_SAVE_DELETE_NVS_FIX.md): S/A/H normal reboot, physical power cuts P |
| E14 | Reclaim only obsolete p1..p8 after selected bank validated; protect settings/credentials/active selector | PresetStore/generation store above, `59d73dd` | Native SDK red/green after import+reboot; 48 Save/Delete cycles, 375 injected cases, corruption/capacity cases A; normal physical/web acceptance H |
| E15 | Explicit storage_full and uncertain failure handling; bounded generation reuse | [Controller results](../src/dmx_controller.h), PresetStore/Bridge | NVS fix/Backup/closure; source/SDK/frontend A/H. Host 0x1105 reproduction is not a captured device error code |
| E16 | Export coherent bulk capture, 512-byte downloads/rate-budget handling/progress; import retains protections | [archive](../include/preset_archive.h), [Bridge](../src/websocket_bridge.cpp), [UI](../web/src/js/archives.js) | [snapshot](../tests/preset_archive/test_snapshot.cpp), [response tests](../tests/web/test_export_response.mjs); [responsiveness](PRESET_EXPORT_RESPONSIVENESS.md): S/A/H normal functionality; electrical latency P |
| E17 | Deterministic composer, four gzip assets/Core-UI split, hashes and <16384 byte limits | [composer](../scripts/web_ui_bundle.py), [pipeline](../scripts/web_asset_pipeline.py), [manifest](../web/generated/manifest.json) | [pipeline tests](../tests/web/check_pipeline.py), [split report](STAGE6B_JS_DELIVERY_SPLIT.md): S/A/H four stations/three WS |
| E18 | Stage 0 diagnostics, bounded network/transport counters, isolated touch tools and no standard THDIFF writes | [diagnostics](../src/hardware_diagnostics.cpp), [instrumentation hook](../scripts/touch_acquisition_trace.py) | [isolation](../tests/fader/test_isolation.py); [Stage 0](STAGE0_DIAGNOSTICS.md), stabilization/closure S/A/ELF checks; historical investigative source retained |
| E19 | Build/profile/source identity and clean/modified receipts; boot label retained | [provenance](../scripts/build_provenance.py), [version](../include/firmware_version.h) | [candidate tests](../tests/release/test_candidate.py), [API test](../tests/release/test_provenance_api.cpp); closure + current four infrastructure receipts S/A |
| E20 | Preserve working USB console/upload initialization; no separate HWCDC begin regression | [application](../src/src.cpp), diagnostics | [USB investigation](USB_UPLOAD_REGRESSION.md), closure/isolation tests S/A/H; not a claim every host/cable enumerates identically |

## Limitations and compatibility coverage

The public [known-issues document](../KNOWN_ISSUES.md) is authoritative for the
following qualifications; concise release notes link it rather than hiding limits.

| ID | Limitation / claim boundary | Evidence and remaining confirmation |
|---|---|---|
| K01 | Unit recommended; Base experimental; no accessory interlock | [profile pins](../include/dmx_hardware.h), README; Stage 3E/4B and closure. Historical Unit/Base comparison at `cab7465` predates v0.3. No proven Base-glitch root cause or final Base migration |
| K02 | Slow continuous acquisition stalls, unknown THDIFF A0→C8 cause, no original-firmware physical reproduction | Stabilization report + retained local acquisition/investigation reports. Captured fix retained; upstream comparison is `8a3c0bcc7d83eceebe7acff932871444313207b1` (CoreS3SE handling), with historical unpinned dependencies/physical reproduction unproven. No authorized THDIFF writes in standard builds. Deferred, not a release cure |
| K03 | Windows AX200 24.20.2.1 / No SMPS workaround, client compatibility classification | Stage 6A records 26–30% packet loss/clean Mac; current user request adds exact driver and verified adapter workaround. No independent retest or all-driver generalization |
| K04 | Backgrounding explicitly cancels Backup; commit outcome can be uncertain | archives/transport UI + [import tests](../tests/web/test_import_progress.mjs), Backup/closure. Foreground reconnect never replays writes |
| K05 | Predictable MAC password, shared control, no HTTPS/WSS/application auth | SoftAP policy/credential store/Bridge; Stage 5/5C/6A/maintenance/Wi-Fi reports. Approval/countdown not universal authorization |
| K06 | Fixed-scale final viewport, restricted selection/gestures; no complete accessibility audit | index.html/CSS source above; Stage 6B.4/final cleanup supersedes earlier pinch-preservation goals. Keyboard tests do not prove WCAG compliance |
| K07 | Synchronous send can block HTTP task, slow clients affect others | Stage 6B.2 measured 518330 us timeout; later successful multi-client runs do not disprove limitation. No nonblocking rewrite; idle work frequency deferred |
| K08 | No electrical DMX qualification from logs/host tests | Stage 3/ownership/closure; RS-485 levels, break/MAB, slots, gaps/coherence under load require external capture |
| K09 | Power Off legacy zero staging not three qualified wire frames | maintenance report; actual DeviceMaintenance commit bypasses Restart-only preparation for Power Off. USB working H; wire completion unverified |
| K10 | Apparent five-second countdown near automatic firmware-update reboot | Stage 6B.5B deferred observation; cause/automatic-reset cancellation unproven. No OTA feature inferred |
| K11 | Finite NVS, corruption fail-closed; SDK startup may format broken storage; downgrade incompatible after adoption | NVS fix, Backup and installation; arbitrary capacity/power-cut guarantees not asserted |
| K12 | Receiver mode separated; new Wi-Fi approval/maintenance surfaces have mode gates | Bridge/Wi-Fi/maintenance source and their tests; do not market full web receiver control or universal legacy-board acceptance |
| K13 | No STA/AP+STA, router/captive portal, OTA updater or LittleFS implementation | network source, Stage 4 design vs implemented Stage 4B; reserved partitions/plans not features |
| K14 | Historical diagnostic M5GFX metadata mismatch not a production dependency claim | [publication review](V0_3_0_PUBLICATION_READINESS.md), source inventory; production source delivery locks .28. Diagnostic cache metadata .34 is isolated from accepted production artifacts |

## Installation, migration and release material claims

| ID | Claim | Evidence / acceptance boundary |
|---|---|---|
| M01 | 16 MiB measured flash, 8 MiB PSRAM; actual table verified on Stage 4B hardware | Stage 0/4B/5B reports; [partition CSV](../partitions/cores3se_16mb_ota.csv). Upper-flash write/alias not proven |
| M02 | Old 8 MiB vs new 16 MiB layout; NVS 0x9000/0x5000 unchanged, app0 0x10000/app1 0x610000, two 0x600000 slots | CSV, [layout validator](../scripts/firmware_layout.py), Stage 4B report/installation. Reserved storage not filesystem service |
| M03 | Sparse 0/0x8000/0xe000/0x10000 images exclude NVS incl. sector rounding; otadata reset to app0 | [packager](../scripts/package_release_candidate.py), [infrastructure tests](../tests/infrastructure/test_infrastructure.py), [release tests](../tests/release/test_candidate.py). App-only cannot migrate; old FULL covers NVS |
| M04 | Simplified Unit v0.2.0→rc.1 retained seeded data/reboot | [acceptance](V0_3_0_MIGRATION_ACCEPTANCE.md): user PASS 2026-10-30. Destructive old baseline setup was expressly approved; not a migration data-loss event |
| M05 | Base migration, final stable-bundle migration, interrupted upgrade/ROM recovery, preboot NVS equality remain outstanding | acceptance/installation/closure. Recovery instructions are a procedure to validate, not evidence of completed power-cut recovery or automatic health rollback |
| P01 | Two Wi-Fi-capable production profile ZIPs, addressed images/manifests/checksums, no RC relabel/FULL | [installation](V0_3_0_INSTALLATION.md), packager/provenance validators. No final stable ZIP generated/published by this task |
| P02 | Original M5Stack 2022 attribution, exact dependency/source-material notices, mixed M5GFX subcomponent licenses | [LICENSE](../LICENSE), [notices](../THIRD_PARTY_NOTICES.md), [manifest](../licenses/manifest.json), [source inventory](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md); publication supplement, not frozen-tag change |
| P03 | Frozen source/dependency/build material, reviewed retained/excluded files; Unit/Base modified-Arduino rebuild/relink | [LGPL guide](V0_3_0_LGPL_DELIVERY.md), inventory, [relink verifier](../scripts/validate_relink.py); recorded fresh reconstruction/marker verification A, not bit-identical timestamp rebuild or legal opinion |
| P04 | Frozen firmware commit/fingerprint distinct from publication commit/tool digest/source-notice checksums; stale receipts rejected | [publication tool](../scripts/publication_release.py), [tests](../tests/release/test_publication.py), [readiness](V0_3_0_PUBLICATION_READINESS.md). Publication commit still unset; new docs require fresh packaging input/provenance review |

## Complete report corpus and superseded statements

All Stage 0–6 and follow-up reports were indexed by content/hash/headings and
cross-checked for implementation, validation and acceptance statements. They are
chronological records, not interchangeable final release specifications.

- Stage 3 gate/ownership and esp_dmx upgrade evaluation explain alternatives;
  v4.1.0 migration/hardware-timer plans are not shipped.
- Stage 4 design contains future LittleFS/OTA/network architecture options;
  Stage 4B implementation/actual table determine shipped scope.
- Stage 5A failed-memory checkpoints are historical; Stage 5B PSRAM and Stage 5C
  offline improvements plus final acceptance supersede those failures.
- Stage 6B planning/non-live foundation and early no-polling/no-mutation phases are
  historical. Final active controls and Status-only telemetry are implemented.
- Earlier pinch-zoom preservation goals are superseded by the accepted fixed-scale
  HTML/CSS. Early pending-state presentation/Refresh/prose and intermediate Settings
  geometry/hold labels are superseded by the final source.
- Pre-commit footprint/version/fingerprint tables describe their checkpoints, not
  immutable accepted stable binaries. Stage 0 heap values are not current runtime
  headroom after all networking/Backup features.
- Backup's old checklist heading could imply every proposed measurement passed.
  Its clarification now separates user acceptance from physical power-cut, Base,
  interrupted-migration and electrical checks that remain unperformed.
- Local untracked investigation notes remain preserved historical evidence; they
  are not silently added to the frozen source or release inventory.

### Reviewed document index

The table is a navigation index, not a statement every plan/test in every report
was implemented or performed. Publication-only files are later than the firmware
commit. Historical untracked notes are local context and may not be in the tag.

| Report | Role |
|---|---|
| [ESP_DMX_UPGRADE_EVALUATION.md](ESP_DMX_UPGRADE_EVALUATION.md) | Architecture/options/gate; implemented scope must be checked against source |
| `FT6336U_READ_ONLY_CONFIGURATION.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| `FT6336U_THDIFF_ABA_TEST.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| [MAIN_FULLSCREEN_NAVIGATION_FIX.md](MAIN_FULLSCREEN_NAVIGATION_FIX.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| `PHYSICAL_FADER_ACQUISITION_TRACE.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| `PHYSICAL_FADER_DEADZONE_FIX.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| `PHYSICAL_FADER_INVESTIGATION.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| `PHYSICAL_FADER_SLOW_DRAG_DIAGNOSTICS.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| [PRESET_EXPORT_IMPORT.md](PRESET_EXPORT_IMPORT.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [PRESET_EXPORT_RESPONSIVENESS.md](PRESET_EXPORT_RESPONSIVENESS.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [PRESET_SAVE_DELETE_NVS_FIX.md](PRESET_SAVE_DELETE_NVS_FIX.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [SETTINGS_WIFI_POWER.md](SETTINGS_WIFI_POWER.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE0_DIAGNOSTICS.md](STAGE0_DIAGNOSTICS.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE1_CONTROLLER.md](STAGE1_CONTROLLER.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE2_DISPATCHER.md](STAGE2_DISPATCHER.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE3_BUFFER_OWNERSHIP_DESIGN.md](STAGE3_BUFFER_OWNERSHIP_DESIGN.md) | Architecture/options/gate; implemented scope must be checked against source |
| `STAGE3_NO_RECEIVER_REGRESSION.md` (local, not distributed) | Historical local investigation; preserve, no new hardware proof |
| [STAGE3_TRANSPORT_GATE.md](STAGE3_TRANSPORT_GATE.md) | Architecture/options/gate; implemented scope must be checked against source |
| [STAGE3_TRANSPORT_IMPLEMENTATION.md](STAGE3_TRANSPORT_IMPLEMENTATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE3E_PRODUCTION_HOTFIX.md](STAGE3E_PRODUCTION_HOTFIX.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE4_WEB_INFRASTRUCTURE_DESIGN.md](STAGE4_WEB_INFRASTRUCTURE_DESIGN.md) | Architecture/options/gate; implemented scope must be checked against source |
| [STAGE4B_IMPLEMENTATION.md](STAGE4B_IMPLEMENTATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md](STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md](STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE5B_PSRAM_FRAMEBUFFER.md](STAGE5B_PSRAM_FRAMEBUFFER.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE5C_SOFTAP_USABILITY.md](STAGE5C_SOFTAP_USABILITY.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6A_WEBSOCKET_DISCONNECT_INVESTIGATION.md](STAGE6A_WEBSOCKET_DISCONNECT_INVESTIGATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6A_WEBSOCKET_IMPLEMENTATION.md](STAGE6A_WEBSOCKET_IMPLEMENTATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B1_WEB_UI_FOUNDATION.md](STAGE6B1_WEB_UI_FOUNDATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B2_LIVE_WEBSOCKET_STATE.md](STAGE6B2_LIVE_WEBSOCKET_STATE.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B3_ACTIVE_WEB_CONTROL.md](STAGE6B3_ACTIVE_WEB_CONTROL.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B4_WEB_PRESETS_BLACKOUT.md](STAGE6B4_WEB_PRESETS_BLACKOUT.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B5A_DEVICE_SETTINGS.md](STAGE6B5A_DEVICE_SETTINGS.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B5B_DEVICE_MAINTENANCE.md](STAGE6B5B_DEVICE_MAINTENANCE.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B5C_SOFTAP_CONFIGURATION.md](STAGE6B5C_SOFTAP_CONFIGURATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B_JS_DELIVERY_SPLIT.md](STAGE6B_JS_DELIVERY_SPLIT.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B_PRESET_ACTION_PRESENTATION.md](STAGE6B_PRESET_ACTION_PRESENTATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B_RESPONSIVE_UI_POLISH.md](STAGE6B_RESPONSIVE_UI_POLISH.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [STAGE6B_WEB_UI_PLAN.md](STAGE6B_WEB_UI_PLAN.md) | Architecture/options/gate; implemented scope must be checked against source |
| [USB_UPLOAD_REGRESSION.md](USB_UPLOAD_REGRESSION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [V0_3_0_INSTALLATION.md](V0_3_0_INSTALLATION.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [V0_3_0_LGPL_DELIVERY.md](V0_3_0_LGPL_DELIVERY.md) | Later publication/source/notice/relink evidence |
| [V0_3_0_LOCAL_CLOSURE.md](V0_3_0_LOCAL_CLOSURE.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [V0_3_0_MIGRATION_ACCEPTANCE.md](V0_3_0_MIGRATION_ACCEPTANCE.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [V0_3_0_PUBLICATION_READINESS.md](V0_3_0_PUBLICATION_READINESS.md) | Later publication/source/notice/relink evidence |
| [V0_3_0_RELEASE_NOTES_DRAFT.md](V0_3_0_RELEASE_NOTES_DRAFT.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |
| [V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md) | Later publication/source/notice/relink evidence |
| [V0_3_0_STABILIZATION_PHASE1.md](V0_3_0_STABILIZATION_PHASE1.md) | Implementation/validation/acceptance record; historical checkpoints qualified above |

## Verification performed in this documentation audit

- Inspected local refs, the 18-commit release range, current complete worktree and
  preserved prior publication changes; index remained empty.
- Cross-checked current source, generated manifest, frozen build receipts, reports
  and test cases. Historical test/build PASS is attributed to those records;
  no firmware build/relink or hardware test was repeated for Markdown edits.
- Documentation links, release-claim coverage, required UI strings, warning/
  workaround qualifications and preserved source/artifact/ref fingerprints are
  checked in the local audit record. Current assets remain byte-identical.
- This audit reran **15 composer/asset tests, 11 notice/document-link tests,
  11 candidate-packaging tests and six infrastructure tests**, all PASS. The
  frontend runner also passed, including **84 live integration groups**, Backup/
  visibility/import-progress/export cases and **12 asynchronous Status groups**.
  Test archives were disposable synthetic fixtures, not release firmware bundles.
- At the documentation-only audit checkpoint, the publication gate rejected the unreviewed documentation
  inventory (`non-publication tracked change since frozen firmware`):
  `docs/PRESET_EXPORT_IMPORT.md` is not in its explicit list, and the two new
  documentation files are not fingerprinted by that list. This is an intentional
  remaining integration gate, not permission to bypass provenance. Tooling was
  not changed. All changes remain unstaged/uncommitted; a clean publication
  commit is also still required.
- Tests requiring a firmware rebuild/relink or hardware are outside this
  documentation-only audit. Asset bytes, source/ref/archive protection and Git
  whitespace are checked separately below.

### Final documentation validation results

| Check executed now | Result |
|---|---|
| Relative documentation/evidence link targets | PASS, 282 existing targets across the seven changed/new documents |
| Coverage inventory | PASS, 65 unique claim IDs covering all 14 requested areas |
| Release history | PASS, 18 commits from v0.2.0 to unchanged v0.3.0 |
| Current UI text / viewport / asset-manifest cross-check | PASS; Backup warning/confirmation, fixed-scale viewport and all four asset hashes/sizes checked |
| Composer/asset tests | PASS, 15 cases; temporary deterministic outputs only |
| Notice/documentation tests | PASS, 11 cases; repeated after final README edit |
| Candidate packaging tests | PASS, 11 cases; synthetic fixtures only |
| Infrastructure tests | PASS, six cases |
| Frontend runner | PASS, including 84 live integration groups, Backup/lifecycle cases and 12 Status groups |
| Production/config/dependency/generated-asset edits | NONE; only the seven documentation files differ from the start-of-task snapshot |
| Existing binaries/ELFs/build receipts | PASS, 24 protected artifact hashes unchanged |
| Reviewed source and notice archives | PASS, both original archive checksums unchanged |
| Frozen tag/HEAD/stash/index | Unchanged refs; empty staging area |
| Git whitespace | PASS; existing LF/CRLF warnings did not change Git configuration |
| Publication preflight at documentation-only checkpoint | Inventory then blocked; resolved by the packaging follow-up below. A distinct clean publication commit remains required |
| Firmware builds / relinking / physical tests | NOT rerun for Markdown-only changes; earlier results are attributed to the preserved records |

Local verification details/logs are under ignored
`dist/documentation-audit-20261101/`; no device logs, release archives or historical
artifacts were overwritten. This audit adds no runtime or build-configuration changes.

## Release-readiness and manual confirmations

| Area | Status / next action |
|---|---|
| Frozen application/hardware functionality | Accepted baseline; no runtime change authorized or made |
| NVS Save/Delete/Backup/reboot | Accepted normal operation; actual SDK fault-injection results recorded, no physical power-cut certification |
| AX200 driver/workaround | Exact environment/workaround confirmed by user; no further firmware patch inferred |
| Unit migration | rc.1 simplified test accepted; approve/check final stable bundle separately if claiming final-artifact migration |
| Base | Experimental; separate migration and electrical behavior not accepted as stable |
| Interrupted serial upgrade/recovery | Validate physically before advertising proven recovery; no atomic whole-upgrade promise |
| Upper flash, RS-485 waveform, shutdown-zero wire result | Unverified; external measurements needed for stronger claims |
| Accessibility | Fixed-scale limitation disclosed; full accessibility audit remains optional future work, not claimed PASS |
| Source/notices/relink | Completed local validation; deliver paired materials with actual published binaries |
| Publication identity | Later reviewed clean packaging commit still needed; do not move v0.3.0 or rebuild/replace accepted images implicitly |
| New documentation in packaging | Reconciled by the 2026-11-01 packaging follow-up: these three exact paths are fingerprinted and required in profile bundles; no docs/ wildcard |
| Final GitHub artifacts | No stable ZIPs/hosted release created here. Review/upload exact hashes, notices, source and final release-note links under separate authorization |

GitHub release text uses repository-relative links in this draft. When publishing,
resolve them against the reviewed publication commit (or permanent repository URLs)
and attach named, checksum-verified artifacts; do not publish broken relative links
from a release-page context or claim missing downloads exist.

## Packaging follow-up — resolved document inventory

The focused 2026-11-01 packaging reconciliation now registers precisely
`KNOWN_ISSUES.md`, `docs/V0_3_0_DOCUMENTATION_COVERAGE.md` and
`docs/PRESET_EXPORT_IMPORT.md` in both the publication fingerprint and sparse
bundle documentation inventory. Canonical bytes, individual manifest hashes,
all-member checksums and exact-member rejection are tested for Unit and Base.
Seven local/untracked investigation catalog entries above are intentionally plain
text references: their files are preserved, not packaged or advertised as public
links. Other evidence links retain repository context; they do not pull linked
reports into a firmware ZIP.

The earlier document-inventory block is resolved, **not bypassed**. Clean committed
publication provenance remains mandatory; the unchanged firmware tag cannot
contain later documentation. See the [complete reconciliation/commit sequence](V0_3_0_PUBLICATION_READINESS.md#final-documentation-inventory-reconciliation--2026-11-01).
No final stable bundle, source replacement, firmware change or new tag is produced.
