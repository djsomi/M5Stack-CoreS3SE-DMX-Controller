# v0.3.0 installation and migration

This describes installation of a newly built, verified v0.3.0 stable bundle.
Local source closure does not publish binaries. Original `0.3.0-rc.1` archives
and their checksums remain historical candidates and must not be relabeled.
The simplified Unit v0.2 migration and final preset functionality have user-
reported physical acceptance. Separate Base migration and interrupted-upgrade
recovery remain unverified. Follow the manifest of the actual bundle used.

Choose exactly one bundle:

- **UNIT:** CoreS3SE + Unit DMX U183 + Battery Bottom; recommended hardware.
- **BASE-EXPERIMENTAL:** CoreS3SE + DMX Base + external PSU; historical fixture
  glitches remain under investigation. Unit acceptance does not certify Base.

Both include Wi-Fi. There is no separate no-Wi-Fi image. Both target ESP32-S3;
the flasher cannot detect which accessory is attached. Check the manifest profile
and the device's UNIT DMX / DMX BASE startup label. Do not rely on the chip ID
to prevent a wrong-profile installation.

## Before either installation path

1. Verify the ZIP checksum, extract it into its own directory, and check every
   entry in SHA256SUMS.txt (`sha256sum -c SHA256SUMS.txt`, or compare PowerShell
   `Get-FileHash -Algorithm SHA256` results). Do not mix components from bundles.
2. Check the actual board has 16 MiB flash and confirm reliable USB/ROM recovery.
   Use normal production firmware, not a diagnostics build. Keep known working
   power; the Unit needs its Battery Bottom for external Unit power.
3. Back up any existing device before writing. Full flash/NVS backups contain
   private data: keep them off GitHub. Close serial monitors and select the actual
   port. The bundle's generated table and exact command follow these instructions.

## Fresh Install

Program the four separate image/address pairs in INSTALL.md. On blank NVS the
application initializes defaults. No whole-chip erase is implicit or required.
If unknown/corrupt prior storage needs destructive clearing, that is a separate
explicit operation after backup, not the normal installation or upgrade path.
No erase command is supplied as part of the normal procedure.

## Upgrade from public v0.2.0 — simplified Unit acceptance passed

First follow MIGRATION-TEST.md on a controlled device with saved data. Confirm
the old table uses NVS at **0x9000, size 0x5000**. Back up flash and NVS, record
all presets/settings, then write the four matching files individually:

| Component | Flash offset |
|---|---:|
| bootloader.bin | 0x0000 |
| partitions.bin | 0x8000 |
| boot_app0.bin | 0xe000 |
| firmware.bin | 0x10000 |

The image validator checks actual images and 4 KiB erase rounding. These
separate ranges exclude NVS `[0x9000,0xe000)`. The partition-table sector ends
at 0x9000. OTA metadata is intentionally reset to boot app0. First application
boot adds Wi-Fi records, so compare legacy keys afterward rather than demanding
whole-NVS byte equality. Preset `dmxpreset`, metadata `dmxmeta` and UI `dmxui`
schemas are unchanged. A backup is still required; SDK startup error recovery
may format broken/incompatible NVS.

**Do not flash a padded FULL.bin at zero:** public v0.2.0 FULL images contain
0xff over NVS and erase it. App-only flashing does not replace the partition
table or establish the new 16 MiB layout. This procedure is serial migration,
not OTA. The application contains all web assets; no filesystem image is needed.

## First boot, web access and recovery

In Sender Settings read the current SSID/password. With no stored credentials,
SSID defaults to `CoreS3SE-DMX-XXXX`; password is `espdmx-xxxx` (AP MAC suffix).
Join that SoftAP and open **http://192.168.4.1/**. It works offline; DHCP does
not advertise the device as an internet router. Cellular routing depends on the
client. Four stations are configured; do not equate station count with WS clients.

Change Wi-Fi in web Settings, confirm in the browser, then approve the exact
proposal with physical OK within 60 seconds. CANCEL/timeout before approval
discards it. Applying disconnects clients; rejoin the new network. Blank New
password preserves the current password. Failed application attempts recover
the previous credentials; inspect the local error if recovery fails. The physical
two-second RESET WIFI action changes Wi-Fi on the next explicit restart;
it does not erase presets/settings. The separate Wi-Fi ON/OFF control requires
a one-second hold, persists across reboot and remains accessible with radio OFF.

Factory passwords are predictable. Change them before shared use. WPA2 association
is not a separate authorization mechanism for DMX/maintenance; HTTP/WS is local
and unencrypted at the application layer. No STA/AP+STA or authenticated OTA
updater is provided.

If boot fails, hold CoreS3SE RESET until the green download-mode indicator,
select the re-enumerated USB port, and retry a verified matching four-image
bundle without erasing NVS. Validate this recovery on the test device. A downgrade
requires the matching old bootloader/table/OTA-init/app, or an explicitly approved
private backup restore. The old public FULL is not a data-preserving rollback.
Neither an empty second OTA slot nor SDK early confirmation proves automatic
application-health rollback.

## Preset compatibility and downgrade

Upgrade leaves the NVS partition at 0x9000/0x5000. Existing legacy presets and
settings remain readable. Import adopts an atomic generation-backed preset
collection. A later mutation may reclaim only redundant p1–p8 keys after the
selected collection is verified; unrelated namespaces are untouched. Export a
JSON backup before downgrading. v0.2 cannot read generation banks or depend on
reclaimed stale blobs. Do not erase NVS to resolve a storage-full error.
