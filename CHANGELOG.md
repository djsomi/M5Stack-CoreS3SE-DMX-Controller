# Changelog

## v0.3.0 — 2026-11-02

Changes since public **v0.2.0** (`32c0571`). The cleaned firmware is
hardware-accepted. Binary manifests identify the committed source; hosted
publication and tag correction remain separately authorized steps.
See [installation](docs/V0_3_0_INSTALLATION.md) and [known issues](KNOWN_ISSUES.md).

### Installation changes

- **Upgrading from v0.2.0 requires a complete flash erase.** Presets, device
  settings and Wi-Fi configuration are reset. Old preset/NVS migration has been
  removed; no automatic runtime erase or conversion was added.
- A normal **application-only update of the tested current-format v0.3.0** keeps
  presets and settings when flash is not erased. Do not use a factory image for
  a data-preserving update. Historical release candidates are not covered merely
  because they display v0.3.0.
- Two complete, Wi-Fi-capable factory images target Unit DMX and experimental
  Base DMX. Each includes bootloader, partition table, OTA initialization and app,
  and is written at 0x0. Both use the 16 MiB layout; no filesystem upload is needed.

### New web interface and Wi-Fi

- Offline SoftAP control at **http://192.168.4.1/** with responsive Editor,
  Presets, Settings and Status views for desktop, tablet and iPhone, including
  tested iOS Home Screen mode.
- Live control of all 512 channels with banked faders, +/- adjustment and direct
  numeric entry. Physical controls and multiple clients share authoritative state.
- Web P1–P8 Load, Save and Delete, overwrite/delete confirmation, occupied-tile
  double-tap loading, and Loaded/Modified tracking.
- Web Blackout ON/OFF using the existing entry-snapshot restoration semantics.
  Edits and loads while blacked out do not energize output; releasing blackout
  restores entry values and discards interim changes.
- **Backup** imports/exports the whole saved P1–P8 collection as version-1 JSON,
  including empty slots, without changing live output. Custom Import confirmation,
  progress dialogs, duplicate protection and useful failure feedback are included.
  Export filenames use local time: `DMX-Presets-YY-MM-DD-HH-mm.json`.
- Shared brightness, screen timeout and volume controls with five stops, live
  preview, save-on-release and physical/web synchronization.
- Browser-confirmed Restart and original Power Off, including USB-powered use,
  with a five-second physical CANCEL countdown.
- Wi-Fi configuration with physical approval, timeout and recovery. Local Wi-Fi
  ON/OFF uses a one-second hold and persists; RESET WIFI uses a separate two-second
  hold and applies after reboot. Wi-Fi OFF leaves local DMX control available.
- Configured four-station SoftAP, station/status telemetry and DHCP router-option
  suppression. Cellular routing was verified on iPhone in the tested setup.
- Automatic stale-connection detection and reconnect, complete-state resync and
  manual Retry. Mutation commands are never automatically replayed.

### Physical UI and usability

- Captured Main faders respond to available raw coordinates without the default
  processed movement dead zone, retaining touch identity/release safeguards.
- Fixed Main-to-Settings/Presets/Precision navigation during kinetic scrolling;
  stale Main rendering no longer overwrites the destination page.
- Aligned Settings sliders/buttons, readable network details, persisted Wi-Fi
  controls and Build Date footer; boot/status information identifies the build.
- Stable mobile navigation, Safari/Home Screen viewport behavior, focus and bank
  gestures; compact channel/value feedback and stable blackout/preset buttons.
- Existing Precision Editor/keypad, STEP behavior, physical presets/copy/clear,
  battery warnings, sound, sleep/wake and profile power behavior are retained.
  These were already present before v0.3.0, rather than new web-era features.

### Reliability, storage and performance

- One authoritative Controller and serialized command processing for touch/web
  actions. Networking callbacks do not directly mutate Controller or NVS state.
- Immutable DMX transmit packets, qualified 513-byte completion, safe retirement,
  ordered blackout transitions and bounded handling of genuine transport faults.
- Fixed sender-only RS485_CLASH failures with no receiver or on reconnect;
  incomplete frames still fail qualification. This does not resolve Base glitches.
- Fixed Save/Delete failures after Import caused by legacy records consuming NVS.
  The final factory baseline uses current-format generations for every mutation;
  obsolete keys are neither adopted nor automatically erased.
- Two CRC-checked preset generations with atomic selection, bounded inactive-bank
  reuse and interruption recovery. Genuine exhaustion reports storage-full while
  retaining the selected collection; corruption is not silently treated as empty.
- Export captures one coherent stored collection and uses larger bounded chunks
  with responsive progress feedback. JSON compatibility is unchanged.
- Shared PSRAM framebuffer frees approximately 153.6 KB of internal RAM, resolving
  the measured SoftAP startup memory failure. One fullscreen canvas is reused.
- HTTP sub-millisecond send budgets no longer risk becoming unlimited timeouts.
  Bounded WebSocket queues, session generations, validation and recovery remain.
- Deterministic embedded HTML/CSS/Core/UI gzip assets, each under 16 KiB, with
  content hashes. Dependencies and reproducible esp_dmx patches are pinned.
- Useful hardware/status diagnostics remain. Touch-register experiments require
  explicit investigation opt-in and are excluded from normal/standard diagnostics.

### Hardware acceptance and known limitations

- **Recommended:** CoreS3SE + Unit DMX U183 + Battery Bottom. Clean erase/install,
  normal operation, preset Save/Load/Delete/Backup, reboot persistence and a
  no-erase current-v0.3.0 app update were physically accepted by the user.
- **Experimental:** CoreS3SE + DMX Base + external PSU. Builds pass; known random
  fixture glitches remain. Unit acceptance does not establish Base acceptance.
- Keep the Backup browser tab **open and active** until Import/Export completes.
- Change the predictable factory Wi-Fi password before untrusted use. Associated
  clients share control; HTTP/WS has no separate per-user authorization or TLS.
- Windows/Intel AX200 driver 24.20.2.1 can exhibit packet loss/WS instability.
  Verified client workaround: **MIMO Power Save Mode → No SMPS**.
- Slow continuous physical fader coordinate stalls remain a known acquisition
  limitation. Fixed-scale zoom/accessibility limits and occasional synchronous
  TCP send stalls are documented, not claimed fixed.
- External DMX electrical/timing measurements, shutdown-zero wire guarantees,
  interrupted-install recovery and OTA health rollback remain unverified. OTA,
  LittleFS, STA/AP+STA and wireless DMX are not implemented features.

### Source and notices

- Original **Copyright (c) 2022 M5Stack** attribution and third-party notices are
  retained. Required corresponding source/build/relink materials must accompany
  binary publication; old RC/source archives are not relabeled as this release.
- Public v0.1.0, v0.1.1 and v0.2.0 history is unchanged. v0.2.0 already introduced
  the Precision Editor; skipped battery-threshold warnings were fixed in v0.1.1.
