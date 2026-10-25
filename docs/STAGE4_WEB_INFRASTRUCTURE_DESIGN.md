# Stage 4A: Web Infrastructure and Flash Layout Design

Design only, 2026-10-24. No firmware, dependencies, PlatformIO configuration,
partitions or device contents are changed by this document.

Baseline: branch `codex/stage3-dmx-transport`, HEAD
`4c973007d74a1f24319e0e375a56107bc261b784` (Stage 2), with the current
**uncommitted Stage 3 candidate** layered on it. The user reports successful
initial Stage 3 functional hardware validation; extended endurance is ongoing.
This newer result supersedes the historical validation status in
`STAGE3_TRANSPORT_IMPLEMENTATION.md`. It does not establish electrical DMX
correctness or approval to commit Stage 3. Unit is primary; Base remains experimental.

The additional user decision permits a justified future repartition and accepts
loss of P1-P8 in that migration. It does not authorize an erase or migration in
this design task. Preserve NVS where practical, but distinguish presets from
other settings and future AP credentials.

## 1. Recommendation and approval gates

Recommend **compressed web assets embedded in the application**, the bundled
ESP-IDF HTTP/WebSocket server, a SoftAP, and bounded messages bridging to the
existing application-task CommandDispatcher. LittleFS is unnecessary initially.

For the full physical 16 MiB, recommend two **6 MiB OTA application slots**, with
the existing NVS offset/size retained and 3.875 MiB reserved for future storage.
First prove asset packaging on the current 8 MiB layout. Introduce the 16 MiB
layout as an opt-in build, then validate a deliberately approved migration on
hardware before changing the normal defaults. Reserved OTA capacity is not an
implemented OTA update feature.

| Decision | Recommendation | Gate before implementation/deployment |
|---|---|---|
| Stage 4B asset packaging | GO for the proposed design after review | Separate approval; no networking or device writes |
| Full 16 MiB layout | GO as proposed, subject to board verification | Approve the exact table in section 3; build and hardware gates remain |
| Make 16 MiB the default | NO-GO now | Successful opt-in build, migration, cold-start and capacity tests first |
| Embedded gzip assets | GO | Asset size, linker retention and deterministic build tests |
| LittleFS or new filesystem dependency | Defer | Demonstrated need for mutable/large assets and an update consistency design |
| Bundled `esp_http_server` | GO for later implementation | Exact installed stack supports WS; concurrency and resource tests still needed |
| SoftAP/HTTP | Stage 5 only | Stage 3 endurance accepted, Stage 4B packaging/layout accepted |
| WebSocket mutation bridge | Stage 6 only | Bounded queue/protocol tests and Stage 5 hardware resource results |
| OTA endpoint or health-based rollback claims | NO-GO now | Separate updater, confirmation and failure-recovery validation |
| Preserve old P1-P8 during migration | Not required by user | Report data loss explicitly; do not silently erase unrelated data |
| JSON preset backup/restore | Roadmap only | Separate feature after initial web control, section 11 |

Outstanding product choices: credential display/recovery interaction, whether
two simultaneous operators are acceptable, and final numeric resource limits
after measurements. Proposed defaults below are reviewable starting points,
not measured capacity claims.

## 2. Current flash, build and packaging audit

### 2.1 Evidence and versions

Read the current Controller/Dispatcher/transport implementation, preset adapter,
application loop, UI canvas paths, Stage 0-3 reports and USB investigation.
`wifi-architecture-proposal.md`, `AGENTS.md` and `PROJECT_STATUS.md` are not
present in this checkout, including its historical/untracked files. This design
uses the actual code and available reports; it does not claim to have recovered
that missing original proposal.

Installed environment:

| Component | Inspected version/configuration |
|---|---|
| Platform | `espressif32@6.7.0` |
| Board | `esp32-s3-devkitc-1` |
| Framework | Arduino ESP32 2.0.16, ESP-IDF 4.4.7 |
| Libraries | M5Unified 0.2.21, M5GFX 0.2.28, esp_dmx tag `v2.02` / metadata 2.0.2 |
| Driver changes | Existing S3 getter patch plus repository Stage 3 TX ownership patch |
| Default profile | `cores3se_unit`; Base and both diagnostic environments also exist |
| USB/clock/memory | Existing USB flags, 1,500,000 upload speed, QIO configuration, working PSRAM settings retained |

`platformio.ini` has no flash-size override, custom partition CSV, filesystem
choice, web asset directory or web packaging hook. Both current driver hooks
remain required and outside this task's scope.

The installed board manifest
`C:/Users/somog/.platformio/platforms/espressif32/boards/esp32-s3-devkitc-1.json`
selects `default_8MB.csv` and `upload.flash_size = 8MB`. Its generic N8/No-PSRAM
description is not an inventory of the actual CoreS3SE. This board choice is
why the build uses 8 MiB despite the measured physical 16,777,216 bytes.

The generated link map uses the framework's `qio_qspi` SDK. Its `memory.ld`
has an approximately 8 MiB IROM virtual mapping window and 32 MiB DROM window;
those are virtual linker regions, not detected physical capacity. Two 6 MiB
slots do not require expanding that code window. More flash does not add RAM.
Do not change the working PSRAM bus mode merely to enable 16 MiB flash.

### 2.2 Current offsets and generated artifacts

The installed CSV at
`framework-arduinoespressif32/tools/partitions/default_8MB.csv` and decoded
`.pio/build/cores3se_unit/partitions.bin` agree:

| Component | Offset | Size | End, exclusive |
|---|---:|---:|---:|
| ESP32-S3 bootloader | `0x000000` | current file 15,104 bytes | before table |
| Partition table | `0x008000` | file 3,072 bytes; sector 4 KiB | `0x009000` sector end |
| NVS | `0x009000` | `0x005000` = 20 KiB | `0x00e000` |
| OTA selection data | `0x00e000` | `0x002000` = 8 KiB | `0x010000` |
| app0 / ota_0 | `0x010000` | `0x330000` = 3,342,336 bytes | `0x340000` |
| app1 / ota_1 | `0x340000` | `0x330000` | `0x670000` |
| spiffs data reservation | `0x670000` | `0x180000` = 1.5 MiB | `0x7f0000` |
| coredump | `0x7f0000` | `0x010000` = 64 KiB | `0x800000` |

The filesystem reservation is currently unused by the application. The table
already has two OTA slots; no repartition is needed merely to demonstrate a
small embedded web page. An OTA table alone does not provide an update service.

Read-only inspection of existing build artifacts (not new builds in Stage 4A):

| Environment | Existing raw `firmware.bin` bytes | Table extent |
|---|---:|---:|
| cores3se_unit | 607,136 | 8 MiB |
| cores3se_base | 607,104 | 8 MiB |
| cores3se_unit_diagnostics | 609,376 | 8 MiB |
| cores3se_base_diagnostics | 609,344 | 8 MiB |

Unit currently has 2,735,200 bytes of raw-image headroom in app0, before adding
network code/assets. This is slot arithmetic, not a forecast of linked Wi-Fi
size. Future packaging must check the final image, not just ELF text size.

`esptool image_info` reports **8MB, DIO, 80 MHz** in the current Unit bootloader
and app headers. The installed PlatformIO builder intentionally maps the QIO
configuration to a DIO image header in this path. Do not change flash mode as
an incidental cleanup. The SDK's generic 4 MB sdkconfig entries do not override
these actual generated headers/table. The app descriptor's SDK build date/name
also does not identify the current controller application source.

### 2.3 What the installed build tools actually use

Exact installed source references, relative to their package roots:

- Platform `builder/main.py:161-192`: derives the maximum application size from
  the selected factory/ota_0 partition. `:202-225` finds filesystem partitions
  by data subtype `spiffs`/`fat`; `:238` defaults the filesystem tool to SPIFFS.
- Platform `builder/main.py:88-99`, `:320-322`, `:456-458`: image mode mapping,
  application image size and upload size flags.
- Framework `tools/platformio-build.py:35-67`, `:70-110`, `:209-215`, `:233-246`:
  partition selection, bootloader generation, extra flash images and table build.
- Framework `tools/sdk/esp32s3/ld/memory.ld:53-80`: IROM/DROM regions. Actual
  framework linking uses the SDK memory/sections scripts; the board manifest's
  linker label is not evidence of a physical 8 MiB limitation.

For a future 16 MiB build, use the installed builder's relevant overrides:

```ini
board_upload.flash_size = 16MB
board_build.partitions = partitions/cores3se_16mb_ota.csv
```

Do not rely solely on an unverified `board_build.flash_size` option. Assert that
generated app/bootloader headers say 16 MB and the maximum application size is
6 MiB, not 16 MiB. Preserve board, PSRAM, USB, pins, baud and driver patches.

### 2.4 FULL images are not data-preserving updates

There is no checked-in, reusable FULL merge/validation script. The ignored
`dist/v0.2.0/publish_release.py` publishes existing assets; it is not a merge
recipe and was not executed. Retained FULL binaries and the installed build
metadata establish the current layout:

```text
0x000000  bootloader.bin
0x008000  partitions.bin
0x00e000  boot_app0.bin
0x010000  firmware.bin
```

For the retained v0.2.0 Unit FULL image, length 665,168 bytes, byte comparisons
confirmed the bootloader, partition table and boot_app0 ranges match the
currently inspected components. Every byte of its NVS range
`0x9000..0xdfff` is `0xff`. A flat FULL upload writes that range and erases
existing NVS contents, even though no explicit NVS image was supplied.

Consequences:

- **Factory/migration FULL** is an explicitly destructive installation artifact:
  it resets NVS in the padded gap and resets OTA selection. It must carry that
  warning. The user's preset-loss waiver does not imply preservation of settings.
- Normal PlatformIO upload writes separate images, avoiding the NVS gap, but
  still includes `boot_app0.bin` at `0xe000`; it can reset OTA boot selection.
  Do not describe that four-image upload as a routine A/B application update.
- A future OTA update writes only the inactive application slot and deliberately
  updates OTA metadata. It must not rewrite the table or boot_app0 every update.
- A FULL file ending after app0 does not erase all remaining 16 MiB. Define
  upper-partition initialization separately; do not silently add whole-chip erase.

Future packaging must derive/validate offsets from the selected build and table,
verify all image ranges and gap bytes, and record the installation's data effects.
Neither filesystem assets nor a meaningful recovery image appear automatically
when merging the four existing components.

## 3. Proposed partition layouts and migration

All sizes use binary MiB/KiB. Both proposed tables were parsed, alignment/overlap
checked and size-checked **in memory** with the installed SDK's
`gen_esp32part.PartitionTable`. Both produce a valid 3,072-byte table; their final
extents are respectively `0x800000` and `0x1000000`. This is structural
validation, not a new build or hardware capacity/migration test.

### A. Conservative 8 MiB layout

Retain the exact current table from section 2.2, eventually copied into a
repository-owned `partitions/cores3se_8mb_ota.csv`. No offsets change. It already
provides 3.1875 MiB per app and a 1.5 MiB storage reservation. Use it for Stage
4B's first asset packaging proof and as a documented fallback.

### B. Recommended full 16 MiB layout

```csv
# Name,Type,SubType,Offset,Size,Flags
nvs,data,nvs,0x009000,0x005000,
otadata,data,ota,0x00e000,0x002000,
app0,app,ota_0,0x010000,0x600000,
app1,app,ota_1,0x610000,0x600000,
spiffs,data,spiffs,0xc10000,0x3e0000,
coredump,data,coredump,0xff0000,0x010000,
```

| Region | Offset | Size | End, exclusive |
|---|---:|---:|---:|
| NVS | `0x009000` | 20 KiB | `0x00e000` |
| otadata | `0x00e000` | 8 KiB | `0x010000` |
| app0 | `0x010000` | 6 MiB | `0x610000` |
| app1 | `0x610000` | 6 MiB | `0xc10000` |
| future storage | `0xc10000` | 3.875 MiB | `0xff0000` |
| coredump | `0xff0000` | 64 KiB | `0x1000000` |

Use the full address range while retaining equal OTA slots and considerable
growth room. Embedded assets count toward each 6 MiB app; reserved data space
is available later without making the initial web UI depend on a filesystem.
The `spiffs` label/subtype is a compatible reservation understood by the current
tools, not a claim that a filesystem is formatted or mounted. Do neither yet.
Twenty KiB NVS is retained to reduce migration scope, not declared sufficient
for unlimited future data or transactional multi-preset imports.

An alternative 16 MiB table could preserve the old lower 8 MiB table and append
an upper data partition. It minimizes moved offsets but leaves OTA slots at
3.1875 MiB. Keep that as a conservative alternative if migration complexity
outweighs future app capacity; it is not the preferred long-term layout.

### Migration and recovery contract

The reported 16 MiB JEDEC/API result is strong evidence of capacity, not a
destructive address-alias/write test. Future hardware validation must verify
actual/configured size and safely test an explicitly selected unused sector
above 8 MiB, checking low critical regions for unintended aliasing. Never use
active app, NVS or OTA selection sectors as scratch space. No such test now.

Moving app1 from `0x340000` to `0x610000` invalidates assumptions in existing
OTA metadata. Old storage/coredump contents also occupy different locations.
Do not update the table through ordinary application OTA. Use a deliberate USB
factory migration with a matching bootloader/table/app0 bundle and known OTA
selection state; validate recovery access before doing it.

Keep the NVS address/size unchanged. For a preservation-oriented migration,
capture a device flash/NVS backup first and write separate ranges that exclude
NVS; validate preset/settings readback. For the accepted preset-loss path, make
all actual reset effects explicit. Do not assume that writing a padded FULL
preserves unrelated settings. The installed Arduino core also erases NVS after
specific initialization failures (`NO_FREE_PAGES`/`NEW_VERSION_FOUND` in
`cores/esp32/esp32-hal-misc.c:250` onward); offset preservation is not a guarantee
against every data loss path.

Rollback across layouts requires restoring the **matching** 8 MiB table,
bootloader, OTA metadata and app bundle, with documented NVS effects. An old
app binary alone under a new table is not the supported recovery procedure.
Retain verified baseline artifacts/checksums off-device; a second OTA slot is
not an immutable factory rescue partition or a substitute for USB recovery.

### OTA readiness is not health-based rollback yet

The actual `qio_qspi/include/sdkconfig.h:27,777` enables bootloader/app rollback.
However, `cores/esp32/esp32-hal-misc.c:204-208,225-236` supplies weak defaults
`verifyOta() == true` and `verifyRollbackLater() == false`: pending firmware is
marked valid in `initArduino()`, **before application setup and DMX health**.

A future OTA stage must deliberately defer confirmation using the correct
`extern "C"` weak-hook override and confirm only after defined local health
checks. Test failed startup, watchdog/reset and power cuts before promising
rollback. Do not implement that hook, an OTA endpoint, signing policy or updater
in Stage 4B/initial web work. Partition and OTA concepts are documented in the
[matching IDF partition guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/partition-tables.html)
and [OTA guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html);
the early-confirmation finding is from the installed Arduino source.

## 4. Asset storage alternatives

| Property | Embedded compressed assets | LittleFS | Hybrid |
|---|---|---|---|
| Flash | Bytes inside each app slot | Separate image/reservation | Recovery assets in app, optional larger data outside |
| RAM | Flash-backed const bytes; bounded send chunks | File handles/cache plus bounded send buffers | Both costs for paths actually enabled |
| PSRAM | Not required to inflate the site | Does not remove internal driver/socket costs | Optional bounded staging only |
| Serving | Stream gzip; browser decompresses | Read bounded chunks; optionally store gzip files | More routing/version checks |
| Build | Generated manifest/table and verified hashes | Deterministic filesystem image and geometry | Two artifact pipelines |
| Update | One app includes matching assets | Requires explicit filesystem deployment | App/storage compatibility contract needed |
| OTA rollback | App/assets roll back together | Single shared FS does not roll back with app | Embedded recovery can survive incompatible FS |
| Recovery | No mount/format failure | Mount/corruption/version failure handling required | Graceful embedded fallback possible |
| Maintenance | Lowest initial complexity | Useful for mutable/large content | Justified later by concrete content needs |

The installed Arduino LittleFS implementation supports a partition labelled
`spiffs`: `libraries/LittleFS/src/LittleFS.h:27` defaults to that label, and
`LittleFS.cpp:64-103` registers the mount. The current 1.5 MiB reservation could
already host LittleFS without a new layout. That proves viability, not necessity.
If selected later, use `formatOnFail=false`, explicit label/geometry, a checked
filesystem image, and a recovery path. Never autoformat user data to conceal a
mount error. `buildfs`/`uploadfs` and explicit merge inclusion would be required;
ordinary `pio run` and the existing FULL recipe do not include it automatically.

Initial recommendation: small vanilla HTML/CSS/JavaScript, gzip compressed and
stored as flash constants, no CDN, remote fonts, telemetry or internet requests.
Use native browser WebSocket. Offline means served through the device AP; a
service worker/PWA is unnecessary and is not assumed to work on plain HTTP IP
origins. A future hybrid can reserve embedded recovery assets plus user fixture
data in storage, with a separately reviewed compatibility policy.

## 5. Module boundaries, ownership and protocol

### 5.1 Current coupling that must be respected

- `src/src.cpp:25-30` constructs Transport, Controller, Dispatcher and views.
  The loop at `:826` onward polls Transport, processes Dispatcher and services
  the existing UI/DMX paths. Do not add socket work to this path.
- `src/dmx_controller.h:21-76` owns working universe, blackout entry restore
  snapshot, preset domain operations and revision. Even reads/storage queries
  belong to the application task.
- `src/view_sender.h:33-35` installs the sole Controller observer and the
  Dispatcher publisher/admission hooks. `:461-490` performs transport admission
  and synchronizes the selected value/redraw after authoritative changes.
  Do not replace these hooks with networking callbacks.
- `src/command_dispatcher.h:6-36` is explicitly application-task-only, including
  `submit()`. Its capacity is 32 and normal processing budget is 4. Its busy
  flag is not a cross-task lock. `executeLocal()` can drain older queued work
  up to capacity; four commands is not a universal per-loop upper bound today.
- `src/command_dispatcher.cpp:132-169` publishes successful mutations and even
  no-change preset loads as appropriate, then reports completion. Admission
  can defer the FIFO head before mutation; preserve that mechanism.
- `src/dmx_transport.h:7-59` owns latest packet, four ordered blackout barriers,
  attempt/generation health and safe driver handoff. Network code must not
  write it, use driver buffers, or infer completion from a WebSocket send.
- `src/preset_store.h` and Controller cleared-mask semantics distinguish
  logically empty, missing, corrupt and failed reads. Keep that distinction.
  Repeated snapshots must not perform eight NVS reads every update tick.
- `src/view_sender.h:3984` onward manages the shared fullscreen canvas and
  diagnostics. It is UI-owned, not a web framebuffer or network scratch buffer.

### 5.2 Proposed modules and public responsibilities

Names are proposals for later phases, not files created in Stage 4A.

| Module/files | Public interface sketch | Owner/data responsibility |
|---|---|---|
| `web_assets.h/.cpp`, generated asset table | `findAsset(path) -> const Asset*` | Immutable flash bytes, MIME/encoding/hash/length |
| `build_info.h/.cpp`, generated metadata | `buildInfo()` | Immutable source/profile/layout/asset/protocol identity |
| `wifi_service.h/.cpp` | `requestStart(config)`, `requestStop()`, copied status | Network worker lifecycle; no M5/Controller/NVS calls in callbacks |
| `web_server.h/.cpp` | `start(assets, bridge)`, `stop()`, bounded HTTP/WS handlers | HTTP server task, sessions and send-buffer lifetimes |
| `web_protocol.h/.cpp` | `decode(bytes, limit)`, `encode(snapshot/result)` | Validated wire DTOs; no domain mutation or global state |
| `web_command_bridge.h/.cpp` | network `tryEnqueue(envelope)`; owner `drain(budget)` | Fixed cross-task command/completion queues and client credits |
| `controller_snapshot.h/.cpp` | owner `capture()`, network `takeLatest()` | Immutable copied projection, never a second mutable controller |
| `network_service.h/.cpp` | bounded lifecycle/outbox work | One worker for blocking network lifecycle/HTTP work scheduling |

All Controller, Dispatcher, preset and Transport access stays on the current
application task. HTTP/WebSocket handlers enqueue owned typed payloads using
zero-wait bounded queues. They never call `dispatcher.submit()`, read
`workingPacket()`, call M5, change the display or persist settings themselves.

Do not steal Controller's single observer. Retain its current UI synchronization;
use the currently unused Dispatcher completion hook for an owner-side fanout,
plus owner-side revision/status sampling. Keep publisher and admission hooks
unchanged. Cache derived preset occupancy on owner startup/storage changes;
retain errors, never turn corrupt slots into empty ones. Snapshot caches have
no authority to mutate or restore the Controller.

### 5.3 Command and state flow

```text
touch -> existing executeLocal/Dispatcher path -------------------+
                                                               v
WS handler -> validate -> bounded ingress -> owner drain -> Dispatcher
                                                            | admission
                                                            v
                                                       Controller
                                                            | existing publisher
                                                            v
                                               Transport -> patched esp_dmx

owner completion/revision/status -> copied snapshot + ordered results
                    -> network outbox -> HTTP server task -> browser
```

Start with two authenticated WS clients, one outstanding mutation credit per
client, ingress capacity 8 and a completion pool of 8. These are proposed caps;
test actual `sizeof` and queue allocation, and reject excess before admission.
Reserve a completion slot before accepting a command. A full queue returns
`busy`; it must not mutate and then discover it cannot report the result.
Use owned command payloads (`include/controller_types.h`, max patch 8 channels),
not browser pointers or strings retained by the owner.

The owner drains only a bounded amount at the existing loop boundary and uses
Dispatcher admission/FIFO rules. Define one global serialized execution order
across touch and web. Preserve the current synchronous local execution behavior
initially; rate-limit ingress so it cannot turn older-command draining into a
burst of expensive NVS writes. Queue rejection/defer is an explicit outcome.

Ordinary latest-state broadcasts can coalesce. Do not coalesce accepted
relative commands, preset operations, or blackout ON/OFF edges. Preserve the
four-barrier transport preflight and defer/reject before mutation when full.
Browser slider producers may coalesce absolute values **before acceptance**;
the final value must remain queued/retried or visibly rejected. Do not silently
drop the final drag value, or place an OFF before its accepted ON.

Snapshots use a single-element overwrite mailbox of copied, bounded DTOs; the
network task takes its own copy. No mutex spans rendering, NVS, socket sends or
Controller access. Ordered completion messages use a separate queue and are
never overwritten. Keep at most one scheduled HTTP send work item and a bounded
per-client result backlog. Send buffers remain owned until the send call/work
has actually finished. Associate sessions with a generation as well as socket
descriptor to prevent descriptor-reuse delivery errors.

### 5.4 Explicit blackout and acknowledgement semantics

Current behavior is preserved (`dmx_controller.cpp:68-86`): ON snapshots the
working universe; effective output becomes zero. Working edits/preset loads
during blackout do not affect output and are discarded when OFF restores the
entry snapshot. Web must display both blackout and working state consistently,
and explain this restore behavior. It must not introduce a different web-only
blackout or silently reinterpret OFF as retaining edits.

Use explicit `blackout.set(active)` rather than a network toggle. Successful
application and transport staging are distinct from qualified TX completion,
and neither proves a fixture received a frame. Preserve blackout barrier order
even if a later state snapshot coalesces past the transient ON state. A web
client receives ordered command results, while current snapshots describe the
latest state. Transport health/generations can report software progress, not
RS-485 electrical correctness.

### 5.5 Initial protocol proposal

Use `/api/v1/session`, `/api/v1/state` and `/ws/v1`; no HTTP GET mutations.
Start with full snapshots at up to 20 Hz while dirty rather than 512 separate
messages. A proposed outbound encoded-frame cap is 4,096 bytes; prove the
maximum snapshot fits, including all 512 values and metadata.

```json
{"v":1,"id":42,"op":"channel.set","channel":21,"value":127}
{"v":1,"id":43,"op":"channels.set","values":[[21,128],[22,9]]}
{"v":1,"id":44,"op":"blackout.set","active":true}
{"v":1,"id":45,"op":"preset.load","slot":1}
```

The server overwrites source as `external`; client-supplied source/system
privileges are rejected. Browser slot 1-8 maps explicitly to internal 0-7.
Channel range is 1-512, value 0-255; start code is fixed zero and never writable.
Reject floats, duplicate fields, unknown operations, invalid types, excessive
nesting, invalid patch counts/duplicate channels and out-of-range values.
Initially reject fragmented application messages and messages over 512 bytes
before allocation. Specify UTF-8/JSON and close/error policy; test control
frames separately. HTTP request/body/header limits and session limits also
need explicit configuration, not just a WS payload check.

Stage 6 must choose a bounded parser. Prefer a pinned fixed-capacity parser
with nesting/token limits; do not assume an unrestricted JSON DOM is safe
because the input is short. No new JSON dependency is required in Stage 4B/5.
Use bounded streaming output/preallocated encoding, with tests proving worst
case sizes. Parser choice and exact capacity are a Stage 6 design gate.

Handshake returns protocol version, boot epoch, firmware/profile/layout ID,
asset bundle hash, capabilities and a coherent initial snapshot before controls
are enabled. Snapshot includes 512 working values, blackout flag/output rule,
logical preset states and transport status. A separate owner state sequence
covers status changes not represented by Controller revision. Encode 64-bit
revision/generation values as decimal strings to avoid JavaScript precision loss.

Results identify request ID, owner sequence, Dispatcher status/domain result,
revision and transport staging outcome. An optional later progress message can
identify qualified transmission completion. No wire-level acknowledgement is
claimed. Identical preset loads still get a result/publication even if revision
does not change. Empty/corrupt/error slots remain different outcomes.

On reconnect send a fresh snapshot; do not automatically replay unknown-result
relative/preset commands. Use bounded per-session duplicate-ID handling and
explicit boot/session epochs. A protocol/asset mismatch disables controls and
requests reload rather than guessing. Initial synchronization and all accepted
command results must be testable under disconnect, queue-full and retry cases.

Touch selection, web selection, STEP and unsent keypad drafts are client-local;
web edits name an absolute channel and never depend on the touch selection.
Proposed multi-operator policy is last serialized accepted mutation wins, with
all clients receiving authoritative state. That policy needs user acceptance.

## 6. Networking, memory and scheduling risks

### 6.1 What is measured, configured or still unknown

| Evidence class | Value/finding | Limitation |
|---|---|---|
| Hardware report | Physical flash 16,777,216 bytes; PSRAM about 8 MiB | Not an upper-address write/alias test |
| Stage 2 hardware report | Internal free after canvas 159,372 bytes; largest DMA block 131,060 bytes | Historical, before current Stage 3 plus networking |
| Stage 2 hardware report | Free PSRAM 8,385,947 bytes; one canvas at `0x3fca3910` | Do not assume that canvas resides in PSRAM |
| Stage 0 comparison | Internal free after canvas 161,484 bytes | Different baseline; not spare Wi-Fi budget |
| Existing Stage 3 build analysis | App static overhead +2,832 bytes, driver object +56 bytes, IRAM +508 bytes | Not a measured new runtime heap watermark |
| Canvas arithmetic | 320 x 240 x 2 = 153,600 pixel bytes | Allocation/graphics metadata adds cost |
| Installed SDK configuration | Wi-Fi RX/TX pools, stacks, socket cap below | Total live/internal consumption requires measurement |
| Proposed product caps | Two stations/WS clients; 512-byte ingress, 4 KiB egress, 20 Hz state cap | Not proven capacities |

Do not subtract static growth from a historical free-heap number and call that
a new measurement. There is no Stage 4 Wi-Fi build or runtime heap result yet.

The selected `qio_qspi/include/sdkconfig.h` enables static RX count 8, dynamic RX
maximum 32, static TX count 8, Wi-Fi task on core 0, lwIP task on core 0 and a
global lwIP socket cap of 16 (`:400-415,496,542-544`). Buffer counts alone do not
establish total Wi-Fi memory. PSRAM malloc/Wi-Fi-lwIP allocation attempts are
enabled, with small-allocation internal preference and zero configured internal
reserve (`:314-317`). Not all buffers, DMA allocations, task stacks or IRAM can
be moved to PSRAM. Do not tune precompiled libraries by adding arbitrary
sdkconfig defines to this Arduino build.

Known stack settings: Arduino loop 8,192 bytes; Arduino event task 4,096;
system event task 2,048; lwIP 2,560; HTTP default 4,096. These are configured
stack capacities, not a sum of every new allocation caused by networking.
Measure actual task creation, stack high-water marks and peak heaps.

Ensure the existing shared canvas can be allocated **before enabling Wi-Fi**.
It is currently allocated lazily. A later owner-side reservation helper can
allocate the same sprite without changing page ownership or drawing another
screen; it must not create a second canvas or silently move it to PSRAM.
If critical UI allocation cannot be preserved, leave web disabled and retain
local control. Merely checking total free heap is insufficient when a large
contiguous block is required.

Budget application queues/snapshots from actual `sizeof` and FreeRTOS overhead.
Use fixed pools and copied snapshots, not per-command allocations. Optional
large JSON/output scratch space can be explicitly allocated in PSRAM and held
by the network task. Failure must disable/reject web work, not fall back to
consuming the internal block needed by the canvas. No blanket claim that
"8 MiB PSRAM is enough" establishes internal/DMA safety.

### 6.2 HTTP task and slow-client isolation

The installed bundled `esp_http_server` supports WS
(`sdkconfig.h:342`; header under
`tools/sdk/esp32s3/include/esp_http_server/include/esp_http_server.h`). It has
its own task; defaults include priority idle+5, 4 KiB stack, no core affinity,
seven open sockets and five-second send/receive timeouts (`:26-39`). Configure
explicit smaller limits and finite timeouts; initially propose core 0, at most
six server sockets and two WS clients, measuring actual global socket usage.
Do not allocate all 16 lwIP sockets to web clients.

The name `httpd_ws_send_frame_async()` does **not** establish a nonblocking
socket send. Schedule work via the HTTP server context, with immutable payload
lifetime, from a separate NetworkService worker; never call it or
`httpd_queue_work()` from the application loop. Even queuing/stopping services
can block. At most one outstanding send work item, per-client credits, bounded
backlogs and finite send timeouts prevent unbounded memory growth. A slow peer
may delay other HTTP clients while the server is sending; close it on timeout
or sustained backlog and require resynchronization. It must not delay the
application owner waiting for network progress.

The architecture includes the HTTP task **and** a small network worker, in
addition to Wi-Fi/lwIP/event tasks; budget them honestly. Use the worker for
start/stop, lifecycle events, outbox pacing and scheduling sends, not domain
mutation. Static asset responses send bounded flash chunks with correct MIME,
gzip negotiation and finite I/O limits; no whole-site Arduino `String` copy.
See the [matching HTTP server API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/esp_http_server.html)
and [v4.4.7 WS implementation](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_ws.c).

### 6.3 Scheduling and measurement gate

Current Arduino loop/event placement is core 1; the Arduino Wi-Fi event task
can run there at higher priority (`WiFiGeneric.cpp:578`). Keep callbacks O(1),
with only copied events/queue notifications. Propose HTTP/worker placement on
core 0, leaving the existing application/DMX ownership and ISR setup unchanged.
Core placement is not hard isolation: cache, flash access, memory, locks and
interrupt contention remain shared. Preserve Stage 3's current approximately
188 microsecond framing busy-wait. No timer/driver redesign in web stages.

Existing preset NVS writes remain synchronous owner work. Remote save/copy/clear
can increase gaps if admitted in bursts. Introduce preset LOAD first in Stage
6A; expose persistent mutations only in Stage 6B after rate/latency tests,
initially one storage operation at a time with a proposed one-per-second cap.
Do not move NVS/Controller into a background task as an accidental fix.

Measure before and after radio initialization, server start, one/two clients,
asset downloads, WS saturation, slow peers, reconnect loops, page transitions,
sleep/wake and preset operations:

- Internal free/minimum heap, largest internal 8-bit and DMA blocks, PSRAM
  free/minimum where supported, allocation failures and task stack watermarks.
- Maximum owner-loop service gap, command latency/queue depth, result drops
  (must be zero), final-value delivery, Transport errors/retries/fault state.
- Canvas allocation and identical ownership/pointer reuse, no sustained memory
  decline after repeated connect/disconnect/navigation cycles.
- Wi-Fi initialization failure/recovery without reset or loss of local control.

Freeze numeric heap/reserve/stack/client limits from measured worst-case peaks
before widening scope. A safe gate is not an invented universal free-heap
number: largest remaining blocks must exceed measured required allocation sizes
plus an agreed margin, and task watermarks must cover measured worst paths.
Record those numbers explicitly at the next phase's exit.

Firmware logs can establish software queue, generation, completion and timing
observations. External RS-485 capture/logic analysis is needed for actual break,
MAB, 513-slot packet length, interframe gaps, frame coherence under Wi-Fi load
and electrical signal quality. Do not claim wire integrity from software logs.
Keep experimental Base glitches a separate hardware/transport concern.

## 7. SoftAP security, provisioning and recovery

Use AP-only mode initially, explicit `192.168.4.1/24` with DHCP, and the URL
`http://192.168.4.1/`. No existing router, internet, STA routing, NAT, cloud,
NTP, remote resources or public exposure is needed. Explain phone "no internet"
warnings. Defer captive DNS and mDNS; manual IP access is the recovery path.

The installed `WiFiAP.cpp:102-160` configures WPA2-PSK with CCMP for a nonempty
password; null/empty password creates an open AP. Prevent that fallback.
WPA3-related SDK symbols do not prove this wrapper provides WPA3 SoftAP;
stronger authentication requires exact API/client verification in a later task.

Proposed SSID: `CoreS3SE-DMX-<device suffix>`, at most 32 bytes. Generate a
distinct random password per device, at least 16 characters within the supported
8-63 ASCII-character PSK range. MAC/SSID suffixes are identifiers, not secrets.
Do not use a shared public default password. Provision it through a deliberate
local display/Settings interaction or supplied device label; approve the new
interaction separately so current controls are not repurposed silently.

Store versioned credentials once in a dedicated NVS namespace such as `dmxwifi`,
with writes owned by the application task. `WiFiGeneric.cpp:763` defaults
persistence on; call `WiFi.persistent(false)` **before** network initialization
so SDK configuration uses RAM (`:694`) while the application owns persistence.
Do not write credentials on every reconnect. Invalid/missing storage must lead
to physical provisioning/network-disabled state, never an open AP.

Forgotten credentials: a confirmed physical recovery action resets only the
Wi-Fi namespace and generates new credentials. Design that action around the
existing touch/sleep controls; do not change boot GPIO/USB recovery or erase
all NVS. Report failures locally without unbounded logging/retry loops. Network
startup failure must leave touchscreen/DMX operational; bounded worker retry
or an explicit retry request is preferable to application reboot.

Initially cap AP stations and authenticated WS clients at two. Treat the shared
PSK/session as control authorization, with a documented two-operator policy.
HTTP over WPA2 is a local trusted-network model, not end-to-end HTTPS or
protection against an authorized malicious operator. Do not expose OTA,
filesystem upload, shell/debug services or arbitrary NVS APIs initially.

Web access safeguards:

- Strict Host and WebSocket Origin allowlists; no permissive CORS or DNS
  rebinding acceptance. Allow only documented device origins, including any
  optional mDNS name when actually enabled.
- Same-origin session bootstrap with a random RAM session, bounded expiry and
  boot epoch; an HttpOnly, SameSite=Strict cookie is practical for browser WS.
  `Secure` cookies cannot be assumed on plain HTTP. Avoid query-string secrets
  and unsupported assumptions about custom browser WS Authorization headers.
- CSRF protection/Origin checks for state-changing HTTP bootstrap actions;
  all domain mutation still goes through the WS bridge. Rotate sessions after
  credential reset/reboot; never log credentials or tokens.
- CSP for self-hosted resources, no inline event handlers/eval, frame blocking,
  nosniff, validated paths and explicit request/rate/client limits.

## 8. Reproducible assets, identity and FULL packaging

### Asset pipeline

Proposed inputs: `web/src/index.html`, `app.js`, `style.css`. Keep the initial
site small; propose a **256 KiB total gzip budget**, subject to actual linked
size verification. This is a cap, not a measured asset size.

Use a repository script `scripts/build_web_assets.py` with explicit file order,
UTF-8 policy, empty gzip filename, fixed mtime and compression level. Pin the
regeneration Python/zlib toolchain when byte-identical compressed output is
required; mtime=0 alone does not guarantee identical bytes across compressors.
No CDN downloads or package-manager resolution during firmware builds.

Check in generated gzip files and a manifest containing source/output SHA-256,
MIME, encoding, URL and protocol/asset version. Normal clean builds verify
freshness and hashes without rewriting tracked files or requiring Node. An
explicit regeneration/check command must reproduce the committed bundle.
Generate the C++ asset table under `$BUILD_DIR/generated` with explicit SCons
dependencies and flash const storage. Assert retention in the final ELF/BIN:
unreferenced asset objects can otherwise be dropped by `--gc-sections`, especially
in Stage 4B before HTTP exists. Use an explicit linker retention root, not an
unrelated UI/runtime call to keep assets alive.

Use content-hashed CSS/JS URLs with immutable caching/ETags; serve root HTML,
session and state with appropriate no-store policy. Negotiate gzip correctly
and retain stable MIME types. The bundle identity is embedded with protocol
and build metadata so a stale browser fails closed/reloads instead of issuing
commands using incompatible protocol assumptions.

### Accurate provenance

Replace stale diagnostic provenance in a **future approved change**, using a
generated build identity: commit, dirty status plus relevant source digest,
hardware profile, layout ID, asset hash, protocol version and tool versions.
Do not substitute the precompiled SDK app descriptor for application provenance.
Development builds may describe a dirty Stage 3 tree truthfully; future release
packaging should reject dirty/mixed source artifacts. Stage 4A changes no
diagnostic behavior. Stage 4B may generate metadata without changing the GUI;
updating diagnostic output is a separately explicit part of its approved scope.

### Packaging contract

Create a reusable `scripts/package_firmware.py` and layout validator/tests.
Take the selected environment's bootloader, actual generated table, OTA init
image, app and PIO flash metadata; reject missing/stale/mixed-layout components,
wrong profile, overlapping ranges, overflow, invalid headers or app larger
than either OTA slot. Use the pinned installed esptool merge mechanism and
verify the resulting binary byte ranges, padding/data effects and hashes.
Manifest records source identity, environment, flash size/mode/frequency, table
hash, offsets, component sizes/hashes, asset identity and factory-reset effects.
Keep Unit/Base and diagnostics distinct. Do not publish these development images.

The embedded design needs **no filesystem flashing**: assets are already in
`firmware.bin` and therefore in FULL. If a filesystem is chosen later, add
its image explicitly at the table-derived offset and define app/FS version
compatibility; a shared single FS breaks automatic paired asset rollback.

Maintain all four current build environments, shared dependency blocks,
default Unit selection and both driver patch hooks. Use opt-in inherited
16 MiB variants for Stage 4B validation without duplicating dependencies.
Once authorized hardware tests pass, a later deliberate default switch can
apply the shared 16 MiB settings to both profiles and diagnostic variants.

## 9. Staged implementation and acceptance

Each stage keeps historical files, logs, existing stash and uncommitted Stage 3
intact. Do not create a branch from HEAD alone and accidentally omit the Stage 3
candidate. Begin later work only with an explicit inventory/safe isolation plan;
no automatic stash/reset/clean, commits or branch mutation in this design task.

### Stage 4B1: embedded packaging on the existing layout

Scope/files: asset sources/generated bundle, asset generator/verifier,
flash-const table and retention, generated build metadata, partition/package
validator and host tests, checked-in copy of current 8 MiB CSV if needed.
PlatformIO hooks may change only after approval. No Wi-Fi symbols/tasks,
Controller/UI/Transport changes, filesystem mount or OTA endpoint.

Dependencies: current platform/libraries and pinned regeneration tooling only;
no new firmware library needed. Host tests cover deterministic generation,
stale/missing asset rejection, correct content/encoding, hash consistency,
linker retention, partition bounds and mixed artifact/overflow rejection.

Build all four normal/diagnostic Unit/Base environments, run existing
Controller/Dispatcher/transport and patch tests, and `git diff --check`.
Check final binary size against both app slots and quantify linked RAM/flash
delta. Hardware testing, only on later explicit upload authorization, checks
unchanged cold start, canvas/UI, battery/sound, sleep and DMX behavior. No
network resource claim yet. Rollback removes only the new packaging delta;
device rollback uses its unchanged matching-layout firmware bundle.

### Stage 4B2: opt-in 16 MiB table/build/migration

Scope/files: proposed CSV, inherited opt-in Unit/Base plus diagnostic variants,
layout assertions, package manifest/recovery instructions. No radio/filesystem
or health-confirmation implementation. Do not change working default builds
until authorized migration succeeds.

Host/build tests: SDK table validation; exact offsets/alignment/no overlap;
both app slots 6 MiB; end 16 MiB; NVS unchanged; app/bootloader headers 16 MB;
no flash/PSRAM/USB/pin regression; all profile/diagnostic builds and FULL range
checks. Size/reserve limits apply to each artifact, not the whole chip.

Hardware gate: explicit migration approval and data-loss notice; known USB
recovery path; optional device backup; validate reported/table/header sizes,
authorized unused upper-sector capacity/alias test, cold boot with both power
configurations, preset/settings behavior, UI/canvas and DMX regression. OTA
slot reservation is verified structurally; real A/B health rollback remains a
later feature test. Roll back with a matching 8 MiB full recovery bundle.

### Stage 5: SoftAP and read-only static HTTP

Scope/files: WiFiService, NetworkService, WebServer, local provisioning/status
integration and minimal read-only resource diagnostics; retain embedded assets.
No WS domain mutations. Dependencies: bundled `WiFi` and `esp_http_server`.

Host tests: lifecycle failure paths, credential validation, path/MIME/gzip/cache
rules, Host/Origin/session policy, bounded request handling, stale assets,
allocation failure and socket/backlog caps. Re-run existing tests/four builds.

Hardware: allocate/reserve the one canvas first; measure radio/server memory,
stack high-water marks, one/two clients, repeated downloads, slow peers,
connection churn, no-internet phone access, physical recovery and network-start
failure. Repeat UI/presets/battery/speaker/sleep and Stage 3 transport tests;
capture RS-485 under network stress separately. Establish numeric internal/DMA
reserve and stack gates from results before proceeding. No sustained leaks,
owner waits for networking or unbounded retries. Rollback disables/removes
network modules without changing partition layout or preset format.

### Stage 6A: WS synchronization and nonpersistent controls

Scope/files: protocol/parser, typed bridge, copied snapshots, session/sequence
handling, owner integration, browser controls for 512 channels, precision edits,
blackout and P1-P8 LOAD/status. No authoritative state in network tasks; no
second canvas. Initial parser dependency/capacity must be chosen and pinned.

Host/integration tests: bounds/types/invalid JSON/oversize/fragmentation,
start-code zero, sender/receiver separation, interleaved touch/web, initial
sync, revision/no-change result, duplicate IDs, reconnect ambiguity, queue full,
completion credits, slow-client disconnect, final slider value, blackout ON/OFF
barriers under backpressure, presets during blackout and immutable active TX.
Verify callbacks never call Controller/Dispatcher/Transport directly.

Hardware: all four builds and existing tests, touch/web simultaneous operations,
one/two clients with flood/reconnect/slow-reader stress, no sustained memory
decline, measured loop/command delays, no accepted result loss, consistent
snapshots and preserved local behavior. External capture must check frame
coherence/length/gaps during floods and blackout/preset edges. Freeze tested
limits and timing acceptance bounds from the baseline/capture, not invented
hard real-time guarantees. Rollback to Stage 5 preserves data and layout.

### Stage 6B: persistent preset controls

Scope: bounded/rate-limited P1-P8 SAVE/COPY/CLEAR through existing Dispatcher
and NVS adapter, only after storage latency/resource acceptance. No NVS format
migration. Test logical cleared slots, missing/corrupt/error distinctions,
failed writes/metadata, occupancy synchronization, permissions and rate limits.
Hardware tests include NVS error/power interruption behavior and output gaps
during writes with external capture. Reject unsafe load rather than hide errors.
Rollback disables remote persistence without deleting saved data.

## 10. Implementation-ready Stage 4B prompt

Use only after the user approves this design and resolves the listed gates:

> Continue from the exact current Stage 3 working tree. Read this design and
> all Stage 3/USB reports. Inventory status, diffs, branch, stash and existing
> artifacts first. Preserve the uncommitted Stage 3 candidate, historical
> directory, logs and stash. Do not reset, stash, clean, pull, merge, commit,
> push, tag, release or upload. If isolation would omit/overwrite the candidate,
> stop and report rather than proceeding from HEAD alone.
>
> Implement Stage 4B1 first: deterministic embedded gzip HTML/CSS/JS assets,
> checked generated bundle/manifest, a build-time freshness verifier and
> generated flash-const lookup/identity metadata, with explicit linker retention.
> Use a minimal offline placeholder page; no control UI or network APIs yet.
> Keep the current 8 MiB defaults and both normal/diagnostic hardware profiles.
> Add host tests and reusable partition/FULL packaging validation. Derive
> offsets/components from the actual selected build. Reject stale/mixed images,
> overflow and inconsistent asset/source hashes. Document FULL NVS/OTA reset
> effects. Never run the existing publish script.
>
> Then implement Stage 4B2 as opt-in inherited 16 MiB Unit/Base and diagnostic
> build variants using exactly the reviewed table: NVS 0x9000/0x5000, otadata
> 0xe000/0x2000, app0 0x10000/0x600000, app1 0x610000/0x600000,
> spiffs reservation 0xc10000/0x3e0000, coredump 0xff0000/0x10000.
> Use board_upload.flash_size=16MB and the repository CSV, preserving all other
> working board/PSRAM/USB/driver settings. Do not switch defaults, mount/format
> a filesystem, erase or flash a device. Validate app/bootloader headers, table,
> slot capacities and merged artifact byte ranges. Present migration/recovery
> instructions and data effects for separate hardware approval.
>
> Do not implement Wi-Fi, HTTP, WS, OTA, deferred OTA confirmation, backup/import,
> Controller changes or transport/timing/UI changes. Generate truthful build
> identity without changing GUI/diagnostic output unless explicitly approved.
> Keep development artifacts identified as dirty candidates, not releases.
> Run existing host/integration/patch tests and all four current PIO builds,
> plus all new 16 MiB variants; run git diff --check. Report files, tests,
> asset and firmware sizes, RAM/IRAM deltas, header/table/package verification,
> status and remaining hardware gates. Stop without committing or uploading.

## 11. Future offline Web Preset Backup & Restore

Record for a later, separately approved stage after initial WS control; no
Stage 4 implementation and no dependency on this feature for the accepted
migration preset-loss path.

Export a consistent owner-produced snapshot of all P1-P8 as a browser download
over the AP (for example a Blob/JSON file). No cloud or LittleFS is necessary.
Use an envelope with format identifier, schema version, universe size 512,
start code zero, creation/source metadata and slots 1-8. Occupied slots carry
exactly 512 integer values. Logically cleared/missing slots remain empty without
exporting stale hidden blobs; corrupt/error slots are reported explicitly.
Never include raw NVS, credentials or unrelated settings.

Import uses a proposed bounded file size (64 KiB, to be proven against schema),
schema/version checks, unique slot numbers, exact channel lengths, integer
bounds and supported metadata. Reject unknown incompatible versions; retain
explicit migration functions/tests for older supported schemas. Validate the
entire file before any persistence, show a preview/confirmation and execute
bounded owner operations. Import must not automatically load output or alter
blackout. Export revision/status must make concurrent edits unambiguous.

Existing multi-key preset blobs/cleared mask do not provide all-eight atomic
restore. Do not claim atomicity from prevalidation. Before implementing import,
choose and test partial-failure reporting/rollback or a versioned journal/bank
scheme, including power loss and NVS capacity. That is a separate storage
design, not permission to enlarge NVS or change preset format now.

## 12. Exit record for this design task

Only this Markdown report is added. No new firmware build, hardware measurement,
partition write, upload, dependency installation or publication is represented
as having occurred in Stage 4A. Existing binaries were read, and proposed CSVs
were validated in memory with the installed SDK parser.

Next recommendation: approve embedded assets and the displayed 16 MiB table,
then execute Stage 4B1 followed by opt-in Stage 4B2. Keep radio/controller
integration gated behind Stage 3 endurance and the subsequent measured resource
and external capture tests. Wait for approval before implementing any of it.
