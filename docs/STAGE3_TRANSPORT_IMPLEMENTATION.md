# Stage 3C: Safe DMX Transport Candidate

Status: **implemented and software-tested; initial Unit DMX functional hardware
validation confirmed by the user; extended endurance testing still in progress**.
External RS-485 timing/coherence verification remains pending. Baseline: Stage 2 commit
`4c973007d74a1f24319e0e375a56107bc261b784`, branch
`codex/stage3-dmx-transport`. No upgrade to esp_dmx v4.1.0.

Read together with the three transport design/evaluation reports and the USB
investigation. This document describes the actual candidate, including its limits;
it does not certify the physical DMX waveform.

### Local Finalization Recheck, 2026-10-24

Before the explicitly requested local Stage 3 commit, the source, patch templates
and existing firmware matched the fingerprint recorded before Stage 4A. No
application source was changed during finalization. Revalidation passed:

- Controller: all six host-test groups.
- Dispatcher: all six host-test groups and source-integration guards.
- Actual ISR/API/adapter/transport: all seven groups, including 385 deliberate
  underrun tails, incomplete transmission, recovery and ordered blackout tests.
- Reproducible patch: fresh/repeated application and rejection of partial,
  unknown, missing, wrong-version and altered-template states.
- All four installed dependency copies: expected complete patch postimages,
  matching repository templates and unchanged raw-byte S3 getter hashes.
- All four PlatformIO builds: PASS, with the candidate RAM/flash values in
  section 7; `git diff --check`: PASS.

The Stage 4 design, historical development files, device logs and existing stash
are excluded from the Stage 3 commit. The USB investigation is retained as
directly related historical documentation; its diagnostic issue is not fixed.
No upload or new hardware test was performed during this recheck.

## 1. Changed Files

Modified:

```text
platformio.ini
src/command_dispatcher.h
src/command_dispatcher.cpp
src/src.cpp
src/view_sender.h
src/view_receiver.h
tests/dispatcher/test_dispatcher.cpp
tests/dispatcher/check_integration.py
```

Added:

```text
patches/esp_dmx_v202/dmx_tx_contract.h
patches/esp_dmx_v202/tx_core.h
patches/esp_dmx_v202/tx_service.h
patches/esp_dmx_v202/tx_api.inc
patches/esp_dmx_v202/tx_mode.inc
scripts/esp_dmx_tx_patch.py
scripts/patch_esp_dmx_tx_ownership.py
src/dmx_transport.h
src/dmx_transport.cpp
src/dmx_transport_esp_dmx.h
tests/transport/fake/esp_dmx.h
tests/transport/fake/esp_system.h
tests/transport/fake/impl/driver.h
tests/transport/fake/impl/dmx_hal.h
tests/transport/test_transport.cpp
tests/transport/test_patch.py
tests/transport/README.md
docs/STAGE3_TRANSPORT_IMPLEMENTATION.md
```

Existing input reports, historical development directory, logs, releases and
stash `996a2f9378208540b0fd3b544e7d9908affe8e84` are preserved. Test archives,
temporary library revisions, exact Stage 2 builds and fresh candidate builds
are under ignored `dist/stage3c-evaluation-20261024/`; they are not release assets.

Controller implementation, preset adapter/format, diagnostics initialization,
battery code, UI geometry/hold behavior, power logic, hardware header and original
S3 compatibility script are unchanged. No networking, partitions or packaging work.

## 2. Driver Patch and First-Build Integration

`platformio.ini` adds only `pre:scripts/patch_esp_dmx_tx_ownership.py` after the
existing S3 patch. Dependencies remain espressif32 6.7.0, Arduino 2.0.16 / IDF
4.4.7, M5Unified 0.2.21, M5GFX 0.2.28 and esp_dmx v2.02 / metadata 2.0.2.
Both hooks run with the dependency installed and before library compilation.
The completely fresh isolated Unit build confirms this ordering in practice.

The patch transformer validates metadata and complete LF-normalized SHA-256
preimages for `esp_dmx.c`, `impl/driver.h` and `impl/intr_handlers.h`. It also
validates the unchanged FIFO HAL, every repository template hash and complete
postimages. It transforms
only the identified task API, mode/lifetime and ISR ownership paths and adds the
three contract/service headers. A fully applied set is accepted without writes;
partial, unknown, missing or mismatched-template sets fail before writes. An
interrupted multi-file write is detected as mixed on the next build.

The existing three-getter S3 patch remains byte-for-byte unchanged and separate.
No permanent solution relies on hand editing generated dependency files. During
development earlier generated candidates were archived, then reinstalled normally
through PlatformIO; no hand-written library modification is needed by a clone.

`tx_core.h` is the canonical portable state core. `tx_service.h` contains the
actual locked FIFO/IRQ service. Task-side start/poll/cancel and guarded legacy
APIs are in `tx_api.inc`; `tx_mode.inc` supplies safe mode entry. The adapter
requires capability `DMX_TX_CONTRACT_VERSION == 1`. Unsupported non-S3 / IDF-major
changes fail compilation rather than inheriting an unreviewed safety claim.

Buffer 0 is staging; the existing buffer 1 is the active TX snapshot. The initial
FIFO fill and every refill read buffer 1 explicitly, independent of the old RX
index. TX_DONE no longer memcpy writes that snapshot or grants ownership through
the semaphore. Legacy wait now checks qualified state; initial availability is
still distinct from a completed attempt. The old semaphore allocation remains
for minimal structural change, but is not a source of completion truth.

## 3. Ownership and Completion Invariants

1. A packet snapshot is copied only under PREPARING reservation, after UART idle
   and disabled TX/RX packet access are established. The 513-byte copy is outside
   the short critical section. No additional driver packet is allocated.
2. Buffer 1 is immutable until qualified READY retirement. FAULT_STOPPED retains
   its reservation indefinitely. Writes to staging cannot alter active data.
3. All TX task APIs require the installation task and reject ISR context. One
   common port lock covers ISR status capture, RX/TX packet access and lifecycle
   transitions. Late ISR work cannot carry a packet read past a new ownership
   boundary. No other task or future network callback may use the driver directly.
4. Progress satisfies 0 <= queued <= requested <= configured buffer <= 513.
   Zero accepted FIFO bytes or inconsistent progress causes explicit failure,
   rather than an ISR retry spin. The project stages exactly 513 bytes/code zero.
5. While FEEDING, sticky raw TX_DONE is checked before and after each FIFO write.
   A done observed while software data remains, including one latched during the
   final refill, fails conservatively. A counter-only completion test is not used.
6. After the final clean refill, refill IRQ delivery is disabled and done delivery
   armed without clearing a newly arriving done. COMPLETE requires DRAINING,
   all requested bytes queued, a qualified done hint, no fault and FIFO/FSM idle.
7. Abnormal results never turn into COMPLETE on a later event. Timeout/cancel
   disables future refill, keeps the snapshot frozen and waits for physical idle
   under the same guard. Only then may a failed attempt release storage. A stuck
   retirement never allows a new send, mode change or driver deletion.
8. Availability, accepted start, first FIFO write and successful completion are
   different observations. Attempt IDs qualify result matching; old raw flags
   are cleared at quiescence before a new attempt. IDs alone do not tag hardware
   interrupts. Attempt wrap occurs only at that boundary.
9. Controller state and accepted pending events survive transmission failures.
   A completion cannot clear a newer pending value or remove another barrier.
   Eventual delivery requires healthy hardware and continued servicing; permanent
   failure is reported, not falsely acknowledged. No persistence of unsaved
   Controller state across an explicit MCU reset is added.

These are software invariants under the installed S3 UART raw-event/idle register
contract. Host tests model that contract, not analog behavior. External validation
of MMIO/event behavior and the real waveform remains a release gate.

## 4. State Machines and Public Interfaces

```text
driver:
READY -> PREPARING -> FEEDING -> DRAINING -> READY + COMPLETE
                       |          |
                       +-> RETIRING -> READY + abnormal result
                              |
                              +-> FAULT_STOPPED (snapshot retained)

application:
Sender desired -> available -> at most one accepted in-flight attempt
  completion -> next ordered barrier, otherwise latest complete state
  failed safe retirement -> one-frame backoff -> bounded retry
  ambiguous ownership / exhausted retry budget -> latched transport fault
Receiver desired -> cancel/retire active TX -> READ -> Receiver ready
```

Driver extension:

```c
esp_err_t dmx_tx_start(dmx_port_t port, uint16_t slots, uint32_t *attempt);
esp_err_t dmx_tx_poll(dmx_port_t port, dmx_tx_status_t *status);
esp_err_t dmx_tx_cancel(dmx_port_t port);
```

Start accepts an attempt, not a successful transmission. Poll performs bounded
maintenance and returns phase, result, attempt, progress, timing, write-mode and
can-start information. Cancel requests retirement, not immediate reusable storage.
The status includes the first FIFO-write software observation separately from
the attempt's deadline anchor. Neither is an electrical-edge timestamp.

Application `DmxTransport` exposes canStage, stage(packet,size,barrier),
requestSender, poll, serviceTx, receiverReady and health. Health separates
requested/published/accepted/started/completed generations, last error, failure
count, driver phase, cancelled barriers and latched fault. There is no public
unsafe clear-fault or in-place reinitialize method.

The CommandDispatcher publisher now receives the applied command. An optional
admission callback checks transport capacity before Controller mutation. Queued
commands defer in FIFO order when a required barrier cannot be admitted; local
commands return BUSY without mutation and cannot jump the blocked queue. Existing
tests without an admission callback preserve their Stage 2 behavior.

Normal Sender and consumed-wake paths both call view_sender.serviceDmxOutput.
The owner loop polls before Dispatcher/page/wake early returns, but that poll
does not create another frame-start opportunity. Settings/Precision/Keypad/
Presets still service output at the top of the Sender loop. Unchanged packets
continue to transmit. No new task, networking, periodic sender or runtime packet
allocation was added.

Receiver setup requests a mode departure, and its existing queue/parsing/display
path runs only after receiverReady. No TX is started while established READ is
active. Explicit Sender departure cancels queued output obligations, records
cancelled_barriers and retires the active attempt; cancellation is not successful
wire delivery. This is a lifecycle cancellation, not ON/OFF coalescing. Blackout
barrier delivery guarantees apply while Sender remains active and transport can
make progress, not after a deliberate stop/reset/power loss.

## 5. Timeout and Recovery Policy

For this fixed 250000-baud/8N2, 513-slot profile:

```text
one slot                          44 us
513 slots                         22572 us
configured break + MAB            176 + 12 us
nominal full-frame budget         22760 us
active deadline                   4 nominal frames = 91040 us
retirement deadline               2 nominal frames = 45520 us
safe-retry backoff                1 nominal frame  = 22760 us
maximum consecutive failed starts/attempts         3 (initial + two retries)
```

The driver calculates its nominal frame budget from actual requested slots,
baud, break and MAB getters. Four-frame active slack is deliberately longer than
one full universe, rather than mistaking a nominal packet duration for a hard
task-scheduling limit. Completion is checked before expiry classification. Two
retirement frames are conservative against at most a FIFO plus shifter/trailing
framing draining; they are an observation deadline, not permission to overwrite.
The three-failure budget follows the reviewed bounded-recovery design and avoids
infinite retries. These are documented engineering policy values, not measured
hardware worst-case limits; validate them under real display/NVS load.

An underrun, collision, zero-write or timeout latches the first failure. Refill
is stopped; already queued bytes may drain. Reuse requires FIFO/FSM idle and
retired ISR access under the port guard. The next accepted attempt forces the
legacy fresh break/MAB; its attempt ID is new. A retry retains the oldest
unconfirmed blackout barrier, rather than replacing it with a newer OFF state.

If retirement cannot establish idle within its deadline, driver FAULT_STOPPED
keeps buffer 1 owned even if idle is observed much later. The application also
latches a fault on driver ownership/status inconsistency, adapter failure or
three consecutive safely retired failures. The UI loop stays responsive and
latest requested state remains retained, but no automatic sends/reinitialization
occur. No blind FIFO reset, library delete/reinstall or automatic device reboot.

Recovery from a latched fault requires an explicit MCU restart/hardware reset
or power cycle, not clearing a flag on the live transport. The old application/
ISR lifetime ends at reset. On boot the existing UART initialization resets the
peripheral and clears interrupt status before installing the new ISR and enabling
new Sender state. Existing Settings restart/power behavior is not redesigned;
hardware reset/power cycling is the conservative validation path if reset/upload
behavior is uncertain. A future in-place recovery API needs its own quiescence,
ISR synchronization and lifetime proof; none is claimed implemented here.

Error-only best-effort Serial output reports health when TX capacity is available;
it never starts USB, waits for a host or prints every packet. Normal firmware
keeps its previous Serial initialization policy; diagnostic environments already
initialize 115200 and can report the errors. Health is also directly inspectable
through the API. Result values are defined by dmx_tx_result_t in tx_core.h.

## 6. Blackout, Presets and Shutdown

Four fixed application barrier entries retain complete effective packets and
their generations. Four matches the Dispatcher's normal per-turn command budget;
it bounds memory and burst admission, not every possible external producer rate.
Only actual blackout edges consume a barrier. Repeated identical ON/OFF commands
remain idempotent. Capacity is checked before changing Controller state.

Rapid ON/OFF produces a required full zero attempt, then the captured OFF restore
packet, before later ordinary values. Barriers are removed only after matching
COMPLETE. OFF is not reconstructed from a future mutable universe. Ordinary
channel updates and successful preset LOAD publications may coalesce to the latest
whole state; preset storage/metadata and all logical operations remain unchanged.
Edits/loads during blackout are discarded on release exactly by the unchanged
Controller entry snapshot. NVS save/copy/clear remain non-output operations.

The existing shutdown helper now stages zeros through DmxTransport instead of
writing a driver buffer directly. It retains the three iterations, legacy
50-tick waits and 10 ms delays, with no new frame start. Its misleading comment
was corrected: **it still does not guarantee even one complete zero frame before
restart/power-off, let alone three**. The active snapshot is no longer damaged
by those staging writes. Power-off/reset can still interrupt output and queued
obligations. A confirmed-zero shutdown state machine remains separately scoped.

## 7. Builds and Resource Differences

All four final candidate builds PASS. The exact committed Stage 2 source was
archived and independently built in an isolated project for comparison:

| Environment | Stage 2 RAM | Candidate RAM | RAM delta | Stage 2 flash | Candidate flash | Flash delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| cores3se_unit | 32932 | 35764 | +2832 | 604149 | 606713 | +2564 |
| cores3se_base | 32932 | 35764 | +2832 | 604113 | 606681 | +2568 |
| cores3se_unit_diagnostics | 35548 | 38380 | +2832 | 606361 | 608957 | +2596 |
| cores3se_base_diagnostics | 35548 | 38380 | +2832 | 606345 | 608925 | +2580 |

Values are bytes of PlatformIO-reported static RAM / flash use, not raw BIN
lengths or measured free heap. Small string/path differences in isolated
dependency directories affect flash sizes, so these are actual build comparisons,
not a path-independent code-size guarantee. The separately completely fresh Unit
build PASS reports RAM 35764 / flash 606745; its M5GFX directory suffix differs.

Offline target ELF/GDB inspection establishes DmxTransport=2800 bytes,
CommandDispatcher=2604 versus 2596, driver object=152 versus 96, and contract
state=56 bytes. The driver still allocates only its original 1028-byte padded
two-packet block. There is one extra 56-byte driver-object allocation request
delta, not another packet allocation or task stack. Host ABI sizes differ.

Unit .iram0.text grows by 508 bytes; aligned DRAM heap-start placement advances
by 3344 bytes (static data plus IRAM overlap/alignment). Add the driver-object
heap delta when estimating runtime headroom. Do not equate +2832 static RAM
with the total free-heap reduction or invent a measured heap/DMA result. New
hardware heap, largest-block and canvas checks are still needed.

The final Unit ELF links dmx_tx_masks, dmx_tx_fill_locked, dmx_tx_poll_locked,
dmx_intr_handler, FIFO HAL and esp_timer_get_time at 0x4037xxxx IRAM addresses.
Portable transition helpers are forced-inline in the driver; no 513-byte copy,
allocation or logging occurs in TX ISR completion. This is call-chain inspection,
not proof of cache-disabled electrical timing.

Candidate normal Unit application BIN SHA-256:

```text
0b7db02364a0110c56037382a2a703d7d8a845e9f683b548dbd08620e925bd3d
```

Use the actual binary hash, environment and source diff for hardware provenance.
The existing diagnostic v0.2.0-base/32c0571+stage0 banner remains stale and unchanged.

## 8. Automated Results

- Existing Stage 1 Controller suite: six groups PASS.
- Existing Stage 2 Dispatcher suite: six groups PASS; only its publisher callback
  signature changed to receive the command.
- Source-integration guards: PASS, retaining critical UI, holds, keypad, battery,
  canvas, NVS, hardware, diagnostics, USB and S3-patch comparisons. Only the
  explicitly replaced transport/shutdown-staging checks were updated.
- Actual patched ISR/API/adapter/transport suite: seven groups PASS. Includes
  385 distinct deliberately incomplete tail lengths, raw done during the final
  refill, arming-window done, zero FIFO write, shifter-not-idle, timeout, stuck
  retirement, safe retry, bounded retry exhaustion, barrier retention/recovery,
  no final-value loss, presets, Receiver and legacy shutdown limitation.
- Patch tests: PASS for fresh/repeated, preserved S3 getter bytes, whole-set
  prevalidation and rejection of mixed/missing/unknown/version/template inputs.
- All four environments and independent no-.pio Unit build: PASS; no manual
  dependency edits. Logs reside in the ignored evaluation directory.
- git diff --check: PASS. Existing Windows LF/CRLF conversion notices are not
  whitespace errors; no bulk formatting or line-ending normalization was done.

Reproduction commands and test-shim limitations are in tests/transport/README.md.
Tests use the actual ISR with modeled HAL/RTOS, not physical concurrent-core
execution or real peripherals. A successful fake does not establish RS-485 frame
integrity, timing or fixture acceptance.

## 9. Hardware Acceptance Checklist

The initial implementation task performed no upload. The user subsequently
confirmed initial Unit functional hardware validation. The broader checklist
below is not claimed complete by that result. The normal Unit command, for
explicitly authorized use:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

Use the known working Unit + Battery Bottom power arrangement. Close monitors
before upload. Preserve user presets/settings and verified rollback artifacts.

1. Verify at least three actual automatic uploads with app boot between them,
   monitor/reboot operation and original 1500000 upload setting. Do not substitute
   flash_id passes. The diagnostic-build USB regression remains open/out of scope;
   do not change USB initialization or speed to make this candidate pass.
2. Check boot/power, selected-channel/grid scrolling, slider final releases,
   Precision STEP 1/5 acceleration and 10/25 constant holds, keypad, Settings and
   P1-P8 save/load/copy/clear. Verify no reset, UI stall, lost setting or NVS spam.
3. Move sliders rapidly while drawing full-screen pages and saving/loading
   presets. Confirm live/final values and continued output. Repeat under sound,
   battery monitoring and real low-battery warning/MUTE restoration conditions.
4. Exercise rapid blackout ON/OFF and several transitions while a frame is
   active. Verify zero output then exact entry restore, including edits/loads
   during blackout. A normal fixture test cannot prove a full zero packet was
   sent; capture it externally to establish the barrier's wire effect.
5. Run extended continuous output (for example 30 minutes) through page changes,
   backlight sleep and consumed wake gestures. Check no sustained heap loss,
   missing final value or unexpected transport error. Recheck one shared canvas,
   free heap/PSRAM, minimum heap and largest internal/DMA blocks with diagnostics
   only when its existing USB access is safely handled.
6. Test Receiver separately: no transmission after READ is established, no stale
   Sender service, no mid-frame direction flip. Mode departure cancels pending
   output obligations explicitly; do not count them as completed frames.
7. Test restart/shutdown deliberately with fixtures safe. Record final observed
   output: the legacy helper does NOT promise three zeros. Do not infer a safety
   blackout from its three staging/wait iterations.
8. With suitable differential/RS-485 capture equipment, compare exact Stage 2 and
   candidate: 250000 baud, 8N2, break/MAB, zero start code, 513 total slots, gaps,
   complete A/B/zero/restore patterns and command-to-wire latency. Set acceptable
   worst-case gap/latency budgets from baseline measurements; no hard limit was
   inferred from software logs. Waiting for true idle can change inter-frame gaps.
9. Only in a separately authorized non-release fault-injection build, deliberately
   starve refill including the last slot. Capture partial attempts, explicit
   failure, quiescence, fresh-framed retry, retained blackout edges and fail-stop.
   Verify no partial attempt is recorded COMPLETE. No injection switch is enabled
   or new device fault experiment automatically run by this implementation.
10. Run experimental Base as a separate matrix after Unit acceptance. Existing
    Base glitches are not evidence of this completion issue and are not declared
    fixed. Keep capture, power/termination and fixture observations separate.

## 10. Remaining Limits and Rollback

Electrical/MMIO behavior and real scheduling margins remain unmeasured. The
legacy approximately 188 us framing busy-wait remains an explicit exception,
and can be stretched by preemption. No new hardware timer or hard real-time
cadence guarantee. Retirement can increase gaps. Recovery thresholds and cache-
disabled behavior need real workload acceptance before calling this stable.

There is no unbounded event queue, guarantee under permanently failed hardware,
in-place fault reset, guaranteed zero-before-power-off, new filesystem, Wi-Fi or
Receiver redesign. Legacy sniffer/parser semantics are not newly certified. The
contract is scoped to this fixed S3 project with one owner and unchanged UART
configuration, not arbitrary third-party callers reconfiguring UART during TX.

Rollback source is the preserved exact Stage 2 commit and isolated build, not
a reset of this working tree. Do not drop the stash or rewrite history. Device
rollback requires explicit approval and the previously verified app-only flash
layout; do not erase NVS/OTA data or silently publish/flash a FULL image. No
merge, push, tag or release is part of local finalization. The user authorized
the focused local Stage 3 commit after the recheck above; extended endurance,
external capture and the separate diagnostic USB investigation remain open.
