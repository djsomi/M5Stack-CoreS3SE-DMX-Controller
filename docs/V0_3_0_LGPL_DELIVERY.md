# Corresponding source and Arduino relinking

Use the actual release source commit in `BUILD_INFO.json`; the old frozen
application/tag and local validation archives do not identify the cleaned release.
Dependency versions and reviewed source materials are unchanged by the preset
compatibility cleanup.

Arduino-ESP32 2.0.16 is statically linked under LGPL-2.1-or-later. Distribution
uses Section 6(a)'s application-source option and Section 6(d)'s equivalent
download access to the materials alongside the firmware download. This is not a
Section 6(c) written offer. Recipients may modify the library, rebuild for their
own use and reverse engineer to debug those modifications. Project MIT terms
remain; third-party components retain their own terms. Preserve the full LGPL
and other texts in `licenses/THIRD_PARTY_LICENSES.txt`.

## Required delivery

- Complete matching application/build inputs: src/, include/, web source and
  generated assets, platformio.ini, partition table, all build hooks/helpers and
  reproducible esp_dmx patch source. Keep the genuine source revision available
  for the build provenance hooks. Do not include stashes, reflogs, backups or logs.
- Exact Arduino core and headers, esp32s3 variant, Preferences/SPI/Wire sources,
  package/build files, S3 SDK headers/static archives/linker files and qio_qspi
  configuration used by PlatformIO package 3.20016.0.
- M5Unified 0.2.21, M5GFX 0.2.28 and patched esp_dmx 2.0.2, including required
  headers/source-file notices and original license texts. The patch hooks remain
  idempotent and verify the exact installed driver revision.
- espressif32 6.7.0 PlatformIO build recipes/board definitions and documented
  external compiler/build tool versions/acquisition information. The SDK identifies
  itself as v4.4.7-dirty; do not falsely identify opaque prebuilt SDK archives as
  an exact clean upstream source commit.
- Reviewed dependency file hashes/classifications and complete notice/font/codec
  material under licenses/source_materials/. SDK archives named by the unchanged
  linker recipe must remain even when linker garbage collection discards code.
  The [inventory](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md) records exact exclusions.
- Build and relink instructions, including the application-source route to
  recompiling/relinking Arduino. A final ELF alone is not offered as relinkable
  object material. The paired source and notices must remain downloadable with
  the firmware; upstream availability alone is not treated as the delivery.

The reviewed dependency archive may be reused unchanged, but replace/supplement
its old application snapshot with the final committed source and update the
source/checksum instructions. Historical frozen-source helpers intentionally
remain pinned to their original source; do not relax them to relabel an archive.
No new source-package/relink result is claimed merely because firmware builds pass.

## Build the matching application

With the documented toolchain/packages available, use the final source commit
from BUILD_INFO.json (not the obsolete local tag before its approved correction):

```powershell
git clone https://github.com/djsomi/M5Stack-CoreS3SE-DMX-Controller.git CoreS3SE-DMX-source
cd CoreS3SE-DMX-source
git checkout --detach <source_commit-from-BUILD_INFO.json>
pio run -e cores3se_unit -e cores3se_base
```

The PlatformIO environments pin espressif32 6.7.0 / Arduino 2.0.16, M5Unified
0.2.21, M5GFX 0.2.28 and esp_dmx v2.02. Use the supplied exact dependency/build
materials for offline reconstruction. A source ZIP without Git metadata needs
its matching genuine revision restored before running the provenance hook.
See [installation](V0_3_0_INSTALLATION.md) for merging the four actual components.

## Modified-library validation

Run only in an isolated source/core copy. Build both production profiles first.
For a harmless link-retention probe, the previously exercised Arduino
`cores/esp32/Print.cpp` change adds a referenced marker inside the retained
`Print::printf(const char*, ...)` implementation:

```cpp
static const char lgpl_relink_probe[] = "LGPL_RELINK_TEST";
__asm__ __volatile__("" : : "r"(lgpl_relink_probe) : "memory");
```

Rebuild Unit and Base. Check that the marker is absent in baseline ELF, present
in modified Print.cpp.o, libFrameworkArduino.a and final ELF, and that Print.cpp.o
has a retained map section. Record original/modified source hashes, commands,
versions and link outputs. A probe in a discarded implementation proves only
compilation. Do not upload or substitute these test images for the release.

Modified-Arduino relinking previously passed for Unit/Base with the unchanged
dependency configuration; it is historical evidence, not a newly performed
modified-library test for this final source commit. Build dates/paths/versions
can change bytes, so this is not a promise of bit-identical reconstruction.
Automated checks establish inventory integrity and tested relinkability, not an
independent legal opinion. Preserve all upstream notices and license terms.
