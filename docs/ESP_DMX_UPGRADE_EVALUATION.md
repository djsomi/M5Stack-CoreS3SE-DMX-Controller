# Stage 3B: esp_dmx v4.1.0 Upgrade Evaluation

Evaluation date: 2026-10-24. **Review only; Stage 3 is not implemented.**

## 1. Decision

**NO-GO: do not upgrade production to unpatched v4.1.0 as a solution to the
Stage 3 safety gate.** It compiles with this project's existing toolchain, but
does not establish immutable active-packet ownership or qualified full-packet
completion. A version upgrade alone is not the missing correctness boundary.

**Recommended next implementation candidate:** retain v2.02 (metadata 2.0.2),
its existing three-expression S3 patch, and the focused Stage 3A snapshot/result
contract. Retain the explicitly permitted approximately 188 us first/long-gap
framing busy-wait for this milestone. Do not add a hardware-timer redesign.
This is a recommendation for a future tested candidate, not approval or proof
that the unimplemented patch is correct.

| Alternative | Decision for the next milestone |
| --- | --- |
| v2.02 + focused ownership/completion patch | Preferred conditional GO for an isolated implementation/test phase after approval; smallest surrounding API/timing change. |
| Official v4.1.0 without a driver patch | Compile compatibility GO; transport-safety and production-migration NO-GO. |
| v4.1.0 + focused contract patch | Technically plausible, but defer. Still needs completion fixes plus API, timer, Receiver and lifecycle validation. Not a patch-free escape from Stage 3A. |

The Unit's demonstrated stability and the Base's experimental glitches remain
separate observations. No underrun was measured here, and no Base root cause or
fix is inferred from these source-level counterexamples.

## 2. Inputs, Exact Versions and Isolation

Read the complete existing `STAGE3_TRANSPORT_GATE.md`,
`STAGE3_BUFFER_OWNERSHIP_DESIGN.md` and `USB_UPLOAD_REGRESSION.md` reports.
Production remains on `codex/stage3-dmx-transport` at Stage 2 commit
`4c973007d74a1f24319e0e375a56107bc261b784`.

Local reference roots used below:

```text
V2 = .pio/libdeps/cores3se_unit/esp_dmx/src/
E  = dist/esp-dmx-v410-evaluation-20261024/
V4 = E/upstream/src/
P  = E/project/
SDK = C:/Users/somog/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/
```

`E` is an ignored, separate temporary evaluation project, not a replacement
production checkout. It has its own PlatformIO configuration, source copies,
`.pio/libdeps` and build outputs. Only the installed global framework/toolchain
packages are reused. No production dependency installation or patch hook was run.

Official tag v4.1.0 was cloned from the requested upstream repository, resolving
to commit `6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5`. Compilation separately used
the official GitHub v4.1.0 ZIP URL. All 56 files under the clone's `src/` match
the installed probe ZIP sources after CRLF-to-LF normalization. The upstream
checkout is unmodified. Do not confuse Windows checkout newline hashes with a
different upstream implementation.

| Component | Evaluated version |
| --- | --- |
| PlatformIO platform | espressif32 6.7.0 |
| Board / framework | esp32-s3-devkitc-1 / Arduino |
| Arduino package | 3.20016.0, Arduino 2.0.16 |
| Bundled ESP-IDF | 4.4.7; the v4 driver takes its IDF 4.x branches |
| Xtensa S3 compiler | 8.4.0+2021r2-patch5 |
| esptool | 4.5.1 |
| M5Unified / M5GFX | 0.2.21 / 0.2.28 |
| Existing esp_dmx | 2.0.2 from tag v2.02, existing S3 getter patch |
| Evaluated esp_dmx | Official 4.1.0, no custom patch |

The upstream [v4.1.0 README](https://github.com/someweisguy/esp_dmx/blob/6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5/README.md)
specifies Arduino >=2.0.3 / IDF >=4.4.1. Our installed versions satisfy those
minimums; the compile results below verify this particular configuration rather
than assuming that a newer framework is necessary.

Normalized LF SHA-256 identities of the principal evaluated sources:

| V4 file | SHA-256 |
| --- | --- |
| esp_dmx.h | `9aa495d09e3b7a222e78972efc46f947bdd4d34070563f5d81869c2c9d7aa52d` |
| dmx/io.c | `8203a870020771d06cc9b2de082722976e2deeb1c909bf07a2da6476def9ead4` |
| dmx/hal/uart.c | `5bf891faa7d25e606ef0319bdd6b4eba80abeab1207432b1f194c334b7f13c62` |
| dmx/hal/timer.c | `914244052c8ba128d83a49647bdc330a46a389ef2cd64f64eff74ea92aa2de5b` |
| dmx/driver.c | `3c5fdd09ce1430c6d59c534e8119c92bd7dce8468206c009bfd0935a2eecdce3` |
| dmx/include/service.h | `18f0eaf6c64aea7502b9c9d1629e724612e609c9b9febaebd44c562bbd6807a0` |

The exact V2 hashes and installed SDK references remain in the Stage 3A report.
Its compatibility patch changes only these getter expressions in `impl/dmx_ll.h`:

```text
hw->uart_idle_conf_reg_t.tx_idle_num   -> hw->idle_conf.tx_idle_num
hw->uart_txbrk_conf_reg_t.tx_brk_num   -> hw->txbrk_conf.tx_brk_num
hw->uart_status_reg_t.rxd             -> hw->status.rxd
```

It does not repair ownership or completion. V4 uses the SDK LL interface in the
new implementation and compiled without these substitutions. Do not apply the
v2-only patch hook to v4 files.

## 3. Exact Source Comparison

Line numbers refer to the identified source versions, not current upstream main.

| Concern | Installed V2 | Official V4 |
| --- | --- | --- |
| Packet storage | `esp_dmx.c:144-175`, `impl/driver.h:25-53`: two packet buffers, shared RX/TX index and progress. | `dmx/include/service.h:109-180`: one `dmx.data[513]`, head, size, status and progress. The two sniffer metadata records are not two TX packet buffers. |
| Application writes | `esp_dmx.c:701-740`: memcpy/slot write into buffer 0 without active-TX protection. | `dmx/io.c:55-97`: ordinary DMX writes still copy into the same active data buffer. Only detected RDM transmission rejects asynchronous writes. |
| Initial FIFO source | `esp_dmx.c:800-812`: buffer indexed by buf_idx. | `dmx/hal/timer.c:20-49`: initial data reads dmx.data after timed break/MAB. RDM discovery's direct prefill is separately in `dmx/io.c:430-441`. |
| Later FIFO reads | `impl/intr_handlers.h:48-65`: always buffer 0. | `dmx/hal/uart.c:264-275`: dmx.data + head, advance head, disable refill only at head == size. |
| Done branch | `impl/intr_handlers.h:66-74`: copies 0 to 1 and gives semaphore; leaves refill enabled if slots remain. | `dmx/hal/uart.c:276-300`: disables both TX interrupt sources, clears done, records EOP, sets COMPLETE/IDLE and notifies. No full-progress/physical-idle/fault qualification. |
| Completion observation | `esp_dmx.c:817-829`: binary token only. | `dmx/io.c:470-514`: status check for wait(0); task notification for positive wait. No validated transmitted-slot count, attempt identity or abnormal TX result. |
| Starting output | `esp_dmx.c:743-815`: zero-wait token admission, existing bounded framing delay. | `dmx/io.c:299-465`: recursive mutex, internal wait_sent(23 ms), possible timer/notification wait, then asynchronous break and data. |
| Direction | `esp_dmx.c:258-309`: explicit READ/WRITE mode, independent IRQ masks; RX index mismatch affects later TX source selection. | `dmx/io.c:145-155,389-395`, `dmx/hal/uart.c:300-322`: RTS changes through write/receive/send/RDM paths; no equivalent public set_mode API. One buffer removes the old index mismatch, not mode-lifetime requirements. |
| Framing | `esp_dmx.c:771-798`: first/long-gap break/MAB busy-wait; UART automatic trailing framing otherwise. | `dmx/hal/uart.c:360-365` disables automatic UART break/idle; `dmx/io.c:444-453` and `dmx/hal/timer.c:20-49` use a timer for every ordinary packet. |
| UART service | Existing TX threshold 8; RX threshold 120 and RX timeout configuration. | `dmx/hal/uart.c:23-24,360-377`: TX threshold 8, RX threshold 1, 250000 baud/8N2, UART mode rather than the old half-duplex mode. |
| Timeout/cancel | Wait timeout does not abort TX or transfer ownership. | Wait timeout still does not abort TX, drain the FIFO, retire timer/ISR references or certify reusable storage. |
| Lifecycle | `esp_dmx.c:215-250`: frees ISR and storage without an explicit graceful completion contract. | `dmx/driver.c:249-305,308-348`: delete/deinit and enable/disable are not an attempt-aware cancellation/recovery contract. |

Primary immutable upstream references for the critical comparison:
[write/send/wait implementation](https://github.com/someweisguy/esp_dmx/blob/6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5/src/dmx/io.c#L55),
[UART ISR and initialization](https://github.com/someweisguy/esp_dmx/blob/6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5/src/dmx/hal/uart.c#L264),
[timer ISR](https://github.com/someweisguy/esp_dmx/blob/6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5/src/dmx/hal/timer.c#L20),
[driver storage](https://github.com/someweisguy/esp_dmx/blob/6281323e3faae2fc6be5dcfe7ea4d11b4b57eac5/src/dmx/include/service.h#L109).

### Stronger retirement, still not truthful completion

V4's TX_DONE interrupt masking is a real improvement over V2. Do **not** carry
over the exact V2 claim that this branch leaves future UART refills enabled.
For an ordinary DMX packet, the V4 branch disables TX_DATA and TX_DONE before
publishing IDLE. It also removes the V2 completion-time packet copy.

However, it does not check head == size, FIFO emptiness, TX FSM idle, a qualified
post-final-refill done event, or a latched underrun. No normal-versus-partial TX
result is recorded. Disabling future reads does not retrospectively transmit
the unqueued tail. Quiescent buffer ownership and successful wire completion
are different contracts.

### Writes during TX still mix generations

V4 `dmx_write_offset` reads status under a spinlock, rejects an active RDM write,
then memcpy writes the single packet buffer under the lock. Plain start-code-zero
DMX is deliberately allowed through. The UART refill and initial timer fill do
not hold that same lock around their complete buffer/FIFO operations.

Even a perfectly atomic memcpy would not solve the temporal problem: bytes
already in the UART FIFO can be from generation A, and a later refill can read
generation B. No second application thread is needed. Merely renaming
dmx_write_packet to dmx_write preserves the existing unsafe publication pattern.

### FIFO underrun and interrupt ordering

The S3 FIFO is 128 bytes; the application packet is 513 bytes. The ISR handles
RX first, then TX_DATA, then TX_DONE. TX_DATA writes before evaluating a latched
TX_DONE. Source-level conditional execution:

```text
initial FIFO: A[0..127], head=128, requested=513
refill service delayed; FIFO drains; TX_DATA and TX_DONE become pending
TX_DATA branch: refill A[128..255], head=256; clear TX_DATA
next iteration: previously latched TX_DONE handled
disable both TX IRQs; set COMPLETE/IDLE; notify waiter
remaining 257 software bytes will not be queued by this attempt
```

This is an admissible failure scenario to test against S3 sticky-event/MMIO
behavior, not an observed Unit fault. V4 changes the V2 failure shape: it can
truncate the software tail rather than leave refills enabled after the token.
Newly queued FIFO bytes may still be draining when IDLE is published. Starting
the next timed break at that point is not justified by that status.

A final-refill variant can bring head to 513 before the stale done is inspected.
Thus a head == size check alone is also insufficient. Detection needs event
ordering before/after FIFO writes, a qualified final drain and a fault latch,
as discussed in Stage 3A. Hardware idle alone is insufficient during underrun.

### Waits, stale notifications and recovery

V4 wait_sent(0) is a short status query, not an unconditional completion proof.
Positive waits use unindexed FreeRTOS task notifications without validating
head/progress/idle after wake. The public `dmx/include/driver.h:315-354` warns
about task-notification sharing. Installation starts IDLE, so true can also mean
initial availability with no completed frame.

`dmx_send_num` itself may wait up to 23 ms for prior sending, then wait on the
hardware timer for spacing. For ordinary DMX the spacing constant selected by
last_controller_pid == 0 is 176 us (`dmx/hal/include/timer.h:39`). Even after a
successful zero-wait precheck, this latter wait remains possible. Failure to
receive its expected timer notification within 20 ms reaches `__unreachable()`
(`dmx/io.c:370-376`), not an explicit recoverable start result. Therefore stock
v4 send is not a guaranteed nonblocking replacement for the proposed bridge.

A wait timeout leaves the attempt and timer/UART state alone. A subsequent send
may return zero after its internal wait, or accept once status becomes IDLE;
neither outcome identifies a previous partial packet. No public cancelled,
underrun, safely-retired or attempt-ID result supplies the required recovery gate.
Do not retry, reset FIFOs, delete or switch to Receiver merely because a wait
expired. No timeout/retry/fail-stop policy was implemented or approved here.

## 4. Is the Stage 3A Double Buffer Still Necessary?

**The invariant is necessary; the exact two-driver-buffer mechanism is not the
only possible implementation. V4 does not supply that invariant automatically.**

Two defensible future choices, both needing corrected completion/lifecycle:

1. Immutable driver snapshot: stage new values separately, copy once before
   start, make initial and all later FIFO reads use the frozen snapshot. Reject
   reuse until terminal quiescence. This structurally tolerates staging updates.
2. Frozen single V4 buffer: retain all updates in application-owned pending
   storage; forbid every dmx_write/slot/offset call while active or retiring;
   copy only at a corrected, exclusive ready boundary. This can avoid a second
   driver packet, but every writer and RX transition must obey that reservation.

Neither public stock-V4 wait plus a pending packet nor adding a snapshot alone
fixes partial-success classification. Snapshot release still requires a reliable
contract. With V2, reusing its already allocated buffer 1 provides structural
isolation without another driver universe; with V4, a driver double-buffer design
would require approximately another 513 bytes plus alignment. Do not pretend V4's
sniffer metadata is spare packet storage.

Required lifecycle remains: READY is availability, accepted START has identity,
ACTIVE storage is immutable, DRAINING requires all requested bytes queued and a
qualified current drain, abnormal RETIRING freezes storage until quiescent,
terminal COMPLETE is distinct from UNDERRUN/TIMEOUT/CANCELLED/START_FAILED.
Already transmitted partial bytes cannot be undone by successful retry.

## 5. Migration Surface and Side Effects

The unchanged application compile failed for genuine API mismatches, not an
ESP32-S3 compiler error. Required future integration work would include:

| Current coupling | V4 migration requirement |
| --- | --- |
| `src/common.h:6-10` checks DMX_PACKET_TIMEOUT_TICK to choose API v1/v2 | Recognize ESP_DMX_VERSION_MAJOR explicitly. V4 currently falls into the obsolete v1 branch. |
| DMX_MAX_PACKET_SIZE / timeout constants | Explicit 513-byte domain invariant; V4 DMX_PACKET_SIZE_MAX / DMX_TIMEOUT_TICK names. Keep preset serialization exactly 513 bytes. |
| `src/src.cpp:751-784` param_config, pins, old install signature | DMX_CONFIG_DEFAULT; install(port, config, personalities, count) before set_pin; check bool returns; explicitly check baud/break/MAB settings. No old event-queue output parameter. |
| Sender setup `src/view_sender.h:36-45` | Replace explicit mode API with a positively enforced Sender lifecycle gate; do not start TX just because driver installation enables RX. |
| Dispatcher publisher `src/view_sender.h:1053-1059` | Stage through transport, not a direct memcpy into active data. V4 write returns byte count, not esp_err_t. |
| Normal/wake sends `src/view_sender.h:82-93`, `src/src.cpp:298-312` | One checked transport path at existing opportunities; bool wait, accepted-byte-count send, and the blocking behavior need explicit handling. |
| Receiver `src/view_receiver.h:8-12,41-55` | Replace queue/event/read API with receive/read and preserve mode separation, completed-buffer read consistency and error behavior. |
| Receiver duration display | V4 dmx_packet_t has err, sc, size and is_rdm (`dmx/include/types.h:305-314`), not the old event duration. A wall-clock approximation is not automatically equivalent. Needs its own approved compatibility solution. |
| Restart/power-off `src/view_sender.h:1626-1653` | Existing helper does not start its alleged zero frames. API translation cannot fix or claim this guarantee; keep a separately scoped shutdown decision. |
| Tests/configuration | Update only scoped API/integration guards; preserve Controller, Dispatcher, UI, USB, NVS, profiles and release layout. Remove the v2 patch hook only in an approved v4 environment. |

For V4, write/send success means returned size == 513, not ESP_OK == 0. Simply
replacing function names while retaining existing result tests can invert success
handling. Receive and pin/install APIs also have different return types.

### Framing and timer resource change

V4 retains 250000 baud and 8N2, and its default break/MAB are nominally 176/12 us,
but it does **not** retain the legacy framing implementation/cadence. On IDF 4.x,
UART1 maps to timer group 0 / timer 1 (`dmx/hal/timer.c:90-109`). Break/MAB use
timer callbacks for every ordinary packet; RDM spacing shares that timer.
The IDF 4.x branch uses fixed auto-reload initialization, while the alarm setter
does not use its auto_reload argument. Actual edge-relative timing under delayed
callbacks needs capture; compilation does not prove minimum MAB or frame gaps.

The user permits the old approximately 188 us busy-wait for this evaluation and
excludes a new hardware-timer redesign. Therefore the Stage 3A strict-framing
extension is deferred. Stock V4 inherently brings a different timer path; an
upgrade must explicitly accept and validate that difference, not call it unchanged.
Keeping V2 avoids adding this variable to the ownership milestone.

### Lifecycle, memory placement and NVS

V4 uses a recursive mutex, UART and timer ISRs, RDM parameters and task
notifications. `dmx_driver_disable` refuses SENDING but otherwise only gates RX;
it is not safe cancellation of an attempt falsely marked IDLE. Deletion calls
timer/UART deinit before freeing storage. The inspected UART deinit only disables
the peripheral and does not free its allocated interrupt handle
(`dmx/hal/uart.c:374-388`); init also does not check esp_intr_alloc's return.
Any recovery design relying on repeated delete/reinstall needs a separate
lifetime/resource correction and proof, not an assumption of safe reinitialization.

IDF 4.x selects DMX_ISR_IN_IRAM (`dmx/include/service.h:38-46`). The probe map
places the timer/UART ISR sections and FIFO helpers in IRAM. This is useful, not
a complete cache-safety proof. For example dmx_timer_set_counter's SDK target
timer_set_counter_value is linked at 0x42024230 in flash in the Unit probe; call
context and the entire cache-disabled call graph still need review. Do not infer
an ISR crash from that address alone. Driver allocation uses MALLOC_CAP_8BIT,
not explicitly MALLOC_CAP_INTERNAL (`dmx/driver.c:112-115`); verify actual
ISR-accessed buffer placement with the project's PSRAM policy before acceptance.

V4 installation calls NVS initialization and registers default RDM parameters
(`dmx/driver.c:109-223`). `dmx/hal/nvs.c:7-26` uses the default nvs partition
and the distinct esp_dmx namespace. No NVS erase was found in this helper; reads,
writes and commits do exist. This does not authorize preset/settings migration
or partition changes. Parameter allocation, optional queue and flash operations
would need resource/load tests. The default config provisions 32 root parameters
and a 32-entry RDM queue, not the old UART receive-event queue.

Migration complexity is moderate for API compilation, higher for behaviorally
equivalent transport/Receiver/lifecycle validation. A targeted V4 completion
patch might have a smaller local TX_DONE diff than V2, but the surrounding
upgrade is broader. No implemented diff-size or full-application memory claim
is justified by this evaluation.

## 6. Compilation Experiments and Results

Commands were run only from `P`, never the production repository root:

```powershell
pio run -e probe_unit
pio run -e cores3se_unit
pio run -e probe_unit -e probe_base
```

The first command built a minimal official-API probe. The middle command was an
intentional negative test using a byte-for-byte source/include copy of the
current application with only the isolated dependency/configuration changed.
The final command built both profile probes and recorded their combined log.
Copying the application files caused the Unit probe to be rebuilt; its final
size/result was unchanged. No API shim or adapted production firmware was made.

`P/platformio.ini` preserves platform, board, framework, M5 pins, build flags,
USB flags, monitor/upload speeds and flash configuration. It replaces only its
own dependency with official v4.1.0 ZIP and omits the v2 patch hook. Per-environment
source filters distinguish probe.cpp from copied application translation units.
Each environment installs dependencies in its own isolated libdeps directory.

The compile probe references installation, profile pins, explicit 176/12 us
settings, write, send_num, wait_sent(0), receive_num(0) and read. It does not
exercise runtime correctness or duplicate the touchscreen application. It was
not uploaded and is not intended as usable diagnostic/controller firmware.

| Isolated experiment | Result | Static RAM | Reported flash use |
| --- | --- | ---: | ---: |
| probe_unit, Unit TX2/RX1/EN-1 | PASS | 23096 bytes | 517789 bytes |
| probe_base, Base TX7/RX10/EN6 | PASS | 23096 bytes | 517789 bytes |
| Unchanged current application, cores3se_unit + V4 | Expected FAIL, exit 1 | No linked image | No linked image |

Representative application errors: DMX_MAX_PACKET_SIZE, dmx_event_t,
dmx_set_mode/DMX_MODE_RX/TX, dmx_read_packet, dmx_write_packet,
dmx_wait_tx_done, dmx_tx_packet, DMX_DEFAULT_CONFIG and dmx_param_config are not
the corresponding V4 API. The old version detection compounds these errors.
Do not report a successful migrated full application or four diagnostic builds;
none was attempted. Once shared API code fails, repeating unchanged Base and
diagnostic compilations would not establish additional compatibility.

Both successful builds resolve M5Unified 0.2.21, M5GFX 0.2.28 and esp_dmx 4.1.0.
PIO initially installs transitive M5GFX >=0.2.28 as 0.2.34, then installs the
explicit 0.2.28 requirement; the actual dependency graphs/build paths select
0.2.28. An upstream discarded-const warning at `dmx/driver.c:214` occurs when
registering the manufacturer label. It is not an S3 register error or build failure.

Evidence retained under `E`:

```text
official-api-probes.log
unchanged-app-unit.log
upstream/                         exact detached official source checkout
project/platformio.ini
project/src/probe.cpp
project/.pio/build/probe_unit/    ELF/map/application binary, not FULL firmware
project/.pio/build/probe_base/    ELF/map/application binary, not FULL firmware
```

These ignored artifacts are not public release assets. The static sizes above
exclude runtime driver/parameter allocations and the real application's UI.
They must not be subtracted from Stage 2 sizes to claim a migration RAM/flash
saving. No new full-flash merge, filesystem image or partition change was made.
The existing 8 MB configured layout and 16 MB measured physical flash remain as-is.

## 7. Recommended Architecture and Explicit Pending Decisions

Retain the single app-task Controller/Dispatcher ownership and the existing
normal/wake frame-start opportunities. Driver buffers are transport copies,
not additional DMX state authorities. No networking or sender task is introduced.

For the preferred V2 candidate, use Stage 3A's buffer 0 staging / buffer 1
immutable active snapshot, plus a qualified attempt/result lifecycle and
quiescence barrier. Keep the existing S3 patch separate. Repository-local patches
must validate the complete expected source set before writing, be idempotent,
reject partial/unknown states and work from fresh dependency installation.
No hand edits to a production .pio installation are a supported solution.

### Blackout is an ordered event, not just latest-value sampling

This evaluation supersedes the Stage 3A option that silently samples ON then OFF
away. Preserve Controller's entry snapshot and discard-edits-on-release rules,
but separately preserve accepted blackout edges in transport order:

```text
active A -> accepted ON barrier (zero packet) -> accepted OFF barrier (restore)
               full confirmed zero before consuming the OFF barrier
```

Recommended proposal for approval: each accepted transition gets a bounded
ordered barrier with its immutable effective packet. Channel updates may coalesce
only within the segment between barriers, not across them. OFF must capture the
correct restored state at command application, not read a future mutable packet.
Unchanged retransmissions continue. Define whether preset LOAD is also a barrier
or a coalescible whole-state update; do not silently invent this policy.

Reserve transport capacity **before** applying a command whose edge needs a
barrier. If bounded storage is full, defer the queued command or return explicit
BUSY before mutation/acceptance; never mutate Controller then drop the edge in
the publisher callback. Dispatcher command FIFO alone is insufficient if its
publisher overwrites one pending packet. Backpressure must preserve order of
subsequent commands. Exact queue depth/admission/UI feedback require review.

At two pending barrier packets, payload storage is 1026 bytes plus descriptors,
in addition to a 513-byte latest-channel packet and contract metadata. This is
an illustrative bounded budget, not an implemented queue depth. No finite queue
can promise every arbitrarily fast edge without admission/backpressure. Retry
must retain the oldest unconfirmed barrier, not replace it with latest OFF.
Acceptance of this stronger wire-event policy and its latency is required.

### Recovery proposal, not an approved policy

Use bounded poll work and distinguish wait expiry from cancellation. On detected
underrun or indeterminate final-refill ordering, latch failure, stop future
refills, retain active storage, establish FIFO/FSM/ISR/framing quiescence and only
then release the attempt abnormally. A timeout must request the same retirement
handshake; it must not reset progress and start another attempt in place.

Candidate recovery choices to approve after measurements:

| Condition | Proposed behavior to review |
| --- | --- |
| One underrun, retirement succeeds | Retry the oldest required event using fresh framing and a new attempt ID; coalesce only permitted channel values within that event segment. |
| Wait/poll delayed but full packet already completed | Validate qualified completion before classifying an elapsed deadline as failure. |
| Repeated failed attempts | Bounded retries/backoff with an explicit health result; numeric deadlines and retry limits to be chosen from measured workload. |
| Cannot establish quiescence | Keep storage reserved and UI responsive; choose explicit fault-latched/manual recovery policy before implementation, rather than implicit reset/reuse. |
| Receiver request or shutdown while active | Drain or explicitly retire through the same boundary; no direction switch/free/power action based solely on wait timeout. Shutdown zero-frame semantics remain separately scoped. |

Do not adopt the older report's example 100 ms/three-retry values by default.
No auto-reset, UART reinstall, silent blackout, NVS erase, automatic power action
or fail-stopped policy is authorized by this evaluation. A persistent hardware
fault cannot have an unconditional eventual-delivery guarantee; pending final
values/events must survive and never be falsely acknowledged as complete.

## 8. Acceptance Gates for an Approved Candidate

### Automated software tests

Test the actual shared transition core and actual patched ISR integration, not
just a fake wait function that returns success. Model independent hardware time,
FIFO occupancy, shifter/FSM, raw sticky flags, IRQ masks and task/ISR ordering.

| Scenario | Required assertion |
| --- | --- |
| Normal 513-byte packet | Start code zero, one generation, exactly 513 queued, qualified drain before COMPLETE. Initial READY is not completion. |
| Rapid channel edits / final release | Active bytes remain immutable; newest permitted final value survives completion and errors without another touch. |
| Deliberate FIFO underrun | Starve refill until FIFO drains with software tail pending; no false success; distinguish V2 future-read risk from V4 disabled-refill truncation. |
| Stale done near final refill | Inject before/during/after write and before/after arming done; head == requested alone cannot yield success. |
| Late ISR / timeout then restart | No retired-buffer read, stale attempt completion, memory reuse or new start before quiescence. |
| Blackout ON then OFF before TX boundary | ON remains an ordered zero-frame obligation; OFF cannot erase it; capacity pressure is explicit and mutation/admission order is consistent. |
| Blackout/preset during TX | Whole packets, original entry restore semantics, no old-prefix/new-tail; preset policy explicit. |
| Mode/shutdown/lifecycle | No sender service in established Receiver, no RX writes to TX-owned storage, no free/cancel while referenced; do not fabricate shutdown zero sends. |
| Build integration | Existing Controller/Dispatcher/guards pass; all four normal/diagnostic envs build; fresh/repeated/mixed/unknown patch-state tests; whitespace and source-scope checks. |

No new fault-injection suite was implemented or run in this evaluation. Source
analysis plus compilation is not a simulated or hardware underrun test result.

### External hardware and workload tests

Capture unchanged Stage 2 normal Unit first with exact source/environment/binary
hash. Then, only with authorization, compare a candidate using a suitable RS-485
receiver/differential measurement setup, not unsafe direct logic probes on the bus.

Measure baud/8N2, break and MAB edges, zero start code, 512 data slots, complete
packet patterns, inter-frame gaps and command-to-wire latency. Nominal calculations
are 22572 us payload and 22760 us including 176/12 framing, not measured limits.
Set acceptable latency/gap budgets from the real baseline before calling a
candidate equivalent. Retaining the old busy-wait does not promise identical
cadence once a truthful drain barrier is added.

Use known full-universe A/B patterns, rapid sliders, Precision/keypad, presets,
blackout ON/OFF faster than a frame, repeated edges/queue pressure, Settings,
NVS saves, battery/speaker activity, backlight sleep and consumed-wake gestures.
Verify final values and ordered zero/restore frames externally. A fault-injection
build must deliberately delay refill safely, including the last refill, and
correlate partial attempts/recovery with aggregate software results. Never count
a captured partial attempt as a successful frame. Do not ship injection enabled.

For a future V4 candidate, additionally test timer ownership, actual minimum MAB
under delayed callbacks/cache-disabled NVS work, RDM-notification isolation,
Receiver duration/readback compatibility and repeated lifecycle cleanup. Probe
compilation alone establishes none of these properties.

Normal Unit is the primary target. Preserve the USB investigation: diagnostics
auto-reset/stub failures are confirmed, normal Stage 2 flash_id passes at 115200
and 1500000 are reported, but repeated actual uploads/monitor/reboot acceptance
is still pending. Do not change USB initialization, diagnostics or upload speed
to accommodate a transport candidate. Keep the stale diagnostic build identifier
as an acknowledged provenance limitation, with a future separate correction.

Test Base separately only after Unit passes; its existing glitches remain open.
Host tests cannot establish RS-485 electrical correctness, receiver acceptance,
frame length/timing or absence of observed fixture glitches.

## 9. Repository Safety and Rollback

Only this new report is intended for the repository. Temporary probe/config/source
copies, upstream clone, logs and build artifacts stay under ignored `E`.
Production src/include/scripts, platformio.ini and installed esp_dmx copies were
fingerprinted before evaluation and checked afterwards; none was modified.
The three input reports, historical development folder, logs and existing stash
are preserved. No branch switch, history modification, upload, commit, merge,
push, tag, release or packaging operation was performed.

Rollback remains the exact Stage 2 commit and preserved verified binaries. Build
any rollback in another output directory without resetting this working tree or
touching stashes. A future device rollback requires explicit authorization and
the verified app offset/layout, preserving NVS/OTA data; do not use a new FULL
image or chip erase as an implicit recovery step.

## 10. Proposed Implementation Sequence, Awaiting Approval

1. Approve the preferred V2 target and the legacy framing-wait exception. Confirm
   ordered blackout barriers, preset publication policy and admission behavior.
   Select timeout/retry/fault recovery policy separately; no inherited defaults.
2. Implement only the isolated snapshot/completion transition core and deliberate
   fault-injection host tests, using actual ISR adapter coverage. Resolve raw
   event/MMIO/quiescence ordering before integrating application call sites.
3. Add a separate deterministic repository-local V2 contract patch with capability
   checks, leaving the existing S3 getter patch unchanged. Verify fresh, repeated,
   mixed and unknown dependency states without touching the preserved baseline.
4. Integrate the app-task transport with pending generations and bounded ordered
   blackout events at existing normal/wake opportunities. Preserve Controller,
   UI, Receiver parsing, presets/NVS, power/USB, pins and configured DMX timing.
   Stop rather than silently selecting an unapproved recovery or event policy.
5. Run host/integration/patch tests and all four builds. Report real RAM, heap,
   flash/IRAM and scheduling deltas; do not use these probe sizes as the baseline.
6. Await explicit upload approval, then perform normal Unit regression/external
   DMX capture and recovery tests. Evaluate Base independently. Commit/release
   only after subsequent explicit approval, never automatically.
7. Revisit V4 migration as a separate maintenance milestone if official upstream
   adopts an adequate contract or a measured need justifies its broader API/timer
   migration. Pin and review that exact source again; do not assume a tag upgrade
   establishes safety.

**Concrete recommendation: keep production unchanged now; approve the isolated
V2 snapshot/completion test milestone next, not an unpatched V4 upgrade.**
