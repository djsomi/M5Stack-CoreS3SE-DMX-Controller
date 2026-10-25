# Stage 4B: 16 MiB Flash Layout and Embedded Assets

## Hardware validation and local finalization - 2026-10-25

**The user confirms successful physical migration and functional hardware
validation of the Stage 4B 16 MiB firmware, including the Stage 3E RS485_CLASH
correction.** This supersedes the pre-migration statements in the historical
integration and implementation records below. No device operation was performed
by the agent during this finalization.

The user reports that the live partition table was read back from flash address
`0x8000`, and the **complete generated partition image** was verified byte for
byte against the approved migration bundle
`dist/stage4b-unit-migration-stage3e/partitions.bin`. That image is 3,072 bytes,
SHA256 `6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`.
This records the user's hardware verification; no new live read or independent
comparison against an uploaded readback file was performed in this task.

The verified table is the approved `cores3se-16m-ota6-v1` layout: NVS
`0x9000/0x5000`, otadata `0xe000/0x2000`, app0 `0x010000/0x600000`, app1
`0x610000/0x600000`, reserved storage `0xC10000/0x3E0000`, and coredump
`0xFF0000/0x010000`. Readback equality confirms the programmed partition image;
it does not establish upper-address writes, alias resistance or OTA operation.

Functional validation was reported successful, including the corrected DMX
Sender. No additional per-control, duration, Receiver or Base test results are
invented. Base retains its experimental status. The normal Unit approved bundle
application remains SHA256
`32330869686e0ffe358d59d11e769eead202538f8f91cd460d3a44ad55ae3a3e`.
No migration bundle, flash backup, historical file, log or stash is rewritten by
this finalization. The obsolete pre-hotfix bundle remains preserved and obsolete.

### Stage 3E relationship and commit scope

Production hotfix `f513287bf4f6fd0c3717d98ea702748f18f8f291` and the pre-finalization
Stage 4B branch both descend from `e41d8ee21add2bb1fd226600e937b2d348368a05`.
The eight hotfix paths in Stage 4B exactly match that commit's blobs. They were
selectively integrated before this hardware validation, with no dirty-tree
cherry-pick, merge or overwrite of the existing Stage 4B edits.

The focused Stage 4B commit is created directly on the existing branch, with
`e41d8ee...` as its single parent. It therefore **contains the hotfix content**
without making `f513287...` an ancestor; the two branch tips are sibling commits.
No history rewrite or merge into main is part of finalization. The production
hotfix branch/worktree and investigation worktree remain separately preserved.

Sender enables FIFO_EMPTY during FEEDING and TX_DONE during DRAINING; it does
not enable CLASH. Unexpected delivered CLASH is acknowledged without changing
TX state. All 513 slots, qualified DONE, UART idle, immutable snapshot ownership,
safe retirement, genuine error/fail-stop policy, blackout order and Receiver
isolation remain enforced. No temporary event ring, trace hook/API/define, USB
command/dump or diagnostic-only overhead is included. Existing Stage 0 variants
are retained with their original known limitations.

The commit includes the approved Stage 4 design, reproducible patch/build/layout/
packaging tools, public asset API, source HTML/CSS/JS, all four generated gzip/
manifest files, permanent regression/infrastructure tests and relevant reports.
Historical development files, investigation-only report, device logs, Python
cache, backups, firmware/migration artifacts and stashes are excluded.

### Final software recheck

The finalization recheck passed all four PlatformIO environments, six infrastructure
methods (partition/layout, gzip determinism/corruption, stale packaging rejection),
source-integration guards, strict patch-state checks including rejection of the
old complete TX policy, actual installed ISR/API/adapter/transport regressions,
and the generated flash-asset API host test. This includes all 385 incomplete-tail
cases and the permanent CLASH/blackout/Receiver tests.

| Environment | Build | Static RAM | Reported flash | Raw app BIN |
|---|---|---:|---:|---:|
| cores3se_unit | PASS | 35,764 | 607,981 | 608,400 |
| cores3se_base | PASS | 35,764 | 607,949 | 608,368 |
| cores3se_unit_diagnostics | PASS | 38,380 | 610,225 | 610,640 |
| cores3se_base_diagnostics | PASS | 38,380 | 610,209 | 610,624 |

All four real artifact receipts, 16 MiB headers, approved partition binaries,
source and asset identities were rechecked. Rebuilt Unit firmware is byte-identical
to the approved migrated bundle application. Firmware build inputs/source digest
remain `aab057aa5bb3a4b1d3ee2dad824a831f988aad2027d2a39fe8f2136bdb0252fa`;
only this report changes for the newly supplied hardware result. Temporary trace
symbols remain absent, and the committed Stage 3E blobs remain exact matches.

Both working-tree and exact staged whitespace checks are required before the
commit. The staged-input check validates generated gzip/manifest and partition
inputs from the actual index, without relying on untracked local build inputs.
Finalization logs are retained under ignored `dist/stage4b-finalization-20261025/`.
The approved design remains unchanged and future backup/restore requirements
are preserved. No new migration bundle or device operation is needed here.

### Explicit remaining limitations

- Upper-flash writes and address-alias verification remain unverified. A matching
  live partition image or successful low-address app boot does not test them.
- Runtime free/minimum internal heap, largest internal/DMA blocks and PSRAM
  consumption under the 16 MiB firmware remain unverified; static build sizes
  below are not new runtime measurements.
- OTA update functionality and meaningful project-health rollback are not
  implemented or hardware-validated. The retained Arduino early-confirmation
  behavior and the presence of two slots do not establish rollback safety.
- The legacy power-action helper still does not guarantee complete zero frames
  before restart/shutdown. No shutdown-zero capture or redesign was performed.
- External RS-485 electrical measurements remain pending: waveform quality,
  baud/framing, break/MAB, complete 513-slot wire packets, coherence and frame gaps.
  Functional fixture tests and software logs do not establish those properties.

Future offline Web Preset Backup & Restore remains the approved roadmap
requirement described below. No Wi-Fi, HTTP, WebSocket, OTA implementation or
Stage 5 work is introduced by this commit. No flash, erase, push, main merge,
tag or release is authorized by or performed during this finalization.

The remaining sections are preserved historical implementation records; their
pre-migration status statements are superseded by this hardware-validation record.

## Prior Stage 3E production hotfix integration — 2026-10-25

**Stage 4B remains uncommitted and has not been flashed.** This section supersedes
the initial build numbers, source identity and migration artifact below. All
historical records and the future Web Preset Backup & Restore requirement remain.

### Confirmed regression, hardware result and production commit

The user captured two no-XLR Unit cold boots with COLLISION=6, raw IRQ `0x20000`,
enabled IRQ `0x20002`, 128/513 slots queued, safe retirement and the three-error
latch. The user then confirmed successful functional hardware validation of the
Stage 3E correction. External RS-485 waveform measurement remains **pending**.
The reported hardware pass is not a claim that this exact production-only or
integrated 16 MiB artifact has been flashed or electrically validated.

Focused local production commit:
`f513287bf4f6fd0c3717d98ea702748f18f8f291`
(`Stage 3E: Fix RS485 clash handling in DMX sender`), on
`codex/stage3e-rs485-clash-hotfix`, from Stage 3 `e41d8ee...`.
All four original 8 MiB environments and Controller/Dispatcher/ISR/transport/
patch/integration tests passed before that commit. Its worktree is clean.
See [the production hotfix report](STAGE3E_PRODUCTION_HOTFIX.md).

The production correction is exactly:

- FEEDING enables FIFO_EMPTY; DRAINING enables TX_DONE, without CLASH delivery.
- Unexpected delivered CLASH is acknowledged in WRITE without changing TX state.
- Full 513-slot completion qualification, immutable snapshot, UART idle, genuine
  underrun/premature DONE/timeout and failed-retirement checks remain enforced.
- Three genuine consecutive failures still latch; blackout ordering, latest
  values and Receiver handling are preserved. No UART/pin/framing change.

### Selective integration and protected work

The hotfix's eight paths were disjoint from the pre-existing Stage 4B edits.
Every target was checked against its original Stage 3/pre-task contents before
copying exact **committed** blobs. No cherry-pick, merge, index or HEAD change was
performed in Stage 4B; its HEAD remains `e41d8ee21add2bb1fd226600e937b2d348368a05`.
The hotfix delta and all Stage 4B work remain uncommitted together here.

Temporary Stage 3D event rings, trace APIs/hooks, USB commands/dumps, trace flags
and diagnostic overhead were excluded. Production src/config and the existing
Stage 0 diagnostic behavior remain unchanged. No Wi-Fi/HTTP/WS/OTA feature exists.

The original `platformio.ini` Stage 4B edits and its scoped integration guard
are byte-for-byte preserved. Partition CSV, packaging/layout/asset scripts,
asset API, readable web sources and checked deterministic gzip assets are
unchanged. No guard was disabled: the existing source digest automatically
incorporates the new production patch and all four post-build receipts match it.

Prior build/ELF/map/driver sets were archived with hashes under
`dist/stage3e-finalization-20261025/stage4b-before-hotfix/` before rebuilding.
The investigation worktree, old bundle, historical files, logs, existing backup
and stash were not overwritten or restored. Stash remains `996a2f9378208540b0fd3b544e7d9908affe8e84`.

### Repeated software validation and resources

All four 16 MiB builds PASS. Controller and Dispatcher (six groups each), actual
ISR/API/adapter/Transport regressions (including 385 incomplete tails), strict
patch-state/prior-revision rejection, source-integration guards, six infrastructure
tests, deterministic gzip verification and generated asset API host tests PASS.
Post-build checks validate real image headers/checksums/hashes, approved table,
source/asset identity, offsets, capacities and embedded retention. Both production
and Stage 4B whitespace checks PASS. No temporary trace symbols are linked.

| Environment | Static RAM | Reported flash | Raw app BIN | Result |
|---|---:|---:|---:|---|
| cores3se_unit | 35764 | 607981 | 608400 | PASS |
| cores3se_base | 35764 | 607949 | 608368 | PASS |
| cores3se_unit_diagnostics | 38380 | 610225 | 610640 | PASS |
| cores3se_base_diagnostics | 38380 | 610209 | 610624 | PASS |

Normal static RAM remains **35,764 bytes**, with no Stage 3D overhead; original
Stage 0 diagnostics remain 38,380 bytes. These are build sizes, not new runtime
heap/PSRAM/DMA measurements. Dependency folder/alignment differences can affect
small flash comparisons. Unit app0 headroom is 6,291,456 - 608,400 = 5,683,056 bytes.

Current source digest (dirty Stage 4B inputs):
`aab057aa5bb3a4b1d3ee2dad824a831f988aad2027d2a39fe8f2136bdb0252fa`.
Unchanged deterministic asset bundle:
`5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9` (674 total gzip bytes).
All four updated driver installations match the production patch fingerprints.

### New verified Unit migration bundle

**Use only `dist/stage4b-unit-migration-stage3e/` for the next migration review.**
Created using the unchanged `scripts/package_firmware.py` after all builds/tests.
The old `dist/stage4b-unit-migration/` contains the pre-hotfix transport and is
**OBSOLETE — DO NOT FLASH**. It is preserved unchanged, not reused or overwritten.
No padded FULL image was generated.

| Image | Offset | Bytes | SHA-256 |
|---|---:|---:|---|
| bootloader.bin | `0x000000` | 15104 | `2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363` |
| partitions.bin | `0x008000` | 3072 | `6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387` |
| boot_app0.bin | `0x00e000` | 8192 | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| firmware.bin | `0x010000` | 608400 | `32330869686e0ffe358d59d11e769eead202538f8f91cd460d3a44ad55ae3a3e` |

Metadata:

```text
manifest.json  1223 bytes  SHA256 94256eb43470cb0244550fb1ae40b301585400aec8341659dd3f2a160748f71d
SHA256SUMS.txt  401 bytes  SHA256 cace27dc14ffeab2d5ecc2f4da162e7a4f2786036cff1027c98c37a2a750d6e2
```

`SHA256SUMS.txt` verifies all four images and manifest (five entries). Manifest
layout is `cores3se-16m-ota6-v1`, profile `cores3se_unit`, flash size 16,777,216,
packaging `separate-images-only`. The application's source fingerprint and bytes
are different from the obsolete bundle; no stale pre-hotfix app is reused.
Bootloader/app headers are ESP32-S3 / DIO / 80MHz / **16MB**. Embedded assets are
already in the application, without an FS image.

Approved table remains exactly NVS `0x9000/0x5000`, otadata `0xe000/0x2000`,
app0 `0x010000/0x600000`, app1 `0x610000/0x600000`, reserved storage
`0xC10000/0x3E0000`, coredump `0xFF0000/0x010000`. CSV and binary table hashes
are unchanged. Separate write ranges, including sector erase rounding, exclude
NVS `0x9000..0xdfff`; OTA selection is intentionally reset to app0. No device
was read, erased, repartitioned or flashed, and no backup was restored/written.

### Remaining gates

Physical 16 MiB migration requires separate explicit authorization and the saved
full-chip backup/recovery path. Validate programmed ranges/live table, NVS preset/
settings behavior, cold starts/no-receiver/reconnect, touch/UI/canvas, battery/
speaker/sleep, runtime heap/DMA/PSRAM, supported Receiver and experimental Base
separately. Upper-address/alias tests require their own controlled unused region.
External RS-485 capture must establish length/coherence, break/MAB, gaps and
blackout transitions; no such measurement is claimed completed.

Two slots do not implement an updater or project-health rollback. The existing
Arduino early OTA confirmation, legacy shutdown's lack of guaranteed zero frames,
diagnostic USB/stub uncertainty and stale diagnostic banner remain limitations.
Future offline Web Preset Backup & Restore remains approved roadmap work only,
with schema validation and explicit partial-failure/power-loss handling. No Stage
5 implementation or publication was started. Stop at this verified new bundle.

## Historical initial Stage 4B record — 2026-10-24

The following initial findings/artifacts are retained for provenance. Current
software/hotfix/package state is given above; do not use the old app hashes or
old bundle for migration.

Software implementation and validation, 2026-10-24. **Uncommitted; not flashed.**
Branch: `codex/stage4b-web-infrastructure`. Baseline:
`e41d8ee21add2bb1fd226600e937b2d348368a05` (committed Stage 3).

The user reports Stage 3 hardware functional and extended endurance tests passed.
That supersedes older endurance status in the retained Stage 3 reports. No new
electrical capture or Stage 4B hardware result is inferred from that statement.
Unit remains primary; Base remains experimental.

The actual, previously untracked `STAGE4_WEB_INFRASTRUCTURE_DESIGN.md` is preserved
unchanged. The latest Stage 4B request approves the exact 16 MiB table and directs
the four named environments to build the new configuration. They now all use
16 MiB on this development branch, instead of the design's earlier opt-in proposal.
**This is not authorization to upload them.** Exact Stage 3 rollback uses its
commit/configuration in a separate checkout, not a new app under the new table.

## 1. Files and protected scope

Modified:

- `platformio.ini`: shared 16 MiB setting, repository partition CSV, asset hook.
- `tests/dispatcher/check_integration.py`: allow exactly those three Stage 4B
  configuration additions while retaining the existing full protected comparison.

Added:

- `.gitattributes`: generated gzip is binary; generated manifest stays LF on checkout.
- `partitions/cores3se_16mb_ota.csv`
- `include/web_assets.h`
- `scripts/web_asset_pipeline.py`
- `scripts/embed_web_assets.py`
- `scripts/firmware_layout.py`
- `scripts/package_firmware.py`
- `web/src/index.html`, `web/src/style.css`, `web/src/app.js`
- `web/generated/index.html.gz`, `web/generated/style.css.gz`,
  `web/generated/app.js.gz`, `web/generated/manifest.json`
- `tests/infrastructure/test_infrastructure.py`
- `tests/infrastructure/test_asset_api.cpp`
- `tests/infrastructure/README.md`
- This report, `docs/STAGE4B_IMPLEMENTATION.md`.

Existing application files under `src/`, hardware/power/diagnostic headers and
all esp_dmx patch templates/scripts are unchanged. Controller, Dispatcher,
Transport, blackout barriers, presets, UI/canvas, battery/speaker, sleep/wake,
USB settings, upload speed and pins retain Stage 3 implementation. There is no
Wi-Fi/HTTP/WS initialization, new networking dependency, filesystem mount, OTA
updater, user-visible screen or runtime call into the placeholder assets.

Historical files, logs and stash
`996a2f9378208540b0fd3b544e7d9908affe8e84` are preserved. Nothing is staged or
committed for Stage 4B. No push, merge, tag or release.

## 2. Final partition table and capacity

```csv
# Name,Type,SubType,Offset,Size,Flags
nvs,data,nvs,0x009000,0x005000,
otadata,data,ota,0x00e000,0x002000,
app0,app,ota_0,0x010000,0x600000,
app1,app,ota_1,0x610000,0x600000,
spiffs,data,spiffs,0xc10000,0x3e0000,
coredump,data,coredump,0xff0000,0x010000,
```

| Region | Offset | Size in bytes | End, exclusive |
|---|---:|---:|---:|
| Bootloader/table/NVS/OTA low region combined | `0x000000` | 65,536 | `0x010000` |
| app0 | `0x010000` | 6,291,456 | `0x610000` |
| app1 | `0x610000` | 6,291,456 | `0xc10000` |
| Reserved storage | `0xc10000` | 4,063,232 | `0xff0000` |
| Coredump reservation | `0xff0000` | 65,536 | `0x1000000` |

Total: 65,536 + 2 x 6,291,456 + 4,063,232 + 65,536 = **16,777,216 bytes**.
NVS remains `0x9000/0x5000` (20 KiB), otadata `0xe000/0x2000` (8 KiB).
App starts are 64 KiB aligned; data boundaries are sector aligned. There are no
overlaps. The `spiffs` data subtype is a reservation compatible with the tools;
no SPIFFS or LittleFS image is created or mounted. No factory rescue app is added.

Old 8 MiB table: app0 `0x10000/0x330000`, app1 `0x340000/0x330000`,
storage `0x670000/0x180000`, coredump `0x7f0000/0x10000`. NVS and otadata
offsets/sizes are unchanged; app1/storage/coredump move in the new table.

Shared configuration:

```ini
board_upload.flash_size = 16MB
board_build.partitions = partitions/cores3se_16mb_ota.csv
```

Platform 6.7.0's actual builder consumes `upload.flash_size` for both image
generation and upload, and derives maximum application size from app0. Verified
capacity in build logs is 6,291,456 bytes, not the whole chip. Board, flash mode,
80 MHz flash clock, PSRAM settings and USB flags are unchanged. Generated image
headers are ESP32-S3, **16MB / DIO / 80MHz**. DIO in this header is the existing
builder's QIO mapping, not a flash-mode change.

The installed SDK partition generator validates the CSV, its binary round-trip,
alignment and boundaries. The post-build verifier compares the entire generated
table, checks its MD5, and requires the exact reviewed layout. A conflict is an
error, never an automatic fallback to another table.

## 3. Asset pipeline and Stage 5 API

Readable sources are under `web/src`. They contain only an offline placeholder,
stylesheet and a small DOM marker. No fetch, WebSocket, timer, storage or control
implementation is present. HTML refers to content-hashed CSS/JS URLs.

Regenerate explicitly with **Python 3.11.7 / zlib 1.2.13**:

```powershell
python scripts/web_asset_pipeline.py --generate
```

The generator enforces this regeneration toolchain, UTF-8 inputs, required files,
nonempty content, bounded size and required HTML substitution markers. Gzip uses
level 9, mtime zero, no filename and a fixed header with OS byte 255. Fixed input
order and canonical JSON remove timestamp/path/order metadata. Two independent
regenerations were byte-identical. Pinning zlib matters; mtime zero alone would
not establish identical compression across arbitrary compressor versions.

Normal builds **verify** the checked generated bundle, rather than regenerating
tracked files. They do not require the exact regeneration version or Node;
the implemented Python tooling requires Python >=3.10 and was tested on 3.11.7.
Missing, stale, malformed or corrupt inputs fail clearly before compilation.
Source hashes normalize CRLF to LF; generated gzip bytes are checked exactly.
Include all four generated files in any eventual commit/source archive.

`pre:scripts/embed_web_assets.py` follows the unchanged DMX hooks. It validates
assets/table, emits `$BUILD_DIR/generated/web_assets.cpp`, and builds a flash-only
object with explicit asset/header dependencies. `--undefined=web_asset_bundle`
retains the root and its referenced bytes despite section garbage collection.
The generated file is never hand-edited. Actual linked BIN containment checks
catch discarded or missing assets.

`include/web_assets.h` provides immutable `Asset`, `Bundle`, `bundle()` and
`find(path)`. Asset records contain path, MIME, `gzip` encoding, byte pointer,
compressed length, original length and SHA-256. Stage 5 can send exactly `size`
bytes from flash without runtime decompression or copying the whole site into
RAM. Do not use strlen on binary bytes. HTTP must implement gzip negotiation,
appropriate caching, bounded chunk sends and security separately.

Bundle identity:

```text
5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9
```

A normalized build-source digest, hardware environment and layout ID are embedded
with the assets and recorded in the post-build receipt. They identify the dirty
development inputs without claiming they are a release commit. The existing
stale diagnostic banner is deliberately unchanged; use receipts/binary hashes,
not that banner, for these artifacts. Current build-source digest:

```text
d631747a0f7a4ae6a39a237f511ecd30a0744c6464c5c8e477de7fb94c5d088a
```

### Sizes

| Asset | Literal source bytes | Served bytes before gzip | Gzip bytes |
|---|---:|---:|---:|
| index.html | 473 | 497 after hashed URL substitution | 338 |
| style.css | 250 | 250 | 196 |
| app.js | 151 | 151 | 140 |
| Total | 874 | 898 | 674 |

Checked manifest: 1,443 bytes. Generated C++ source: 4,654 bytes for normal
profiles, 4,666 for diagnostics (environment name length). Those are host-side
source sizes, not RAM allocations. Target asset descriptors total 84 bytes and
the Bundle root 24 bytes, both flash-mapped. ELF inspection places them and the
compressed arrays at `0x3c...` in DROM. Total linked read-only growth includes
strings, hashes and alignment, as shown below. Asset budget is 256 KiB gzip.

## 4. Builds, tests and resource comparison

All four final builds PASS, including the post-build header, checksum/hash,
table, source/asset identity, offsets and retention checks:

| Environment | Stage 3 static RAM | Stage 4B static RAM | Stage 3 reported flash | Stage 4B reported flash | Delta | Raw app BIN bytes |
|---|---:|---:|---:|---:|---:|---:|
| cores3se_unit | 35,764 | 35,764 | 606,713 | 607,993 | +1,280 | 608,416 |
| cores3se_base | 35,764 | 35,764 | 606,681 | 607,961 | +1,280 | 608,384 |
| cores3se_unit_diagnostics | 38,380 | 38,380 | 608,957 | 610,237 | +1,280 | 610,656 |
| cores3se_base_diagnostics | 38,380 | 38,380 | 608,925 | 610,221 | +1,296 | 610,640 |

Comparison uses the preserved actual Stage 3 ELFs from before this change.
`.dram0.data`, `.dram0.bss`, `.iram0.text` and `.flash.text` sizes are unchanged
for every environment; the increase is `.flash.rodata`. Base diagnostic alignment
adds 16 more bytes than the other variants. Static RAM delta is zero. Normal
Unit app0 headroom is 6,291,456 - 608,416 = **5,683,040 bytes**; the same image
must fit app1. Reported flash use differs from the raw signed/hash-bearing BIN.

No new task, constructor, allocation or decompression runs in the application.
The lookup functions are not called by existing firmware. **Runtime heap, PSRAM,
largest DMA block, canvas behavior and actual flash access are unmeasured in
Stage 4B until hardware tests.** Static section equality does not measure them.

Automated results:

- Six infrastructure test methods PASS: deterministic regeneration, checked
  artifact equality, gzip round-trip/header/length/MIME checks, missing/invalid/
  stale inputs, corrupt manifests/gzip, CRLF handling, deterministic generated
  C++, exact partition binary round-trip, alignment/overlap/overflow/OTA capacity
  rejection, MD5 corruption and separate-image/stale-image packaging rejection.
- Generated asset API host compile/run PASS: lookup, metadata, bounds/null/missing
  paths, identity and binary pointers.
- Controller six groups, Dispatcher six groups, source-integration guards and
  actual ISR/API/adapter/transport seven groups PASS, including deliberate
  underruns, incomplete transmissions, retries and ordered blackout barriers.
- Driver patch-state tests PASS. No driver/template changes or manual library edits.
- Four requested environments PASS. No new firmware library dependencies;
  M5Unified 0.2.21, M5GFX 0.2.28 and esp_dmx 2.0.2 remain selected.
- An isolated archive of the Stage 3 baseline plus only intended Stage 4B build
  inputs, with no `.git` and initially no `.pio`, successfully installed dependencies,
  applied both DMX patches and built Unit. A clean output rebuild also checks
  the final hook revision. It shares installed platform/toolchain packages,
  but no production libdeps, generated objects or receipt.
- `git diff --check` and whitespace checks of all 18 new files PASS; the actual
  staging area remains empty.

The isolated build's different dependency path gives 608,025 bytes reported
flash versus 607,993 locally; assets/source identity and partitions match. This
proves a working clean source build, **not bit-identical full firmware across
different paths/tool versions**. No stronger firmware reproducibility is claimed.

Validation artifacts are ignored under `dist/stage4b-validation-20261024/`:
`stage3-baseline/`, `builds.log`, `final-builds.log`, `clean-project/`,
`clean-unit.log`, `clean-final-build.log`, host executables and image bundles.
Existing Stage 3 logs/artifacts are not overwritten.

## 5. Separate-image packaging and exact offsets

After a build, `$BUILD_DIR/infrastructure.json` records source/bundle/layout,
actual PlatformIO flash offsets, component sizes/hashes and data effects. The
verifier checks ESP32-S3 headers, segment checksums, appended SHA-256, exact
CSV/binary equality, bootloader/table boundaries, both app capacities, SDK OTA
initialization bytes, no overlaps/NVS writes, and embedded bytes in the app.

```powershell
python scripts/package_firmware.py --env cores3se_unit --output dist/NEW-UNIT-BUNDLE
```

The output directory must be new. Packaging refuses stale sources, mismatched
assets/profiles, missing components or altered image hashes. It copies four
**separate** images, manifest and SHA256SUMS. It performs no serial, erase,
merge or upload operation. There is no automatic FULL image option.

Observed generated image layout, checked against PlatformIO and the table:

| File | Offset | Current bytes | Effect |
|---|---:|---:|---|
| bootloader.bin | `0x000000` | 15,104 | Install matching 16 MiB boot image |
| partitions.bin | `0x008000` | 3,072; one 4 KiB erase sector | Install new table |
| boot_app0.bin | `0x00e000` | 8,192 | Reset OTA selection to app0 |
| firmware.bin | `0x010000` | Per-profile table above | Install app0 including all assets |

Normal Unit uses these hashes for the shared low-flash components:

```text
bootloader.bin  2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363
partitions.bin  6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387
boot_app0.bin   f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0
```

Use each bundle's own SHA256SUMS for the app. No filesystem image is required:
assets are already inside firmware.bin. No app1 image is included. A/B update
support is not implemented, and an old app at the old app1 offset is not a backup
under the new table.

## 6. First migration procedure â€” NOT EXECUTED

Wait for explicit user approval. This is a planned factory USB migration, not
an ordinary app upload or OTA update. Do not run a normal upload merely to test
the new application while the table/metadata still describe the old layout.

1. Make fixtures safe; stop DMX use. Record power arrangement and current firmware.
   Use the known Unit/Battery Bottom arrangement. Close monitors. Preserve a
   verified Stage 3 recovery bundle and recovery access before writing anything.
   Diagnostic automatic USB/stub behavior remains an open separate issue; start
   with normal Unit and the proven recovery procedure, without changing USB/speed.
2. Read/retain the current device table, OTA metadata and a private flash backup
   with new filenames. Confirm actual 16 MiB flash and the expected old 8 MiB
   table. Determine current active slot before any app-only recovery experiment.
   If contents/capacity differ, stop and revise this procedure.
3. Build/package the chosen final environment; verify its complete SHA256SUMS,
   source/layout identity and four offsets. Never mix 8/16 MiB bootloader, table,
   app or a different hardware profile. No FS image is part of the installation.
4. **No whole-chip erase is required. No NVS erase is required by the new table.**
   Write the four separate regions at the offsets above, which resets otadata
   deliberately and installs app0. Sector erase/padding of these separate writes
   does not cross `0x9000..0xdfff`. The SDK boot_app0 file is not all 0xff: its
   first OTA record has sequence 1 and SDK-provided CRC, selecting ota_0. Use the
   checked file, not a hand-crafted record or copied live old selection.
5. Existing presets/settings are expected to remain because NVS offset, size,
   format and adapter are unchanged. The user permits preset loss, but it is
   **not automatic or necessary** here. If a clean NVS reset is chosen, obtain
   explicit authorization for its wider effects: erase only `0x9000/0x5000`,
   which loses all presets and other settings/credentials in that partition.
   Do not silently turn that waiver into a full-chip erase. The Arduino core can
   also erase NVS after certain init errors; absolute preservation is not promised.
6. Reboot and verify the selected app, its 16 MiB image header and decoded live
   table. Read back/compare programmed image regions, allowing for any boot-time
   OTA metadata changes rather than expecting live otadata to stay immutable.
   Verify actual flash-size reporting with the existing diagnostic/API procedure;
   an 8 MiB diagnostic configuration is not evidence for the new header/table.
7. Confirm known-good cold starts/power behavior, canvas and all controls before
   reconnecting fixtures. Exercise the hardware acceptance matrix below. Do not
   format reserved storage or claim high-address write/OTA rollback tests passed
   solely because the low-address application boots.

Command template for step 4, **only after separate authorization and replacing
`BUNDLE` with the verified normal Unit bundle directory**:

```powershell
$python = "$env:USERPROFILE/.platformio/penv/Scripts/python.exe"
$esptool = "$env:USERPROFILE/.platformio/packages/tool-esptoolpy/esptool.py"
& $python $esptool --chip esp32s3 --port COM8 --baud 1500000 --before default_reset --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size 16MB 0x0 BUNDLE/bootloader.bin 0x8000 BUNDLE/partitions.bin 0xe000 BUNDLE/boot_app0.bin 0x10000 BUNDLE/firmware.bin
```

This deliberately rewrites the table and OTA selection; it is not a general
routine update command. If automatic reset/stub fails, stop and follow the
documented USB recovery investigation rather than repeatedly changing speed,
erasing or mixing images. The template was not executed by the agent.

### Why no FULL image

The retained older flat FULL images contain 0xff padding over NVS. Writing one
erases NVS even though no NVS image is explicitly listed. Filling gaps is not a
preservation method. This pipeline never merges those gaps and does not create
a new FULL artifact. A future release FULL format needs a separately approved,
explicit factory-reset policy; app-plus-assets already requires just firmware.bin.
Reserved upper storage/coredump/app1 contents are not automatically initialized
by these four writes. They remain unused; any later use needs an initialization
and data-compatibility policy.

## 7. Recovery and exact Stage 3 rollback

Do not reset this development tree or use its new firmware.bin as an 8 MiB
rollback. Keep the new branch, original branch, design, logs and stash intact.

Before changing configuration, 22 baseline components were copied with SHA-256
to `dist/stage4b-validation-20261024/stage3-baseline/`: all four actual Stage 3
app ELF/map/BIN, bootloader/table, plus the SDK OTA image and old CSV. They are
local validation/recovery evidence, not a public release or substitute for the
user's off-device backup.

To rebuild exact committed Stage 3 sources elsewhere, use a **new** output path:

```powershell
git archive --format=zip --output=dist/NEW-stage3-source.zip e41d8ee21add2bb1fd226600e937b2d348368a05
Expand-Archive -LiteralPath dist/NEW-stage3-source.zip -DestinationPath dist/NEW-stage3-source
pio run --project-dir dist/NEW-stage3-source -e cores3se_unit
```

That commit retains its original 8 MiB board-default table, dependency pins,
both reproducible DMX patch hooks and all four normal/diagnostic environments.
These archive/build commands have no checkout/stash operation. No rollback
source/configuration has been rewritten by Stage 4B.

Failed migration: use verified physical/ROM USB recovery, read current state if
possible, and either retry the verified matching 16 MiB four-image bundle or
restore the complete matching 8 MiB Stage 3 bundle. The recovery write uses the
same four low offsets, with the **old** bootloader/table/app and 8MB flash-size
header setting. Reset otadata with the matching SDK file. Old app1 returns to
`0x340000`, but it is not guaranteed to contain valid firmware: boot app0.
Restoring only the old app under the new table is not the supported rollback.
Preserve NVS via separate image writes or restore a compatible private backup
with explicit approval; do not flash a padded old FULL as a preservation method.
Any whole-chip erase or NVS restoration is a separate authorized action.

## 8. OTA limitations and future acceptance

Installed Arduino 2.0.16 / IDF 4.4.7 sources were rechecked:

- `qio_qspi/include/sdkconfig.h:27,777` enables bootloader/app rollback support.
- `cores/esp32/esp32-hal-misc.c:204-208,225-236` weak defaults return true from
  `verifyOta()` and false from `verifyRollbackLater()`, confirming a pending image
  in `initArduino()` before setup/M5/Controller/Transport application health.

Two slots plus those SDK defaults do **not** establish useful project-health
rollback. No hook override, OTA transport, signature/authentication policy or
updater is implemented here. No automatic rollback is claimed operational.

Future OTA acceptance requires an authenticated/validated inactive-slot writer,
compatible chip/layout/assets, deferred confirmation with the correct hook ABI,
defined boot health gates and bounded deadlines. Test failure before confirmation,
watchdog/reset, power cuts during download/write/selection/first boot, corrupt or
oversized images, both slot directions, healthy confirmation, matching asset
rollback, and USB recovery. Keep table changes outside routine app OTA. A known
second app is required before treating the other slot as a working fallback.
None of those tests is replaced by software queue logs or placeholder page hashes.

## 9. Stage 5 requirements and future roadmap

Stage 5 consumes this flash-backed API and serves the placeholder/static site;
it must add SoftAP/provisioning, bounded HTTP task work, gzip/cache/security
rules and measured resource caps separately. Keep Controller/Dispatcher/Transport
access on the existing owner task. WS commands remain a later stage; do not call
those components from HTTP/Wi-Fi callbacks. Preserve/reserve the one canvas before
radio allocation and measure internal/DMA heap and scheduling under load.

The approved future **offline Web Preset Backup & Restore** remains roadmap-only:
download/upload P1-P8 JSON with schema version, validation, compatible older
schemas where practical, logical occupancy/error distinctions and bounded
owner-side import. No cloud or filesystem is needed for browser downloads.
Define partial-failure/power-loss semantics before implementation; existing
multi-key NVS presets do not provide an all-eight atomic import transaction.
It is separate from Stage 4B and initial Wi-Fi/WS.

## 10. Hardware acceptance and remaining gates

- Explicit migration approval, exact final artifact/source identity, safe fixture
  state, device/private backup and proven USB recovery path.
- Actual reported chip capacity, live decoded table/header consistency and a
  separately planned/authorized unused-upper-flash address/alias validation.
  A 16 MiB header or JEDEC result alone does not prove every upper address works.
- Repeated power/cold starts for the known USB/Battery Bottom configurations;
  no power/USB policy change or new open serial monitor was made here.
- Normal Unit touch grid/selection/scroll, sliders, Precision STEP/hold/keypad,
  P1-P8 save/load/copy/clear, exact blackout restore, Settings and one shared canvas.
- Sleep/consumed wake, MUTE/UI tones and battery warning volume/restoration.
- Continuous DMX and Receiver separation; compare wire timing/coherence using
  external RS-485 capture where required, not software logs alone.
- Measure post-migration internal free/minimum heap, largest internal/DMA block,
  PSRAM and repeat-page allocation behavior; no new hardware memory claim yet.
- Test Base separately and retain its experimental designation.
- OTA fault/recovery acceptance belongs to the later OTA implementation, not this
  migration. Diagnostic USB/stub uncertainty and stale diagnostic provenance
  remain separately documented limitations.

Software packaging/layout has no identified toolchain conflict. Physical migration,
upper-address access and the hardware regression checklist remain **unperformed**.
Stop here for review; no firmware upload, erase, physical repartition, commit or
publication is authorized by completion of these software checks.
