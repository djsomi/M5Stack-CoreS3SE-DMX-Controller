# v0.3.0 corresponding source and relinking

This is a publication/build supplement, not a replacement firmware. The frozen
firmware source is `59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`, tagged `v0.3.0`;
its build-input SHA-256 is
`cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232`.

Arduino-ESP32 2.0.16 is statically linked under LGPL-2.1-or-later. Distribution
uses Section 6(a)'s application-source option and Section 6(d)'s equivalent
download access to these materials **alongside the firmware download**. This is
not a Section 6(c) written offer. Publish the actual source/build archive and
notices, not merely upstream links. Recipients may modify the library, rebuild
for their own use and reverse engineer to debug those modifications. Project MIT
terms remain; third-party components retain their own terms. See the complete
LGPL and other texts in `licenses/THIRD_PARTY_LICENSES.txt`.

## Delivered materials

- `frozen-source`: original commit/tag objects, all trees and build-input blobs.
  One shallow commit, no parent history, other branches, stashes, reflogs or
  repository configuration. Non-build historical document blobs are intentionally
  omitted. The source reconstruction keeps the genuine Git identity required by
  the frozen build hooks; no new commit is manufactured.
- `core/packages/framework-arduinoespressif32`: complete installed Arduino core,
  Preferences/SPI/Wire libraries, esp32s3 variant, package/build files and actual S3 SDK headers,
  static archives, linker scripts and configuration. Other chips and Windows
  standalone `.exe` upload tools are omitted. Only qio_qspi is retained; unused
  memory modes, IDE/debug helpers and other-board files are omitted. PlatformIO
  uses tool-esptoolpy. No retained source or linker recipe is edited.
  The SDK identifies itself as `v4.4.7-dirty`; a clean v4.4.7 source commit is
  **not** falsely asserted for those prebuilt SDK libraries. The exact delivered
  bytes are hashed in MATERIALS.json. Replacing/recompiling Arduino does not
  require rebuilding this separately licensed SDK.
- `core/platforms/espressif32`: actual 6.7.0 build recipes and board definitions.
- `libdeps`: exact production M5Unified 0.2.21, M5GFX 0.2.28 and patched esp_dmx
  2.0.2. Both repository patch hooks and patch source files are in frozen-source;
  their already-patched/idempotent state is checked during compilation.
  Examples/media and M5GFX's non-ESP desktop picture-frame images are omitted.
- All source-file notices remain in place. Supplied unused M5GFX source/font
  files retain their IPA, efont, Adafruit and other notices; the binary inclusion
  inventory is not a claim that those extra source files are MIT-only.
- `BUILD-TOOLS.json`: exact versions, acquisition commands and per-file hashes of
  external compiler/build tools. Compiler executables are not redistributed in
  this archive. This avoids representing GCC runtime exceptions as a license to
  redistribute the whole compiler without its own source obligations.
- `MATERIALS.json`: exact member lengths/hashes. `tools/` supplies reconstruction
  and verification tools. The archive is independent of the publisher's Git repo.
- `licenses/source_materials/manifest.json`: all 2,818 retained dependency files,
  their component/role/hash, the 1,316 exact exclusions with reasons, 112 component
  groups and complete source-distribution notice files. Full Boost 1.0, Asio's
  actual BSL declarations, SDK subcomponent notices and original source comments
  are included. Independent revisions not recorded by the vendor remain unknown;
  the exact PlatformIO package and file hashes are the artifact provenance.
- The source-only GNU FreeFont data has GPL/font-exception terms, not Adafruit
  BSD alone. Original bitmap headers, full GPL, font exception, author list and a
  complete official FreeFont source supplement are supplied. The outline-source
  supplement is version 20120503; this is not a fabricated version for M5GFX's
  unversioned vendored bitmaps. IPA replacement/regeneration information remains
  in its original README. See the source-distribution inventory report.

The application is supplied as source rather than relying on a final ELF as
relinkable object material. The archive includes generated browser assets and
their composer, partition CSV, headers and all build hooks. No preset/NVS dump,
device password, full-flash backup or private log belongs in this archive.

## Build on Windows

Use Git, Python 3.11 and PlatformIO Core 6.2.0. A virtual environment is
recommended. Use a short, space-free path for the isolated PlatformIO core, such
as `C:/dmx-build/core`; some bundled Windows compiler process launchers fail in
long include paths can exceed Windows' 32,767-character subprocess limit.
Keep both project and core paths short. Never use a production core.

```powershell
python -m venv build-env
./build-env/Scripts/python -m pip install platformio==6.2.0
$env:PYTHONDONTWRITEBYTECODE = '1'
python -B materials/tools/publication_materials.py verify materials
python -B materials/tools/publication_materials.py restore materials frozen C:/dmx-build/core
$env:PLATFORMIO_CORE_DIR = 'C:/dmx-build/core'
```

Install the tools using the exact commands in BUILD-TOOLS.json (run in this
isolated environment, with its `pio` on PATH). The Xtensa S3 compiler is
`espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5`. The SDK is already
included; do not replace it with a newer registry package. Tool hashes describe
the audited Windows installation; a different host's executable hashes require a
separate host validation. Cached copies may be used only after hash verification.

```powershell
$tools = Get-Content materials/BUILD-TOOLS.json | ConvertFrom-Json
foreach ($tool in $tools.tools) {
    pio pkg install --global --tool "$($tool.owner)/$($tool.name)@$($tool.version)"
}
pio run -d frozen -e cores3se_unit -e cores3se_base
```

Do not add `-v` with the audited SCons 4.11.1 package: its verbose post-action
printer fails after linking with `_Null + str`. Ordinary `pio run` executes the
unchanged post-build image/provenance verifier successfully. Do not disable that
verifier to avoid the logging defect. No source changes are required.

## Demonstrate a modified-library relink

First preserve the baseline build outputs outside `.pio/build`. In the isolated
core only, edit `cores/esp32/Print.cpp`, inside
`size_t Print::printf(const char *format, ...)`, adding:

```cpp
// Local relink validation, 2026-11-01; never part of accepted firmware.
static const char lgpl_relink_probe[] = "LGPL_RELINK_TEST_20261101";
__asm__ __volatile__("" : : "r"(lgpl_relink_probe) : "memory");
```

Then run the same two build commands. `printf` is retained in both production
link maps. The default buffer `Print::write` implementation is discarded, so a
probe there proves only compilation and is insufficient. The string/reference proves the
modified Print implementation survives archive extraction and linker garbage
collection. Check that the marker is absent in baseline ELF, present in modified
Print.cpp.o, libFrameworkArduino.a and final ELF, and that Print.cpp.o has a
retained section in the final map. Record both source hashes and the compiler/
link outputs. Do not substitute these binaries for the accepted firmware or
upload them. No actual serial text or DMX behavior is added by this probe.

This is a relinking test, not a promise of bit-identical reconstruction or
hardware acceptance of a modified library. Build dates, paths and tool versions
can change binary bytes. Accepted images keep their original hashes/provenance.

## Publication gate

Host firmware, corresponding-source/build archive and notice archive together,
with SHA-256 references and a separately identified publication commit. Retain
the immutable v0.3.0 tag. The local validation tree remains unpublished until
its inventory, relink evidence and dual-provenance report have passed review.
No source archive alone is treated as proof of LGPL compliance.

The reduced tree is configuration-specific, not an SDK for every Arduino board.
Optional SDK **archives named by the unchanged linker recipe remain included**
even when linker GC excludes them from the firmware. Deleting such archives
would break reconstruction. External filesystem-image formatters are omitted
from the acquisition list because this firmware does not build a filesystem.
License review uses original declarations and upstream texts; automated checks
verify coverage/integrity and relinkability, not an independent legal opinion.
