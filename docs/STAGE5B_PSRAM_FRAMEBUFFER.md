# Stage 5B: Recover internal RAM with the existing PSRAM framebuffer

Finalization note (2026-10-25): the user physically confirmed PSRAM placement,
approximately 153.6 KB internal recovery, READY, working phone HTTP and normal
DMX/display operation. Stage 5C is also accepted with three simultaneous clients
and working iPhone cellular routing. Four clients are configured, not yet
physically validated. The report below preserves the pre-test implementation
record; final production startup tracing defaults OFF. See the final Stage 5
implementation summary for current validation/limitations.

## Confirmed cause

This report supersedes the unresolved failure-location hypothesis in
`STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md`. The actual device logs are present locally:

- `logs/device-monitor-261025-113458.log`
- `logs/device-monitor-261025-113510.log`
- `logs/device-monitor-261025-113522.log`

All three report successful Wi-Fi initialization, AP configuration/start,
DHCP, HTTP creation and route registration. The first failure is the
**application's `guard.after_http`**, not `esp_wifi_init` or `httpd_start`.
The guard correctly stops networking after internal RAM headroom is exhausted.
These are user-performed hardware captures of Stage 5A, not new Stage 5B tests.
The logs do not independently attest a flashed binary SHA-256.

Representative measured values from `113510.log`, all in bytes:

| Checkpoint | Internal free | Largest internal 8-bit | DMA free | Largest DMA | Internal minimum | Free PSRAM |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| M5.begin before | 307,652 | 270,324 | 299,912 | 270,324 | 302,432 | 8,386,295 |
| M5.begin after / fullscreen before | 302,776 | 262,132 | 295,036 | 262,132 | 297,500 | 8,386,051 |
| Fullscreen after | 149,156 | 114,676 | 141,416 | 114,676 | 143,852 | 8,386,051 |
| Sender cells after | 138,316 | 104,436 | 130,576 | 104,436 | 133,012 | 8,386,051 |
| Receiver canvas after | 110,136 | 75,764 | 102,396 | 75,764 | 104,832 | 8,386,051 |
| DMX startup after | 108,648 | 75,764 | 100,908 | 75,764 | 103,520 | 8,386,051 |
| esp_wifi_init before | 90,588 | 75,764 | 82,848 | 75,764 | 90,588 | 8,386,051 |
| esp_wifi_init after | 46,172 | 37,876 | 38,432 | 37,876 | 46,172 | 8,352,907 |
| esp_wifi_start after | 35,356 | 27,124 | 27,616 | 27,124 | 35,272 | 8,350,875 |
| httpd_start before | 35,260 | 27,124 | 27,520 | 27,124 | 35,260 | 8,350,875 |
| httpd_start after | 22,376 | 14,580 | 14,636 | 14,580 | 22,376 | 8,350,875 |
| URI registration / guard.after_http | 22,316 | 14,324 | 14,576 | 14,324 | 22,316 | 8,350,875 |

The fullscreen pointer is `0x3fca78b8`, capability mask 3 (internal + DMA),
while PSRAM does not change across that allocation. Internal usage increases
**153,620 bytes**. M5GFX's RGB565 request is 153,602 bytes, rounded to 153,604,
with the remaining difference consistent with allocator overhead. This is the
largest movable ordinary pixel allocation in the trace.

Other measured costs are 10,840 bytes for the two Sender cell sprites, 28,180
for the Receiver console, 1,488 across DMX startup, 44,416 internal bytes across
`esp_wifi_init`, and 12,884 across `httpd_start` (plus 60 for route registration).
These deltas include concurrent runtime effects; they are not all individual
malloc sizes. Capability pools overlap and must not be summed as independent RAM.

### Guard interpretation

The unchanged predicate in `src/network_service.cpp` requires:

```cpp
m.internal >= 32768 && m.dma >= 16384
```

Here internal is free `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`; dma is the
**largest free** `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` block. It is not total
DMA free and not free PSRAM. The failing case is 10,452 bytes short of the
internal threshold and 2,060 bytes short of the largest-DMA threshold.

The separate startup preflight remains 80 KiB internal free, 48 KiB largest
internal 8-bit and 32 KiB largest DMA. Stack-watermark and transport-error guards
also remain unchanged. These are conservative admission/stop reserves from
Stage 5, not a proven worst-case allocation model. The hardware trace justifies
rejecting this marginal state; it does not prove the thresholds suffice for all
future loads. Runtime stress acceptance is still required.

Removing Stage 5A's 3,048 bytes of static overhead would nominally leave only
25,364 internal bytes at this checkpoint, still below 32 KiB. Disabling that
instrumentation alone is not a correction. It remains enabled for normal Unit
so the same checkpoints can be compared.

## Chosen correction and failure behavior

Only the existing shared fullscreen pixel buffer moves to PSRAM. It remains
one sprite at the existing display dimensions (320 x 240 on CoreS3SE), with
16-bit RGB565, the same draw functions and the same page-owner checks.

New `include/fullscreen_canvas_memory.h` provides a small tested allocation
policy used at the original `getFullscreenCanvas()` allocation site:

1. Allow one allocation attempt for the view lifetime.
2. Set the same 16-bit depth, then explicitly `setPsram(true)` before creation.
3. Let M5GFX allocate and own the sprite normally.
4. Validate the returned pointer with the installed SDK's
   `esp_ptr_external_ram()`.
5. If M5GFX fell back to internal memory, immediately delete that sprite and
   return null. Never retain it and then start Wi-Fi as though RAM was freed.

The view caches the valid pointer and its ready state. Repaints and page changes
reuse it; there is no second framebuffer, whole-frame internal copy or new
per-frame application allocation. The failure path does not repeatedly attempt
large allocations on redraw. If reservation fails, existing `guard.canvas`
blocks network startup. Main-screen controls remain available, but fullscreen
pages can be unavailable until reboot; this is an explicit low-memory failure
mode, not a claimed successful UI. No preset or NVS repair is attempted.

M5GFX's own failed-PSRAM branch can momentarily allocate an internal fallback
during initial creation. The policy detects and frees it before networking;
it is never used as a successful fullscreen buffer. A null allocation remains
null. The existing unreserved-buffer cleanup now clears ready/pointer metadata
after deletion, avoiding a stale freed pointer in that failure path. The attempt
flag is not reset to create a redraw allocation loop.

Normal successful reservation still retains the single framebuffer across
Sender/Receiver mode changes. Active-page ownership, fullscreen push, rendering,
layout, touch consumption and refresh triggers are unchanged.

## Installed M5GFX transfer-path assessment

The exact installed M5GFX 0.2.28 source was inspected; no vendor files changed.
Paths below are under `.pio/libdeps/cores3se_unit/M5GFX/src/`:

- `M5GFX.h:277-281`: the default M5Canvas has no parent and inherits the default
  allocation choice; the parent-taking constructor already opts into PSRAM.
- `lgfx/v1/LGFX_Sprite.hpp:148-174`: native `setPsram` and `createSprite`.
- `lgfx/v1/LGFX_Sprite.cpp:58-79`: normal source selection and existing-size reuse.
- `lgfx/v1/misc/SpriteBuffer.cpp:116-156`: explicit PSRAM allocation, possible
  internal fallback, and source-capability-aware `use_dma()`.
- `lgfx/v1/LGFX_Sprite.hpp:421-424`: `pushSprite` forwards `use_dma()` into
  destination `pushImage`; an external PSRAM buffer is not a direct DMA source.
- `lgfx/v1/misc/pixelcopy.cpp:27-44`: matching source/destination depths select
  the no-conversion path.
- `lgfx/v1/panel/Panel_LCD.cpp:287-326`: opaque, matching-format pixels flow
  directly from the source to the SPI bus, respecting the DMA flag.
- `lgfx/v1/platforms/esp32/Bus_SPI.cpp:903-945,1027-1073`: non-DMA SPI writes
  use bounded register staging; the S3 path has a 64-byte stack buffer. Smaller
  writes can use existing reusable flip buffers. Conversion paths elsewhere
  can likewise use library-managed small DMA staging, not a second full frame.

The panel's default write depth is RGB565 (`lgfx/v1/Panel.hpp:62`); this
application does not change it. The unchanged fullscreen push has 320 x 240
opaque matching-format pixels. Its 153,600-byte transfer exceeds the bus's
small-write staging branch, so the full-frame path uses the bounded register
buffer rather than allocating an internal frame on each refresh. Existing
smaller/conversion drawing paths retain their existing library behavior.

Thus this library supports a normal pixel framebuffer in PSRAM without treating
it as a DMA-only descriptor/buffer. No DMX snapshot, UART ISR state, driver
buffer, task stack or DMA descriptor is moved. The unchanged push path handles
source capabilities; no direct-DMA call is forced on a PSRAM pointer.

PSRAM still has cache and bandwidth limits. CPU-staged SPI transfer can consume
more owner-task time than an internal direct-DMA source, and cache-disabled
access remains restricted. See [the matching IDF 4.4.7 external-RAM guidance](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/external-ram.html).
The application owner serializes drawing and preset NVS calls; network handlers
do not draw or write presets. No new ISR accesses the framebuffer.

There is no measured rendering-speed or maximum-loop-gap result for Stage 5B
yet. The SPI fallback includes waits for peripheral progress; this change does
not turn screen drawing into a nonblocking operation. Its duration and effect
on owner servicing must be measured. Interrupt-driven DMX retains the tested
implementation, but that alone does not establish unchanged cadence on the wire.
If hardware reveals UI stalls, transport failures or unacceptable frame gaps,
stop and retain the logs rather than changing driver timing or weakening guards.

### Why the Receiver console and HTTP stack stay unchanged

The 28,180-byte Receiver allocation is significant, but much smaller than the
fullscreen buffer. Retaining its already-tested internal placement avoids
changing another rendering/transfer path and preserves allocation before radio
startup. The 8192-byte HTTP stack is part of a measured 12,884-byte service-start
cost. It is not shrunk or moved: actual stack-watermark evidence under parsing,
error and slow-client paths is required before a stack-size experiment.
Wi-Fi pool counts, SDK configuration, dependencies, credentials, NVS format,
HTTP limits and resource guards remain byte-for-byte unchanged in Stage 5B.

## Expected memory improvement versus measurements

Expected recovery is approximately **153,604 internal bytes plus heap overhead**,
with corresponding PSRAM consumption. The measured old delta suggests about
153,620 bytes on this allocator. A simple same-cost estimate gives roughly
175,936 internal bytes after HTTP, versus 22,316 before; free PSRAM might become
about 8,197,255 versus 8,350,875. These are **estimates, not new device readings**.
Task ordering, fragmentation, allocator routing and transient SDK allocations
can change the result. Largest-block and minimum-heap improvements cannot be
inferred by adding the buffer size to the old values.

The existing Stage 5A checkpoints are unchanged. A successful new capture must
show `fullscreen.after caps=4`, a nonnull external address, much less internal
heap loss across that pair, and approximately a framebuffer-sized PSRAM loss.
The after-Wi-Fi/HTTP checkpoints and startup.READY must then be collected on the
device. `/status` continues to sample internal/minimum/largest/DMA/PSRAM,
station/request counts, task watermarks, owner-loop gaps and transport health.
Its largest-DMA and Settings DMA fields remain largest-block measurements.

| Required Stage 5B measurement | Result |
| --- | --- |
| Internal free / largest before and after fullscreen allocation | pending hardware |
| External pointer/capabilities and PSRAM delta | pending hardware |
| Internal / largest DMA after Wi-Fi and HTTP initialization | pending hardware |
| Lifetime minimum internal heap and task stack watermarks | pending hardware |
| Fragmentation across page/mode cycles and HTTP traffic | pending hardware |
| Maximum owner servicing gap and transport errors under load | pending hardware |
| External DMX coherence / cadence / electrical capture | pending hardware |

## Tests and scope

New host tests exercise the production allocation policy with a controlled
canvas allocator: RGB565/PSRAM ordering, 320 x 240 geometry, external-pointer
acceptance, internal-fallback deletion, total allocation failure, and repeated
entry without repeated allocation/deletion or retained-pixel mutation.
The same executable runs the actual unchanged `safe()` predicate/constants
extracted into an ignored host-test header, including the hardware failing
case and both reserve boundary values. This is not a reimplementation of Wi-Fi.

Display integration checks inspect the exact installed M5GFX source and assert
one persistent sprite, the native capability-aware transfer path, retained
page-owner checks and unchanged resource capabilities. Existing whole-view
baseline comparisons remove only the explicitly reviewed allocation change,
failure metadata reset and previous Stage 5 additions before comparing all
remaining UI/application code. They do not simulate physical rendering or
measure SPI/DMX timing.

Files changed relative to the preserved Stage 5A working tree:

- `src/view_sender.h`: allocation call, one-attempt flag, SDK pointer predicate,
  and clearing pointer/ready metadata when an unreserved sprite is deleted.
- `tests/dispatcher/check_integration.py`, `tests/network/check_integration.py`:
  allow only the reviewed memory change through shared exact normalization.
- `tests/network/check_startup_trace.py`: replace the old Stage 5A no-relocation
  assertion with the approved Stage 5B allocation integration check.

Files added:

- `include/fullscreen_canvas_memory.h`
- `tests/network/canvas_guard_support.py`
- `tests/network/test_canvas_memory.cpp`
- `tests/network/check_canvas_memory.py`
- `docs/STAGE5B_PSRAM_FRAMEBUFFER.md`

Normal Unit keeps Stage 5A's bounded USB tracing. No Serial/HWCDC initialization
or diagnostic behavior is changed. `src/network_service.cpp`, Controller,
Dispatcher, transport, Stage 3E patch, presets/NVS, Receiver, platform/partition
configuration and embedded assets are not modified in this stage.

Host checks passed: allocation/guard tests; Controller; Dispatcher; actual
transport/ISR adverse-state tests (including the 385 underrun cases and Stage 3E
clash regressions); reproducible patch states; HTTP policy/assets; startup trace;
integration checks; and all six infrastructure checks. All four frozen-input
builds pass:

| Environment | Result | Static RAM bytes | Flash bytes | Application BIN bytes |
| --- | --- | ---: | ---: | ---: |
| cores3se_unit | PASS | 62,772 | 1,025,869 | 1,026,288 |
| cores3se_base | PASS | 59,724 | 1,021,481 | 1,021,904 |
| cores3se_unit_diagnostics | PASS | 62,348 | 1,023,757 | 1,024,176 |
| cores3se_base_diagnostics | PASS | 62,348 | 1,023,725 | 1,024,144 |

Static RAM is unchanged from Stage 5A (the attempt flag fits existing layout
padding). Flash changes are +88/+116/+104/+108 bytes respectively. Static RAM
figures do not show the intended runtime heap/PSRAM relocation and do not prove
that SoftAP succeeds. The normal Unit still includes Stage 5A's 3,048-byte
instrumentation overhead; the other environments compile that trace out.

All four build receipts identify source SHA-256
`751c744cfad7d70663a0fea4b46f36ace91a94e8246c5d0a73d5cf6e74a0b192`.
Normal Unit application BIN SHA-256:
`d1c1b15cee8272367380c5c18bdb94594bd451a4cfab2e691b149a254470f2d4`.
The 16 MiB image header, approved partitions and embedded asset bytes are
validated by the unchanged Stage 4B artifact hook. No release FULL or migration
bundle was regenerated. Build logs/resources/host outputs are preserved under
ignored `dist/stage5b-validation-20261025/`.

Reproduce the new host test with the installed isolated compiler:

```powershell
python -B tests/network/check_canvas_memory.py --guard-header dist/stage5b-host/guard_under_test.h
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Iinclude -Idist/stage5b-host tests/network/test_canvas_memory.cpp -o dist/stage5b-host/canvas-memory-tests.exe
.\dist\stage5b-host\canvas-memory-tests.exe
python -B tests/network/check_integration.py
python -B tests/dispatcher/check_integration.py
python -B tests/network/check_startup_trace.py
```

## Hardware upload and acceptance

No automatic upload was performed. From this repository, after closing other
COM8 monitors:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
pio device monitor -e cores3se_unit --port COM8 --baud 115200 --rts 0 --dtr 0 --filter time --filter log2file
```

Use the normal Unit firmware on the already-verified 16 MiB layout. Keep the
known-working Unit + Battery Bottom power arrangement. No erase, FULL migration,
credential reset or NVS rewrite procedure is needed. With the monitor open,
press physical reset and retain all output through `startup.READY` or the
FIRST_FAILURE summary; wait about 15 seconds. The monitor announces its new
timestamped file under `logs/`. Do not reuse/overwrite the Stage 5A captures.

Acceptance procedure:

1. Compare every Stage 5A startup checkpoint with the table above. Require an
   external framebuffer (`caps=4`), preserved RGB565 geometry and READY. Any
   fallback or allocation failure must block Wi-Fi rather than retain an internal
   full frame. Keep guards unchanged if startup still fails.
2. At minimum, pass the existing free-internal >=32 KiB / largest-DMA >=16 KiB
   guard throughout sampling. For this validation, target >=64 KiB internal and
   >=32 KiB largest DMA after startup to retain margin above those floors; these
   are test targets, not silently changed firmware thresholds or proof of all
   future peak loads. Record the actual lifetime minimum and transient troughs.
3. Read the same persistent SSID/key in Settings. Connect one browser to the
   configured/read-back address and verify index, gzip CSS/JS, cache behavior
   and `/status`. Run the existing read-only probe when other refreshing clients
   are idle:

   ```powershell
   python -B tests/network/probe_device.py --host 192.168.4.1 --output dist/NEW-stage5b-http-probe.jsonl
   ```

   Confirm the address in Settings first; use a new output file.
4. Exercise Settings, Precision, Keypad and Presets repeatedly (at least 100
   complete page cycles), checking one unchanged canvas pointer, no flicker,
   corruption or residual pixels. Test rapid slider dragging, keypad entry,
   +/- hold and step behavior, channel selection, scrolling and touch consumption.
5. Repeat with sustained refreshes for at least five minutes, first one browser
   and then two. Capture `/status` before/after each condition: free/minimum
   internal, largest internal/DMA, PSRAM, both task stack watermarks, owner gaps,
   transport errors and stopped state. Require no persistent decline once caches
   and SDK staging have warmed up. A lifetime minimum cannot recover; compare
   current free/large-block plateaus separately from that low-water mark.
6. Under that traffic, test blackout ON/OFF ordering, preset save/load/copy/clear,
   battery/speaker behavior, sleep/wake and Sender/Receiver/Sender. Require zero
   transport errors/fail-stop, responsive controls and no unexpected reset. Look
   for allocation/fragmentation growth after mode/page returns, not just idle.
7. Disconnect/reconnect clients, repeat at least three cold boots and verify
   identical credentials and successful startup. Recheck no-XLR boot and cable
   disconnect/reconnect while serving HTTP. Preserve the Stage 3E regression gate.
8. Compare UI refresh and maximum owner gaps with the existing stable baseline.
   Use external RS-485 capture/logic analysis to verify complete 513-slot packets,
   frame ordering, break/MAB and cadence/gaps under the worst UI/HTTP load.
   Visual fixture response and software counters alone do not prove wire integrity.

Hardware memory, rendering, browser and DMX success remain **unverified for
Stage 5B**. In particular, native PSRAM support does not establish equal display
latency. Base remains experimental; successful compilation does not settle its
separate glitch investigation. Existing Stage 4B limitations concerning upper
flash writes, OTA rollback and shutdown-zero frames remain unchanged.

## Git and rollback

Work stays on `codex/stage5-softap-http`, with HEAD at the preserved Stage 4B
commit `f31411a38d56934c5d698d52371cf45235491bc4`. Stage 5/5A changes, historical
files, logs, backups, other worktrees and the existing stash are retained.
All Stage 5B changes remain uncommitted pending hardware validation.
Final preservation checks confirm all 320 preexisting files remain present,
with only the four intended existing files changed. Wi-Fi/HTTP source is
byte-for-byte unchanged, all four artifact receipts match the current source,
the staging area is empty and the stash is unchanged. `git diff --check` and
the added-file whitespace/EOF checks pass.

If validation fails, keep its logs and this working tree. The pre-change source
snapshot is under ignored `dist/stage5b-validation-20261025/before/`; use a
separate reviewable environment or the tested Stage 4B commit for a rollback
build rather than resetting, stashing or discarding this work. Do not disable
guards to force the Stage 5A firmware to run in the known insufficient state.
No upload, commit, push, merge, tag, release or packaging change was performed.
