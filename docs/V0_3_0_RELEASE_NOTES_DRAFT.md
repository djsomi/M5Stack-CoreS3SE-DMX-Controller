# v0.3.0 — local stable release notes

**Local stable source closure — not published.** The user accepted Preset
Save/Delete, Backup Import/Export, reboot persistence and final UI on the real
controller. Simplified Unit migration was previously accepted. Separate Base
migration, interrupted-upgrade recovery and electrical measurements remain pending.
This historical filename is retained to preserve existing links; it is no longer
an instruction to distribute the old rc.1 archives.

- Complete offline, responsive web DMX channel control with live authoritative
  synchronization, faders, +/- fine adjustment and numeric input.
- SoftAP networking and embedded HTTP/WebSocket assets; automatic stale-connection
  recovery without replaying mutation commands.
- P1–P8 preset load/save/delete with overwrite/delete confirmation, loaded/modified
  tracking, and synchronized blackout with the existing pre-blackout restoration.
- Device Settings previews and release-time persistence; Restart/Power Off browser
  confirmation and five-second physical CANCEL. The established hardware Power
  Off path remains, including USB-connected use.
- Wi-Fi Configuration with physical OK/CANCEL, 60-second expiry, verified credential
  persistence/recovery, local password readout and next-boot Wi-Fi factory reset.
- Responsive four-card Settings, iPhone/Home Screen navigation and gesture polish;
  deferred maintenance intents invalidate synchronously on leaving Settings.
- Authoritative Controller/Dispatcher and immutable qualified DMX transmit handling,
  including the confirmed sender RS485_CLASH correction. Transport counters do not
  substitute for external electrical measurements.
- 16 MiB layout, two reserved 6 MiB app slots, PSRAM framebuffer and embedded gzip
  assets. No filesystem upload is needed. Two OTA slots do not implement OTA.

## Preset persistence and Backup fixes

- Fixed real SDK NVS exhaustion (`0x1105`) reproduced when imported generations
  coexisted with obsolete legacy `p1`–`p8` blobs in the existing 20 KiB partition.
- Only after a selected generation passes full size/header/version/CRC validation,
  the next bank-backed mutation reclaims those eight redundant keys. Initial
  adoption, read-only Load/Export, active banks/selector, unrelated settings and
  credentials are protected. Dual generations and atomic selector remain intact.
- Save/Delete work after import and reboot, with bounded bank reuse. Genuine
  exhaustion gives explicit storage-full feedback without destructive reset.
- Backup uses the unchanged version-1 JSON schema. Export downloads
  `DMX-Presets-YY-MM-DD-HH-mm.json` using browser-local date/time.
- Import has custom whole-P1–P8-collection confirmation without a preset count,
  Import/Cancel controls, animated progress through the backend result, duplicate
  protection and success/error feedback. Import never loads a live preset.
- Confirmation and Import/Export progress warn to keep the tab open and active.
  Existing visibility handling suspends the WebSocket and cancels transfers;
  foreground reconnect does not replay them. That behavior was not changed.
- Accepted prerequisites retain captured raw-coordinate fader input, fix stale
  Main rendering after fullscreen navigation, isolate touch investigations and
  provide persistent local Wi-Fi ON/OFF with existing recovery controls.

## Validation and hardware acceptance

The user reports all manual Preset Save/Delete, Import/Export, reboot-persistence
and final UI acceptance tests passed on the actual controller. Do not interpret
this as physical power-cut, raw flash-error or Base migration validation.
Automated closure covers actual IDF 4.4.7 NVS, dual-generation/selector lifecycle,
legacy cleanup, failure/corruption/capacity/interruption cases, v0.2 compatibility,
frontend/browser/recovery, deterministic assets and all four firmware profiles.
See the [closure record](V0_3_0_LOCAL_CLOSURE.md) for final results and inventory.

## Hardware and installation

Recommended: CoreS3SE + Unit DMX U183 + Battery Bottom. Base DMX + external PSU
remains **experimental**; historical random fixture glitches remain under review.

Stable distribution requires two newly verified Wi-Fi-capable profile bundles, each with separately addressed
bootloader, partition table, OTA initialization and application files, manifest,
checksums and instructions. There is no Wi-Fi/no-Wi-Fi split and no padded FULL.
Use [installation instructions](V0_3_0_INSTALLATION.md) and the
[migration acceptance plan](V0_3_0_MIGRATION_ACCEPTANCE.md).

v0.2.0 uses an 8 MiB layout; v0.3.0 uses 16 MiB. NVS location/size and
legacy preset/settings schemas are unchanged before generation adoption. Selective serial upgrade is designed
to avoid NVS; the user reports preset/settings retention and reboot passed the
simplified Unit procedure. Interrupted-upgrade recovery and separate Base migration
are unverified. App-only flashing cannot
migrate the table. Public old FULL images erase NVS when flashed at zero.

## Known limitations

Slow continuous touchscreen fader movement may exhibit intermittent coordinate stalls originating at or before the touch-controller acquisition layer. Historical upstream code contains the same relevant acquisition mechanisms. No application-level cause was demonstrated. The behavior is not proven to have occurred on the original firmware.

The captured raw-coordinate correction is retained. Read-only acquisition tests
found THDIFF changing from 0xA0 to 0xC8 in association with touch activity; the
exact cause and any relationship to stutter remain unknown. THDIFF writes and
the A–B–A experiment are suspended. Standard builds do not include the experiment.

- No OTA updater or validated application-health rollback; no STA/AP+STA.
- Factory AP passwords are predictable MAC-derived defaults. Change them through
  the physical-approval flow. Associated clients share control; local HTTP/WS is
  not an authenticated/encrypted administrative channel beyond WPA2 association.
- A slow/lost Wi-Fi client can still block a native synchronous send temporarily;
  backpressure/recovery exists, not a claim of nonblocking socket delivery.
- No automatic Unit/Base accessory detection or wrong-profile flash interlock.
- Physical slider precision, automatic-reboot/countdown observation, upper-flash
  write/alias checks, shutdown-zero guarantees and external DMX measurements remain
  documented limitations or deferred investigations.

Export a JSON backup before downgrading: v0.2 cannot read generation-backed edits
and cannot rely on obsolete p1–p8 records after reclamation. Retaining NVS during
upgrade is not a promise of reverse-firmware preset compatibility.

Original rc.1 archives/checksums are preserved and are not stable artifacts.
Publication requires new matching clean-tag builds/bundles, verified checksums
and separate publication authorization. No hosted release or upload is part of
this local closure.
