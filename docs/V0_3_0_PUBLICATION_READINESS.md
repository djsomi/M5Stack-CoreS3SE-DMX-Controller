# v0.3.0 source and binary distribution

The release source is the commit recorded in `BUILD_INFO.json`. Unit and Base
production images are rebuilt from that committed source. Finalization does not
move tags, push, flash hardware or create a hosted release.

## Firmware and installation

- `CoreS3SE-DMX-Unit-v0.3.0.bin` and `CoreS3SE-DMX-Base-v0.3.0.bin` are complete
  factory images at 0x0: bootloader 0, table 0x8000, OTA initialization 0xE000,
  application 0x10000. Original component bytes and checksums are verified.
- From v0.2.0, erase flash; old presets/settings are lost. There is no raw-NVS
  migration. Factory images include the NVS gap and can overwrite data.
- The tested current-format v0.3.0 application-only update at 0x10000 without
  erase preserves presets/settings. See [installation](V0_3_0_INSTALLATION.md).
- Unit is recommended; Base is experimental. Diagnostics are developer builds,
  not a third normal release variant. Web assets are embedded; no filesystem image.

## Notices and corresponding source

[LICENSE](../LICENSE) preserves the original Copyright (c) 2022 M5Stack MIT notice
verbatim and the additional project notice. [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md)
and [licenses/manifest.json](../licenses/manifest.json) cover the exact installed
M5Unified 0.2.21, M5GFX 0.2.28, esp_dmx 2.0.2, Arduino-ESP32 2.0.16 and relevant
SDK/runtime components. M5GFX has BSD/font/codec terms beyond top-level MIT.
Original source-file notices and full license texts must remain intact.

The notice verifier checks 24 audited notice groups and installed source/header
hashes. The expanded [source-material inventory](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md)
checks 112 component groups and 37 source/notice records. Components supplied as
build material are distinguished from code actually linked into the firmware.

Static Arduino linking uses the documented corresponding-source/relink delivery
approach in [the LGPL guide](V0_3_0_LGPL_DELIVERY.md). Publish matching application,
Arduino/dependency/SDK sources and build material with their notices and build/
relink instructions alongside the binaries. An MIT LICENSE or source link alone
is not substituted for that reviewed delivery. Old application archives identify
their original source and must not be relabeled as the cleaned release.

## Final publication checklist

1. Review the source commit and clean production receipts; compare source hash,
   hardware profile, component offsets, merged contents and SHA-256 sums.
2. Keep the detailed [changelog](../CHANGELOG.md), installation instructions and
   [known issues](../KNOWN_ISSUES.md), including password, Backup foreground,
   AX200, accessibility and synchronous-send/physical-touch limitations.
3. Deliver matching source/build/relink and license material for this commit.
   Reuse unchanged reviewed dependencies; do not bypass old source-specific gates.
4. The old local v0.3.0 tag remains unchanged during finalization. Recheck all
   remotes; only after explicit approval correct an unpublished local tag to this
   source commit. Never force-update an existing public tag without a new decision.
5. Only after separate approval, publish the source/history, final tag and hosted
   assets. GitHub's automatic source archive follows the final tag. Local history,
   RCs, logs, backups and private development files are not release attachments.
