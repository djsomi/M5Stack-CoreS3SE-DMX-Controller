# Changelog

## Publication-readiness supplement (uncommitted; no new version)

- Reconciled the complete original Copyright (c) 2022 M5Stack MIT notice with
  the existing project notice; added exact dependency/subcomponent license texts
  and linked-versus-unused inventory for the frozen v0.3.0 firmware.
- Added deterministic notice packaging/checksums and missing/tampered notice
  checks without changing sparse flashing ranges, NVS behavior or firmware.
- Prepared frozen-source/SDK build materials, demonstrated modified Arduino
  library relinking for Unit and Base, and added separate firmware/publication
  provenance preflight. Completed the expanded build-material inventory, exact
  safe exclusions, Boost/SDK/font/codec notices and paired archive validation.
  Local validation archives are not stable firmware release bundles.
- Reconciled completed Preset Backup functionality, security/accessibility/
  performance limitations and portable public documentation paths.
- Existing annotated v0.3.0 tag is unchanged. Follow-up repository history and
  binary distribution/source-delivery provenance remain separately authorized steps.

## v0.3.0 — 2026-10-31 (frozen local source; GitHub publication pending)

Comparison: `v0.2.0` (`32c0571`) → `v0.3.0`
(`59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`). The evidence-backed
[complete coverage matrix](docs/V0_3_0_DOCUMENTATION_COVERAGE.md) links each change
to commits, implementation, tests and acceptance. [Release notes](docs/V0_3_0_RELEASE_NOTES_DRAFT.md)
and [known issues](KNOWN_ISSUES.md) contain installation/security qualifications.

### User-visible additions

- Complete offline web control of all 512 channels: banked faders, +/- fine
  adjustment, numeric entry and authoritative physical/web/multi-client updates.
- Responsive dark Editor, Presets, Settings and Status views; compact mobile
  header, persistent navigation, `100svh` internal scrolling, safe-area handling,
  tab-change scroll reset and browser-local secondary-percentage preference.
- Web P1–P8 Load/Save/Delete with overwrite/delete confirmation, occupied-slot
  double-tap loading, shared Loaded/Modified tracking and stable action buttons.
- Web blackout with confirmed ON/OFF feedback and existing pre-blackout snapshot
  restoration. Edits/loads during blackout do not energize outputs; release
  restores the entry values, discarding interim changes.
- **Backup**: version-1 JSON Export/Import of the entire P1–P8 collection,
  including empty slots. Import does not load live output. Custom confirmation,
  animated non-percentage progress, duplicate protection and backend result/error
  feedback; local-time `DMX-Presets-YY-MM-DD-HH-mm.json` downloads.
- Shared Device Settings: five-stop brightness/volume/timeout controls, live
  preview, release-time persistence and physical/web synchronization. Always On
  ordering matches the device; routine Saved/Applied messages and Refresh removed.
- Web Restart/Power Off with browser confirmation, five-second physical CANCEL
  and stable green Confirm styling. Original Power Off remains, including
  USB-connected operation; no artificial battery/USB restriction.
- SoftAP with embedded HTTP/WebSocket UI at `http://192.168.4.1/`, four-station
  configuration, DHCP default-router suppression and current station telemetry.
- Wi-Fi credential changes with physical OK/CANCEL, 60-second approval expiry,
  verified persistence and recovery. Separate local Wi-Fi ON/OFF (one-second
  hold, persisted, default ON) and RESET WIFI (two-second hold, next reboot).
- Physical Settings slider/button alignment at x=10..240, scrollable network
  details and Build Date footer. Build/version/environment/source identity is
  visible through boot/status reporting.
- Status diagnostics expose current/stale/unavailable readings, memory, station,
  transport and WebSocket health without exposing stored credentials in APIs.

### User-visible fixes and refinements

- Fixed preset Save/Delete after JSON import under constrained NVS: obsolete
  legacy `p1`–`p8` blobs are reclaimed only after validating the selected complete
  generation. No NVS enlargement or destructive reset; real full-storage failures
  retain data and produce explicit actionable feedback.
- Captured physical Main faders use available raw coordinates without the default
  processed movement dead zone, retaining identity/release/cancellation checks.
  Upstream slow-coordinate stalls remain deferred, not claimed fixed.
- Main → Settings/Presets/Precision navigation cancels kinetic momentum and avoids
  stale Main rendering in the same iteration; returning preserves scroll position.
- Improved Safari/Home Screen navigation stability, focus behavior and bank/preset
  gestures. Final accepted fixed-scale zoom restrictions are documented, not
  described as unrestricted pinch-zoom accessibility.
- Restored compact `CH nnn · value` feedback; stabilized blackout header/preset
  action buttons through pending commands, preset loads and acknowledgments.
  Confirmed lower blackout ON status is red; OFF remains muted.
- Automatic stale-OPEN WebSocket detection, fresh-snapshot recovery and manual
  Retry; routine successful health probes keep the LIVE indicator stable.
- Fixed sender-only RS485_CLASH no-receiver/reconnect regression without accepting
  partial frames or weakening retirement checks. This is separate from Base glitches.

### Internal engineering and performance

- Extracted one authoritative DMX Controller and serialized CommandDispatcher;
  networking callbacks never mutate Controller/NVS/display state concurrently.
- Immutable active TX snapshots, full 513-byte qualified completion, safe buffer
  retirement, ordered blackout barriers, bounded retries/fail-stop for real faults;
  sender/receiver separation, baud/break/MAB/pins remain controlled by existing paths.
- Reproducible repository-local patches on esp_dmx v2.02/2.0.2; v4.1.0 was evaluated
  but not adopted. Pinned espressif32 6.7.0, M5Unified 0.2.21 and M5GFX 0.2.28 remain.
- 16 MiB layout replaces the 8 MiB configuration, retains NVS at 0x9000/0x5000,
  provides two 6 MiB app slots and reserves future storage. No OTA/LittleFS feature.
- One 153,600-byte shared framebuffer explicitly in PSRAM recovered approximately
  153.6 KB internal RAM and resolved the measured SoftAP initialization allocation
  failure; unsupported internal fallback is not silently used.
- Native HTTP/WS bounded queues, generation-qualified requests, strict validation,
  snapshot/revision ordering, fair bounded outbox draining and first-failure metrics.
  Rejected sub-millisecond HTTP budgets cannot truncate to unlimited lwIP timeout;
  three-second absolute HTTP deadline and byte limits retained.
- Bounded application liveness probes, lifecycle cleanup and no mutation replay.
  These do not remove the documented synchronous TCP send-stall limitation.
- Dual-generation preset banks, CRC/readback checks and atomic selector; bounded
  inactive-bank reuse and validated legacy cleanup. Corruption/uncertain outcomes
  are not converted to empty presets or silent success.
- Export reads one coherent collection, larger bounded download chunks and
  rate-budget-aware delivery; no artificial percentage progress or JSON changes.
- Deterministic composer and separate Core/UI gzip bundles, content hashes/manifest
  checks, embedded assets and 16 KiB compressed per-asset limit.
- Stage 0 hardware/memory diagnostics, transport/network counters and diagnostics
  isolation. Temporary touch acquisition/snapshot/THDIFF tools need explicit
  investigation opt-ins; no experimental register writes in standard builds.
- Host/integration/browser tests, real ESP-IDF NVS reproduction/fault injection,
  patch-state checks and production/diagnostics Unit/Base build coverage.

### Compatibility, acceptance and distribution

- Existing grid/Precision/keypad, STEP persistence, physical preset copy/clear,
  battery alerts/volume restoration, sleep/wake and Unit/Base startup power behavior
  are preserved from earlier releases; they are not newly introduced here.
- User-reported Unit hardware acceptance covers web/physical control, multiple
  clients, settings/maintenance/Wi-Fi, Backup, Save/Delete and reboot persistence.
  Four stations/three WS sessions were recorded, not four simultaneous WS clients.
- Simplified Unit v0.2.0 → rc.1 migration retained seeded presets/settings and
  reboot persistence. Base migration, interrupted-upgrade recovery, physical
  power-cut tests and external DMX electrical measurements remain unverified.
- Sparse four-image serial installation excludes NVS; app-only flashing cannot
  migrate partitions. Old padded FULL images overwrite NVS. Export before
  downgrading: v0.2 cannot read generation-backed changes/reclaimed legacy blobs.
- Unit recommended; Base experimental. No accessory-detection flash interlock.
- Keep Import/Export tab open and active. Change the predictable factory password
  before untrusted use; associated clients share control over local HTTP/WS.
- Windows/Intel AX200 driver 24.20.2.1 compatibility issue: user-verified
  **MIMO Power Save Mode > No SMPS** workaround; not a confirmed firmware defect.
- Publication supplement supplies notices, corresponding-source/build/relink
  materials and dual provenance. It is separate from the immutable firmware tag;
  old RCs and local source/notice validation ZIPs are not final firmware releases.

Earlier annotated v0.1.0, v0.1.1 and v0.2.0 tags/history remain unchanged.
v0.2.0 already included the Precision Editor/shared-canvas UI; v0.1.1 fixed skipped
low-battery thresholds. No earlier feature or release is retroactively reclassified.
