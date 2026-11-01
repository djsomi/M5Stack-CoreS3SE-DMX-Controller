# Stage 5A: SoftAP memory failure diagnosis

Finalization note (2026-10-25): this is historical investigation evidence.
The failure was later confirmed as guard.after_http and corrected by the Stage
5B PSRAM framebuffer change. The user has now hardware-accepted Stage 5C,
including iPhone cellular coexistence and three concurrent clients. Verbose
startup tracing is OFF by default in the final source; /status diagnostics
remain enabled. See STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md for final state.

## Conclusion and evidence boundary

The displayed `257` is the actual `Status.error` integer (`0x101`,
`ESP_ERR_NO_MEM` in the installed SDK). **It does not prove that
`esp_wifi_init()` returned that error.** Stage 5 also synthesizes this error for
its own guards and task/netif allocation failures. The exact first failing API
or guard is not yet established by a hardware trace.

The user reports a working display/Settings, initialized PSRAM, WIFI FAILED (257),
internal free approximately 85,804 bytes and displayed DMA approximately 37,876
bytes. These are the only new hardware measurements. The existing Settings
`DMA` value is **largest internal DMA-capable block**, not total free DMA heap.
No new firmware was uploaded, no port opened and no runtime values invented.

A plausible early failure is the application preflight guard: it requires a
49,152-byte contiguous internal 8-bit block, in addition to 81,920 total internal
bytes and a 32,768-byte largest DMA block. A 37,876-byte largest DMA block is
compatible with a fragmented internal heap that fails the 49,152-byte test.
However, the largest internal 8-bit block has not been supplied; the two largest
block values must not be assumed identical. The new trace will settle this.

**Current change: temporary bounded instrumentation only. No allocator, Wi-Fi
buffer count, memory guard, partition, credential or production transport fix
has been applied.** Wait for the first-error capture before choosing a correction.

## Where 257 can originate

Source: `src/network_service.cpp`, `Status.error` in
`include/network_service.h`, and the marked Settings block in
`src/view_sender.h`.

| Path | Existing behavior | New evidence label |
| --- | --- | --- |
| UI reservation failed | Synthesized NO_MEM; worker never created | guard.canvas |
| Admission limits fail | Synthesized NO_MEM; no Wi-Fi API called | guard.preflight |
| Worker task creation fails | RTOS result converted to NO_MEM | xTaskCreatePinnedToCore |
| Worker initial memory check fails | NO_MEM before netif/Wi-Fi initialization | guard.worker.entry |
| esp_netif_new returns null | Converted to NO_MEM | esp_netif_new |
| Checked SDK API returns NO_MEM | Underlying esp_err_t retained, subject to existing readback checks | exact API name |
| AP/DHCP succeeded, memory reserve fails | Synthesized NO_MEM before HTTP | guard.after_ap |
| HTTP setup succeeded, reserve fails | Synthesized NO_MEM | guard.after_http |
| Runtime heap or task stack guard fails | Synthesized NO_MEM, network teardown | guard.runtime.heap / guard.stack |

The API wrapper records the raw return before cleanup or a later generic
verification error can replace the displayed value. Expected idempotent
`ESP_ERR_INVALID_STATE` for an already-created default event loop and
already-stopped DHCP are recorded without treating them as first failures.
The original DHCP-stop return handling is otherwise unchanged. Unexpected
nonzero results are evidence even if the original caller ignored that result;
read the following events to distinguish that from the operation that aborted
startup. Verification failures have their own labels.

`guard.preflight detail` is a bit mask computed from the exact guard inputs:
1 = total internal below 81,920; 2 = largest internal below 49,152;
4 = largest DMA below 32,768. Multiple failed checks combine their bits.
The original comparisons and aborts remain present. A task allocation record's
detail contains its original FreeRTOS return; before/checkpoint records with
`rc=0` do not assert that an API has already succeeded.

The error visible after teardown can accompany memory that has already been
freed. It is not a measurement of heap at the instant the allocation failed.
The retained first-error snapshot is captured before teardown.

## Installed implementation and memory contributors

Environment remains platform `espressif32@6.7.0`, Arduino ESP32 2.0.16,
ESP-IDF 4.4.7, M5Unified 0.2.21 and M5GFX 0.2.28, esp_dmx v2.02 with both
reproducible patches. The 16 MiB/two 6 MiB app-slot layout is unchanged.

### Canvas allocation is a confirmed contributor, not yet a confirmed failure

Installed paths below are relative to `.pio/libdeps/cores3se_unit/M5GFX/src/`:

- `lgfx/v1/LGFX_Sprite.hpp:396`: `_psram` defaults false.
- `lgfx/v1/LGFX_Sprite.cpp:58-79`: `createSprite` chooses
  `AllocationSource::Dma` when that flag is false.
- `lgfx/v1/misc/SpriteBuffer.cpp:116-156`: that source calls `heap_alloc_dma`;
  explicit Psram uses the PSRAM allocator, with a DMA fallback on failure.
- `lgfx/v1/platforms/esp32/common.hpp:148-151`: the DMA allocator calls
  `heap_caps_malloc(..., MALLOC_CAP_DMA)`; PSRAM explicitly requests
  `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`.

`view_sender_t::getFullscreenCanvas()` sets 16-bit depth but does not enable
PSRAM. At 320 x 240 it asks M5GFX for 153,602 bytes, rounded by the allocator
to **153,604 bytes**, plus heap metadata. Successful PSRAM initialization does
not redirect this explicit DMA request. Reservation still creates one shared
fullscreen canvas, reused by Settings, Precision, Keypad and Presets.

Sender also reserves its two existing 50 x 54 cell sprites. Receiver reserves
its existing 320 x 88, 8-bit console sprite (28,164 rounded bytes plus metadata).
Those reservations and their lifetimes are unchanged in Stage 5A. Their separate
checkpoints prevent attributing the Receiver/cell cost to the fullscreen buffer.

Controller, Dispatcher and transport state use existing bounded static storage;
their initialization is already included in the first setup snapshot. There is
no runtime Controller heap-construction API to bracket. `dmx.startup.before` and
`.after` bracket the original configuration/pin/driver-install sequence.
Installed `esp_dmx/src/esp_dmx.c:128-176` allocates its object, queue, packet
storage and semaphore from the existing internal/RTOS allocators. None is moved
or reconfigured here. A `.after` checkpoint reports memory, not a newly added
assertion about a return value the old application did not inspect.

### Exact Wi-Fi defaults

Installed SDK configuration:
`framework-arduinoespressif32/tools/sdk/esp32s3/qio_qspi/include/sdkconfig.h:305-317,398-423`.
Initializer/field definitions:
`.../include/esp_wifi/include/esp_wifi.h:88-246`.

| Field/config | Current value |
| --- | ---: |
| static_rx_buf_num | 8 |
| dynamic_rx_buf_num | 32 (cap, not 32 permanently preallocated full buffers) |
| tx_buf_type | 0, static |
| static_tx_buf_num | 8 |
| dynamic_tx_buf_num | 0 |
| cache_tx_buf_num | 16 |
| rx_mgmt_buf_type / count | 0 / 5 |
| mgmt_sbuf_num | 32 |
| ampdu_rx_enable / rx_ba_win | 1 / 16 |
| ampdu_tx_enable | 1 |
| nvs_enable | 0, existing application override |
| Wi-Fi task core | 0 |
| SPIRAM_TRY_ALLOCATE_WIFI_LWIP | 1 |
| SPIRAM_MALLOC_ALWAYSINTERNAL / RESERVE_INTERNAL | 4096 / 0 |

The source still uses `WIFI_INIT_CONFIG_DEFAULT()` with **only the same
`nvs_enable=0` override**. Three trace configuration records pack four values
each, low byte first, into `detail` (also printed in hex):

- `cfg.srx_drx_stx_cache`: static RX, dynamic RX, static TX, TX cache.
- `cfg.txType_dtx_mgmt_nvs`: TX type, dynamic TX, management short buffers, NVS.
- `cfg.ampduRx_ba_mgmtType_count`: AMPDU RX, RX BA window, RX management type/count.

HTTP remains core 0/priority 3/8192-byte stack/3 accepted sockets; worker stack
is 8192 bytes at priority 1/core 0. `http.stack_sockets` detail packs stack bytes
in the low 16 bits and socket count in the high 16. No new task is introduced.

Static Wi-Fi RX/TX buffers need internal DMA memory; the documented approximate
1,600 bytes per buffer makes the existing 8+8 pool about 25.6 KB before other
Wi-Fi resources. Dynamic caps are not equivalent to immediate startup costs.
See the matching [IDF 4.4.7 Wi-Fi buffer description](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html).
PSRAM support for eligible Wi-Fi/lwIP allocations does not make DMA pools,
descriptors, interrupt-critical state or ordinary task stacks movable.
[IDF 4.4.7 external RAM restrictions](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/external-ram.html)
also cover cache-disabled access. Do not apply newer-IDF assumptions.

Arduino uses prebuilt SDK libraries. Adding arbitrary sdkconfig defines in
PlatformIO does not rebuild those libraries consistently. If later justified,
use supported `wifi_init_config_t` overrides and validate the exact framework;
do not create a misleading alternate SDK configuration through build flags.

## Temporary instrumentation

Added `include/wifi_startup_trace.h`, `src/wifi_startup_trace_core.h` and
`src/wifi_startup_trace.cpp`. The default is enabled only for **normal Unit**.
Base and the original Stage 0 diagnostic environments compile these calls out.
An explicit `DMX_STAGE5A_TRACE=0` disables the temporary trace; no PlatformIO
configuration was changed to enable it.

Captures are fixed-size task-side snapshots, stored under a short portMUX:

- setup entry and after existing credential preparation;
- immediately before/after M5.begin;
- immediately before/after just the fullscreen canvas, including pointer and
  pointer capability bits (1 internal, 2 DMA-capable, 4 external PSRAM);
- after Sender cell reservation, before/after Receiver console reservation;
- before/after the existing DMX startup sequence;
- preflight guard and worker-task creation;
- before/after allocation phases `esp_netif_init`, `esp_wifi_init`,
  `esp_wifi_set_mode`, `esp_wifi_start`, `httpd_start`;
- raw result/memory after each intermediate netif/event/config/DHCP/readback/
  route registration API, READY, failure guards and teardown.

Every snapshot stores milliseconds, total byte-accessible free heap, free and
largest internal 8-bit heap, **both free and largest internal DMA heap**, minimum
internal 8-bit heap and free PSRAM. These capability pools overlap; do not add
internal and DMA totals together. `total8` includes eligible PSRAM, and is not
the amount available to an internal/DMA-only allocator.

The history holds 48 append-only records; it never overwrites early evidence.
The first failure is retained separately even if capacity was reached, and a
FIRST_FAILURE summary reports any dropped entries. Normal successful startup
uses at most 45 records on the current branch (depending on DHCP status);
subsequent runtime failure/cleanup may exceed capacity, but the first error is
still retained. No periodic heap records or requests are printed.

Output is released automatically once startup succeeds or failure handling
finishes. It requires no Arduino Serial input or `d` command. The application
task drains at most one 64-byte USB packet per 4 ms service interval, using a
fixed 448-byte formatting buffer. A full USB endpoint retains the unsent suffix
for a later turn. There is no waiting for a monitor, printf-to-VFS spin, ISR
printing, dynamic trace allocation or additional task. Network callbacks do not
format/print; they only capture bounded records. Existing DMX servicing still
precedes the owner telemetry/trace call.

The output uses the already-working secondary USB Serial/JTAG console's
FIFO/WR_DONE backend, as used by the hardware-tested Stage 3D instrumentation.
It does **not** call Serial.begin, HWCDC.begin, install a USB driver, change
PHY/pins/interrupt enables, consume USB RX or replace the SDK log sink.
The matching [IDF console routing](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/vfs/vfs_console.c)
and [USB console implementation](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/vfs/vfs_usb_serial_jtag.c)
confirm the backend. Direct bounded FIFO writes avoid the VFS implementation's
possible 50 ms full-endpoint wait. Concurrent existing SDK logs can interleave;
retain the whole capture, not just filtered fragments.

Logging waits for startup to return READY/FAILED; if an SDK call hangs outright,
this does not promise a live dump from that blocked phase. That is a different
failure from the observed returned FAILED (257). A late monitor attachment may
miss an already-drained prefix; open the monitor and reset to collect one boot.

### Observer overhead

Normal Unit static RAM increases by **3,048 bytes** relative to Stage 5
(59,724 -> 62,772). Flash increases by **4,380 bytes**
(1,021,401 -> 1,025,781). This is temporary evidence storage/output code, not
additional Wi-Fi buffers. Ordinary task-stack temporaries and snapshot/formatting
CPU cost also exist. Runtime values have not been measured by the agent.

This overhead can change a marginal guard or allocation outcome. For example,
85,804 minus 3,048 is only 82,756 bytes before other runtime differences. Keep
the original reported values alongside the instrumented ones; do not silently
add the overhead back to make a failing guard pass. All guards remain enforced
against actual instrumented heap.

## Hardware measurement worksheet

| Checkpoint | total8 | int8 / largest8 | dma_free / dma_largest | min8 | psram |
| --- | --- | --- | --- | --- | --- |
| setup.enter / credentials.after | pending | pending | pending | pending | pending |
| M5.begin.before / after | pending | pending | pending | pending | pending |
| fullscreen.before / after + pointer caps | pending | pending | pending | pending | pending |
| sender.cells.after / receiver.canvas.before / after | pending | pending | pending | pending | pending |
| dmx.startup.before / after | pending | pending | pending | pending | pending |
| guard.preflight (record detail mask) | pending | pending | pending | pending | pending |
| esp_wifi_init before / after | pending or not reached | pending | pending | pending | pending |
| esp_wifi_set_mode before / after | pending or not reached | pending | pending | pending | pending |
| esp_wifi_start before / after | pending or not reached | pending | pending | pending | pending |
| httpd_start before / after | pending or not reached | pending | pending | pending | pending |
| FIRST_FAILURE before teardown / cleanup | pending | pending | pending | pending | pending |

For each before/after pair, compare internal and PSRAM deltas, and pointer caps
for the fullscreen allocation. Largest-block changes expose fragmentation that
total free memory alone misses. Global static objects predate setup; compare
compiler/map figures rather than inventing pre-constructor runtime snapshots.

## Minimal correction proposal, conditional on that trace

1. **If guard.preflight fails before any esp_wifi_init record:** no Wi-Fi driver
   initialization failure has occurred. Identify its bit mask and canvas/console
   deltas first. Wi-Fi buffer-count reductions cannot fix this pre-Wi-Fi guard,
   because those pools have not yet been allocated. Keep the guards intact.
2. **If the fullscreen capture confirms the expected internal/DMA placement and
   insufficient headroom:** the leading small candidate is explicitly enabling
   PSRAM for only the existing shared fullscreen sprite, once, before its first
   allocation. Estimated internal recovery is about **153,604 bytes plus heap
   overhead**, with corresponding PSRAM consumption. Largest-block improvement
   is not guaranteed to equal that number. No DMX packet, ISR state, descriptor,
   task stack or driver buffer would move. This has **not** been implemented.
3. Installed M5GFX `LGFX_Sprite.hpp:421-424` passes the sprite's `use_dma()` flag
   into `pushImage`; `SpriteBuffer::use_dma()` recognizes PSRAM and selects the
   non-direct-DMA path. That makes ordinary pixel storage a supported candidate,
   not a reason to force a PSRAM pointer into a DMA-only API. Check the returned
   pointer is actually external because M5GFX can fall back to DMA. Validate
   every fullscreen page, refresh latency, flicker, touch response, NVS activity,
   sleep/wake and external DMX capture under Wi-Fi load before accepting it.
4. **If esp_wifi_init actually returns NO_MEM with adequate prior guards:** use
   its pre/post and failed-before-cleanup values to choose an experiment. Keep
   static TX mode with PSRAM. Reducing static TX 8 -> 6 would nominally save
   about 3,200 bytes. RX 8 -> 6 plus that TX change would nominally save 6,400,
   but AMPDU/RX BA-window compatibility and burst reliability require separate
   review/testing; do not assume a smaller RX pool is harmless. Dynamic limits
   mainly bound traffic-time use and need not cure initialization. No such
   override is made in Stage 5A.
5. **If HTTP/task allocation or a later guard fails:** distinguish the 8192-byte
   worker/HTTP stack allocations, total versus contiguous headroom and actual
   stack watermarks. Do not shrink stacks without measurements, relocate stacks
   blindly, remove the Receiver reservation or suppress the error.

The recommendation is **instrument first, then approve one evidence-backed
allocation experiment**. Do not tune pools and move the canvas simultaneously;
that would obscure which change resolves the failure. A new partition layout,
PSRAM hardware setting, preset migration or Controller/transport redesign is
not justified by current evidence.

## Validation

- New bounded-history host test: PASS (distinct DMA total/largest fields,
  before/after records, first-error retention, full-capacity overflow without
  overwriting original evidence).
- Stage 5A source guards: PASS (unchanged guards and Wi-Fi counts, protected
  M5/canvas/driver boundaries, task-only bounded console path, no credentials or
  USB initialization).
- Existing Controller, Dispatcher, actual ISR/transport fault-injection and HTTP
  policy/asset host executables: PASS; their domain/driver/policy sources remain
  unchanged. Existing integration and all six infrastructure checks: PASS.
- Reproducible driver patch fresh/repeated/mixed/unknown-state checks: PASS.
- Final `git diff --check`, added-file whitespace checks and preservation
  checks: PASS. All 311 preexisting files remain present; only the five intended
  Stage 5A files differ from the captured Stage 5 working state. Credential
  preparation remains byte-for-byte unchanged. ELF symbol inspection confirms
  the trace exists only in normal Unit. HEAD and the existing stash are unchanged;
  nothing is staged.
- Final frozen-input builds: all four PASS. A preceding multi-build run correctly rejected one artifact
  when trace formatting changed during compilation; that run is retained as
  history and is not accepted as final validation.
- No wire-level timing/coherence conclusion is derived from these tests.

Build artifacts/logs: ignored `dist/stage5a-validation-20261025/`.
Normal Unit upload remains a separate user action.

| Environment | Result | Static RAM bytes | Flash bytes | BIN bytes |
| --- | --- | ---: | ---: | ---: |
| cores3se_unit | PASS | 62,772 | 1,025,781 | 1,026,192 |
| cores3se_base | PASS | 59,724 | 1,021,365 | 1,021,776 |
| cores3se_unit_diagnostics | PASS | 62,348 | 1,023,653 | 1,024,064 |
| cores3se_base_diagnostics | PASS | 62,348 | 1,023,617 | 1,024,032 |

The trace compiles out outside normal Unit. These environments add no static
trace RAM; minor code-layout differences are not a claim of byte-identical
binaries. All four receipts record source SHA-256
`2933df377c918573e9d16910fa58a11c1d738dad5038e7b77707577ea034f660`.
Normal Unit application BIN SHA-256:
`a80983a44d9b88b8b27c156930635cf902ed1825e979dbebc0b6864a52f559e5`.
Partition SHA-256 remains
`6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`;
asset bundle SHA-256 remains
`5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9`.

Reproduce the new host checks without contacting a device:

```powershell
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/network/test_startup_trace.cpp -o dist/startup-trace-tests.exe
.\dist\startup-trace-tests.exe
python -B tests/network/check_startup_trace.py
python -B tests/network/check_integration.py
python -B tests/dispatcher/check_integration.py
```

## Exact hardware procedure

Use the currently approved 16 MiB device, Unit profile and known-working power
arrangement (Battery Bottom for a powered Unit DMX). Close other COM8 monitors.
The following commands are provided for the user, **not executed here**:

```powershell
Set-Location -LiteralPath 'C:\path\to\M5Stack-CoreS3SE-DMX-Controller'
pio run -e cores3se_unit -t upload --upload-port COM8
pio device monitor -e cores3se_unit --port COM8 --baud 115200 --rts 0 --dtr 0 --filter time --filter log2file
```

This is the normal Unit environment; do not switch to the Stage 0 diagnostic
environment to collect Stage 5A logs. There is no new FULL/migration/erase step.
The existing normal upload writes the same approved bootloader/table/app layout
and OTA selection initialization; it does not write NVS. No credential reset is
required. No change to upload speed or USB setup is proposed.

The monitor's installed `log2file` filter announces a timestamped
`logs/device-monitor-YYMMDD-HHMMSS.log`. Start a fresh capture at least a second
apart to avoid its same-second filename collision. Keep the monitor open, then
press physical reset once and wait about 15 seconds. The monitor can reconnect
if USB re-enumerates. No `d` input is needed. Capture the entire ROM/M5/SDK and
`[S5A]` output through `FIRST_FAILURE` (or `startup.READY`). If the prefix is
missing, reset with the monitor already attached; do not reinitialize HWCDC.

Record Settings values after the trace drains, but label them post-failure;
send the full log and those values. Repeat one reset with identical power and
hardware connections to check reproducibility. Do not change credentials,
erase NVS, alter Wi-Fi pools or move the canvas between the two captures.
After collection, check that existing touch controls and DMX still function.
External RS-485 capture remains needed for frame/cadence/electrical claims.

## File/Git scope

Stage 5A modifies `include/network_service.h`, `src/network_service.cpp`,
`src/src.cpp`, `src/view_sender.h` and `tests/network/check_integration.py`.
It adds the three trace files above, `tests/network/test_startup_trace.cpp`,
`tests/network/check_startup_trace.py` and this report. Existing Stage 5 work,
Receiver behavior, historical files/logs/backups and the stash are preserved.
No source from the separate Stage 3D worktree was changed or merged; only its
already-tested console output approach was inspected.

No upload, commit, push, merge, tag, release or memory correction was performed.
