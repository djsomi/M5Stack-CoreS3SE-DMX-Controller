# M5Stack CoreS3SE DMX Controller v0.3.0

**GitHub release draft — not published.** Frozen hardware-accepted firmware:
annotated `v0.3.0`, source `59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`.
Changes below compare with public v0.2.0. Later publication/notice work has its
own provenance; it does not alter that tag or the accepted firmware binaries.
The historical draft filename is retained for existing links.

## Before installing

- **Recommended:** CoreS3SE + Unit DMX U183 + Battery Bottom.
- **Experimental:** CoreS3SE + DMX Base + external PSU. Observed random fixture
  glitches remain unresolved; Unit tests do not certify Base.
- **Change the predictable factory Wi-Fi password before untrusted use.** WPA2
  association is the current control boundary, not per-user authorization.
- v0.2.0 uses an 8 MiB layout; this release uses 16 MiB. Follow the
  [installation/migration guide](V0_3_0_INSTALLATION.md). **Do not use app-only
  flashing to migrate or a padded FULL image to preserve NVS.**

## New web control

Control the complete 512-channel universe offline at **http://192.168.4.1/**.
Editor provides banked faders, +/- fine adjustment and direct numeric entry.
Physical controls and multiple browser clients receive authoritative state,
validated snapshots and ordered updates. Reconnect obtains a fresh snapshot and
never automatically replays a mutation. Manual Retry remains available.

P1–P8 support Load, Save and Delete, explicit overwrite/delete confirmation,
occupied-tile double-tap loading and shared Loaded/Modified indication. Blackout
is synchronized across clients. While ON, output is zero; edits/loads do not
energize fixtures. Turning it OFF restores the exact entry snapshot and discards
interim edits/loads, preserving the existing Controller semantics.

Responsive dark Editor, Presets, Settings and Status views support desktop,
tablet, iPhone Safari and the tested iOS Home Screen mode. Stable bottom navigation,
internal `100svh` scrolling, safe areas and immediate tab changes avoid the earlier
navigation jumps. New tabs start at the top; reselecting the active tab preserves
position. Compact header, channel feedback and stable blackout/preset action
buttons avoid routine pending-state flashes. The lower blackout ON status is red.
Browser preferences, including secondary percentage, remain browser-local.

**Accessibility qualification:** the accepted viewport is fixed-scale and limits
pinch zoom. Keyboard/focus regression checks are not a full accessibility audit.
See [known issues](../KNOWN_ISSUES.md).

## Presets, persistence and Backup

The **Backup** card exports/imports version-1 JSON for the entire saved P1–P8
collection, including empty slots; live output is unchanged and import never
implicitly loads a preset. Import uses a custom confirmation without a misleading
preset count, animated progress through the backend result, duplicate protection
and success/failure feedback. Export uses the browser's local time:

`DMX-Presets-YY-MM-DD-HH-mm.json`

**Keep this browser tab open and active until the operation completes.** The
warning appears in Import confirmation and both progress dialogs. Lifecycle
handling intentionally suspends the WebSocket and cancels transfers on backgrounding;
foreground recovery does not replay them. If the import response is lost near
commit, inspect/export the current collection before retrying.

Fixed Save/Delete failures after an earlier import under constrained NVS.
Legacy `p1`–`p8` blobs previously coexisted with imported generation banks and could
prevent allocation of the next collection. Reclamation now occurs only after
validating the authoritative selected generation; only those redundant eight
keys are removed. Two fixed banks, CRC/readback validation and atomic selection
are retained. Active data, unrelated settings and Wi-Fi credentials are protected;
real exhaustion gives explicit storage-full feedback instead of erasing NVS.

Export now captures one coherent collection and uses bounded, larger download
chunks with progress feedback. Import keeps strict validation, conflicts,
session ownership and rollback protections. The JSON format is unchanged.
Existing v0.2 records remain readable; **export before downgrading**, because old
firmware cannot read generation-backed edits or depend on reclaimed legacy blobs.

## Device Settings, Maintenance and Wi-Fi

- Brightness, volume and screen timeout have five discrete stops, live preview,
  commit-on-release persistence and physical/web synchronization. Always On matches
  the device order. Successful changes are visually quiet; persistence errors remain.
- Web Restart/Power Off require browser confirmation, then offer a five-second
  **physical CANCEL** window. Power Off invokes the original working shutdown path,
  including USB-connected operation; no artificial USB/battery restriction.
- Settings uses four independent cards: Device Settings / Device Maintenance,
  then Wi-Fi Configuration / Browser Preferences, paired on desktop and stacked
  on mobile. Confirm styling is green; initial maintenance buttons remain neutral.
- Wi-Fi credential changes require physical OK/CANCEL within 60 seconds. Blank
  New password keeps the current one. Verified journaling/persistence supports
  recovery; applying changes disconnects clients so rejoin with the new credentials.
- Local touchscreen Wi-Fi ON/OFF uses a one-second hold and persists, defaulting
  ON for existing installations. OFF stops the radio and HTTP/WS; DMX/local control
  continue. Local re-enable remains accessible. RESET WIFI uses a separate
  two-second hold and takes effect after reboot without erasing presets.
- Physical Settings aligns its sliders/buttons, shows current network details and
  moves Build Date to the final scrollable footer. Build/profile/source identity
  is available at boot and in Status.

## Physical control and DMX reliability

Captured Main-fader input uses raw coordinates after ownership capture, removing
the default processed movement dead zone without global threshold changes,
smoothing or geometry changes. Slow continuous upstream coordinate stalls remain
a deferred limitation, not a claimed fix. Main-to-fullscreen navigation now
cancels kinetic momentum and prevents stale Main drawing after Settings, Presets
or Precision takes ownership; Main scroll position is retained.

The authoritative Controller and serialized Dispatcher unify physical/web state.
The transport uses immutable active packets, qualified full 513-byte completion,
safe retirement and ordered blackout barriers. The confirmed sender RS485_CLASH
no-receiver/reconnect regression is corrected without accepting incomplete frames
or changing receiver behavior. This is separate from experimental Base glitches.
Physical Precision/keypad, preset copy/clear, battery warning priority, sleep/wake,
sound and existing hardware power behavior are retained from earlier releases.

## Infrastructure and resource improvements

- SoftAP-only native HTTP/WebSocket, four-station configuration and DHCP
  default-router suppression; iPhone cellular routing worked in the tested setup.
- One shared 153,600-byte framebuffer explicitly in PSRAM recovered approximately
  153.6 KB internal RAM, resolving the observed Wi-Fi initialization memory failure.
- Bounded validated requests/outboxes, fair drain scheduling, session generations,
  revision/snapshot ordering, lightweight metrics and first-failure diagnostics.
- Idle liveness probes after 15 seconds, with a ten-second deadline and one
  outstanding probe; healthy probes keep LIVE steady. Stale sockets reconnect
  through the existing state machine and require a complete snapshot.
- HTTP budgets below one millisecond are rejected before lwIP can truncate them
  to an unlimited socket timeout. Existing absolute deadlines/byte budgets remain.
- Deterministic embedded HTML/CSS/Core/UI gzip assets and hashes, each below
  16 KiB; no filesystem image. Final compressed sizes: **5,360 / 5,411 / 13,325 /
  15,486 bytes**, respectively.
- Pinned espressif32 6.7.0, Arduino-ESP32 2.0.16, M5Unified 0.2.21, M5GFX 0.2.28
  and esp_dmx v2.02 with reproducible project patches. v4.1.0 was not adopted.
- Stage 0 diagnostics remain; temporary touch investigations require separate
  explicit opt-ins. THDIFF experiments are suspended, not part of standard builds.

## Acceptance and verification

User-reported physical acceptance covers Unit DMX, active web control,
multi-client synchronization, settings/maintenance/Wi-Fi, Backup, Save/Delete and
reboot persistence. An accepted snapshot had **four associated stations and three
active WebSocket sessions**, with zero reported send failures, overflows,
Wi-Fi disconnects and DMX TX errors. This is not a four-WS-client benchmark.

The closure record reports frontend/browser/host/integration regressions,
Controller/Dispatcher/transport/ISR tests, all four firmware configurations,
deterministic assets and actual ESP-IDF 4.4.7 NVS tests. Those include 48 repeated
Save/Delete cycles and 375 injected write/erase/error cases. These are recorded
automated results, not physical power-cut/electrical qualification. See
[complete coverage](V0_3_0_DOCUMENTATION_COVERAGE.md) and [closure](V0_3_0_LOCAL_CLOSURE.md).

Simplified **Unit v0.2.0 → rc.1** migration retained seeded P1–P8/non-default
settings and reboot persistence. Separate Base migration, final stable-bundle
migration, interrupted-upgrade recovery, preboot NVS byte equality, upper-flash
write/alias and external RS-485 measurements are not established by that test.

## Known issues and operational cautions

- Windows + Intel Wi-Fi 6 AX200 driver **24.20.2.1**: reported packet loss and
  WebSocket instability; user-verified workaround is **Device Manager > Intel
  Wi-Fi 6 AX200 > Advanced > MIMO Power Save Mode > No SMPS**. Classified as a
  client compatibility issue, not a confirmed firmware defect.
- Factory MAC-derived password is predictable. Associated clients share control;
  no separate authenticated/encrypted HTTP/WS administration, STA/AP+STA or OTA.
- Historical synchronous sends can stall the shared HTTP task; recovery/backpressure
  is implemented, not a permanent nonblocking TCP fix. No queue enlargement workaround.
- Fixed-scale accessibility limits, slow touchscreen coordinate stalls, Base
  glitches, shutdown-zero wire guarantees and an unexplained update/reboot countdown
  observation remain documented. No automatic Unit/Base accessory interlock.
- Two 6 MiB app slots do not provide an OTA updater or validated health rollback.
  Reserved filesystem space is not an implemented LittleFS feature.

Read [KNOWN_ISSUES.md](../KNOWN_ISSUES.md) before installation or live use.

## Installation and publication materials

Planned distributions are **CoreS3SE-DMX-v0.3.0-UNIT.zip** and
**CoreS3SE-DMX-v0.3.0-BASE-EXPERIMENTAL.zip**, each Wi-Fi capable, using separately
addressed bootloader/table/OTA-init/application components, manifest, checksums
and instructions. There is no Wi-Fi/no-Wi-Fi split and no padded FULL image.
NVS stays at `0x9000`, size `0x5000`; sector-rounded image ranges exclude it.
Select the matching hardware profile; app-only flashing cannot migrate partitions.

Project/upstream attribution and third-party texts accompany distribution.
Corresponding frozen application/Arduino/dependency/build materials, documented
reconstruction and modified-library relinking are locally validated for Unit and
Base. The reviewed source/notice archives and inventory are described in
[source delivery](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md) and
[LGPL guide](V0_3_0_LGPL_DELIVERY.md); passing tests alone is not a legal opinion.

Final packaging must preserve accepted binary checksums and record **both** the
frozen firmware commit/fingerprint and a later clean publication/packaging commit,
plus source/notice archive checksums. No new firmware build or tag replacement is
required merely for these documentation changes. Existing RCs/validation archives
are not stable firmware bundles; do not relabel them. This draft does not claim
that archives have been published or authorize upload, merge or release creation.
