# Changelog

## v0.3.0 — 2026-10-31 (local source release)

### Added

- Offline Wi-Fi web control, real-time multi-client state, preset management,
  blackout, Device Settings/Maintenance and physically approved Wi-Fi configuration.
- Version-1 JSON preset Backup/Import with whole-collection confirmation, bounded
  progress dialogs and browser-local timestamped download names.
- Persistent local Wi-Fi ON/OFF and explicit build/version/source provenance.

### Fixed

- Post-import Save/Delete NVS exhaustion: reclaim obsolete p1–p8 blobs only after
  validating the authoritative generation; retain dual banks and atomic selection.
- Clear actionable storage-full errors and bounded generation reuse across reboot.
- Main-to-fullscreen navigation during kinetic scrolling; captured fader input
  bypasses the proven processed-coordinate movement dead zone without smoothing.
- Temporary touch investigations are opt-in; THDIFF writes remain suspended.

### Compatibility and validation

- Existing NVS offsets and legacy schemas retained. Export before downgrading:
  old firmware cannot read imported banks or rely on reclaimed legacy records.
- User-reported physical acceptance: Save/Delete, Import/Export, reboot persistence
  and final Backup UI. Automated native-NVS, frontend, integration and four-profile
  firmware checks are recorded in docs/V0_3_0_LOCAL_CLOSURE.md.
- Unit recommended; Base experimental. No OTA updater, wrong-profile interlock,
  proven interrupted-upgrade recovery or external electrical validation is claimed.
- Local commit/tag only; original RC bundles remain unchanged. No hosted release.

Earlier published versions are preserved as annotated v0.1.0, v0.1.1 and v0.2.0
tags. See their existing history/release notes; no earlier tag is rewritten here.
