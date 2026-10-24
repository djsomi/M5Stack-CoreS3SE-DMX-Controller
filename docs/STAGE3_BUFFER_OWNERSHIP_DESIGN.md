# Stage 3A: DMX Completion and Buffer Ownership Design

**Design for review, not implemented.** Source baseline:
`4c973007d74a1f24319e0e375a56107bc261b784`, branch
`codex/stage3-dmx-transport`. No firmware, dependency, patch hook, USB setting,
diagnostic behavior, partition, packaging or device state was changed.

## 1. Recommendation and Evidence Boundary

Recommend **B + a narrowly scoped C**: use the existing driver buffer 1 as an
immutable active TX snapshot, and patch v2.02's TX progress/result lifecycle.
Keep buffer 0 as writable TX staging. Reuse the existing allocation; do not add
a third driver universe, upgrade esp_dmx, or replace the UART transport.

Snapshot isolation alone fixes ownership, not false completion. A qualified
normal result must require the entire requested packet to have been queued,
no detected underrun/abort, no possible future refill of that attempt, and
confirmed UART drain/idle. Errors retire an attempt; they never complete it.

This is the smallest defensible **combined contract**, not the smallest diff.
A one-line counter check or switching just one TX read to buffer 1 is inadequate.
The earlier source-level underrun counterexample is conditional: it has not
been measured on the Unit, and is not evidence about experimental Base glitches.

There is a separate scope decision: current first/long-gap framing busy-waits.
Strictly nonblocking application calls require the isolated hardware-timer
framing extension in section 8. Retaining the approximately 188 us legacy
framing wait is a smaller ownership-only change, but does **not** satisfy a
literal no-busy-wait requirement. Do not silently claim otherwise or substitute
an ordinary application-loop/FreeRTOS timer for microsecond framing.

No design document proves electrical correctness. The contract below is a
software specification subject to implementation tests, MMIO/interrupt-order
validation and external RS-485 acceptance. Existing stable Unit operation is
the comparison baseline, not an argument to skip those gates.

## 2. Exact Source Identity and Reference Map

The following installed files were re-read. All four environment copies match
the listed SHA256 values. Library metadata is 2.0.2 from upstream tag v2.02;
Arduino is 2.0.16, installed IDF headers 4.4.7, platform espressif32 6.7.0.

| File under esp_dmx/src | SHA256 |
| --- | --- |
| esp_dmx.c | `9ddc50e211e768ff1c1612fc803e973071b22e6b18c60ebae9800cd288e497b4` |
| impl/intr_handlers.h | `f841719683ff5c8aaf24c07a650c6746feb5a5ef52a2dc7d78df187117adbd43` |
| impl/driver.h | `5aa8ba4c1d361d2fff6ac5e58aff9fd5db3562f7d260f0f4549612e8288be626` |
| impl/dmx_hal.h | `bb5d12b60c1aa121448f54868215570d9bd20c6f8db61e36d897f18ceb3fcf8b` |
| impl/dmx_ll.h, already S3-patched | `e2c294fd91101ce2ac3b18e73e77ffcdb8e2b2c20c3579a8e1857f8598e3487c` |

Existing `scripts/patch_esp_dmx_v202.py` SHA256:
`c47110e3c1cc082a810676438cc55e6e48c9fc02dfa50731d4a3480b425e6ed1`.
Its three changes are only S3 field getters:
`uart_idle_conf_reg_t.tx_idle_num -> idle_conf.tx_idle_num`,
`uart_txbrk_conf_reg_t.tx_brk_num -> txbrk_conf.tx_brk_num`, and
`uart_status_reg_t.rxd -> status.rxd`. Keep that compatibility patch separate.

References are one-based lines in these exact local files. `D/` below means
`.pio/libdeps/cores3se_unit/esp_dmx/src/`; `SDK/` means the installed
`framework-arduinoespressif32/tools/sdk/esp32s3/` directory.

| Ref | Exact location | Relevant behavior |
| --- | --- | --- |
| R1 | `D/esp_dmx.c:144-175` | Internal two-buffer allocation, buf_idx=0, initial available semaphore. For 513-byte buffers the existing Arduino allocation expression allocates 1028 bytes, including padding. |
| R2 | `D/esp_dmx.c:182-207` | Custom IRAM ISR, FIFO thresholds; TX low-water default is 8 at line 26. |
| R3 | `D/esp_dmx.c:215-250` | Deletion frees ISR and packet allocation; no explicit graceful TX completion contract. |
| R4 | `D/esp_dmx.c:258-309` | READ disables TX IRQs, clears flags and resets RX index; WRITE disables RX, clears flags, gives token and resets TX FIFO/buffer 0, but not buf_idx. |
| R5 | `D/esp_dmx.c:642-699` | READ-mode readback uses inactive RX buffer. WRITE-mode readback returns buffer 0, not buffer 1. |
| R6 | `D/esp_dmx.c:701-740` | Unprotected packet/slot writes into buffer 0. |
| R7 | `D/esp_dmx.c:743-815` | Send takes token, optionally synthesizes framing, prefills from buffer[buf_idx], enables TX IRQs. |
| R8 | `D/esp_dmx.c:817-829` | Wait takes/gives token only. No mode, progress, fault or physical-idle test. |
| R9 | `D/impl/intr_handlers.h:43-65` | ISR prioritizes FIFO refill; reads buffer 0 and increments slot_idx. Disables refill only at send_size. |
| R10 | `D/impl/intr_handlers.h:66-90` | TX_DONE copies buffer 0 to 1 and gives token without qualifying progress; break/idle IRQs only clear flags. |
| R11 | `D/impl/intr_handlers.h:93-201` | RX writes buffer[buf_idx], toggles buf_idx on break, publishes RX events. |
| R12 | `D/impl/driver.h:25-53` | Private driver packet pointers, shared RX/TX slot_idx, send_size, mode, semaphore and timestamps. |
| R13 | `D/impl/dmx_hal.h:26-57,162-170,288-308` | Masked IRQ status, ISR FIFO access, temporary UART defaults and FIFO reset. HAL get_txfifo_len returns writable capacity, not occupied length. |
| R14 | `SDK/include/hal/esp32s3/include/hal/uart_ll.h:283-287,310-337,777-780` | FIFO writes/reset/capacity; is_tx_idle is FIFO count zero AND TX FSM state zero. FIFO reset alone is not documented as stopping the shifter. |
| R15 | `SDK/include/soc/esp32s3/include/soc/uart_struct.h:91-95,146-160,536-540` | FIFO-low, break/idle and TX_DONE raw bits; TX_DONE describes FIFO data, not the driver's software tail. |
| R16 | `D/esp_dmx.h:28-29`; `D/dmx_caps.h:9-11,27` | Final configured 250000 baud, break_num 44, idle_num 3, packet 513, one-second restart-framing threshold. |
| R17 | `src/view_sender.h:36-45,82-93,471-472,1053-1059` | Setup, normal send opportunity, Dispatcher publisher and immediate packet write. |
| R18 | `src/src.cpp:298-312,815-869,911-938` | Consumed-wake TX opportunity, early command processing/returns, backlight sleep and scene dispatch. |
| R19 | `src/view_sender.h:1626-1653,1701-1732` | Existing zero-before-power helper and immediate power-action callers. |
| R20 | `src/command_dispatcher.cpp:132-140`; `src/dmx_controller.cpp:68-83` | Output publication trigger and logical blackout snapshot semantics. |
| R21 | `SDK/include/driver/include/driver/timer.h:185-211,344-388` | Legacy IDF 4.4 timer ISR registration and ISR-safe alarm/counter APIs. |

The SDK declares a 128-byte UART FIFO (`soc_caps.h:263`). The hardware idle
predicate cannot alone prove that all requested software slots were enqueued.
Conversely, slot_idx==send_size alone does not prove the UART has transmitted them.

The matching [IDF 4.4.7 UART guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/peripherals/uart.html#using-interrupts)
warns that stock high-level UART APIs are not automatically usable with a custom
ISR. Do not install the stock UART driver or use its completion queue as a fix.

## 3. Current Root Cause and Limits

### Two independent software-contract defects

1. **Mutable active source:** R6 writes buffer 0; R9 reads it over multiple FIFO
   refills. The ISR can see old-prefix/new-tail values or interrupt a packet copy.
   Existing buffer 1 is not an immutable source: the initial fill uses buf_idx,
   later fills use buffer 0, and the copy to buffer 1 occurs only at TX_DONE.
2. **Availability confused with completion:** R8 exposes a binary token. R10
   gives it for FIFO TX_DONE without checking the unqueued tail or retiring refill
   access. Initial install/mode entry also gives it with no completed packet.

In the underrun counterexample, FIFO-low and TX_DONE are both latched. The ISR
refills first, increasing progress, then handles an older TX_DONE. Merely checking
the now-increased slot count can misclassify that older event, including after
the final refill. A FIFO event must be evaluated **before refilling**; raw bits
must remain observable even when their interrupt delivery is masked.

The existing implementation has no explicit TX fault/abort result, TX deadline,
attempt ID or coherent quiescence handshake. A wait timeout changes nothing:
it neither cancels the packet nor makes its storage reusable. Changing mode can
disable refills while old FIFO/shifter data remains, and can switch direction
before a complete packet. These are source observations, not measured failures.

**Refinement:** WRITE-mode dmx_read_packet/read_slot return staging buffer 0
(R5). Reusing buffer 1 as active TX therefore need not change TX readback to
mean something new. Neither readback is fixture acknowledgment.

## 4. Alternatives

| Model | Required work | Strength / limitation |
| --- | --- | --- |
| A: improved completion, freeze buffer 0 during TX | Correct fault/progress/quiescence state; reject or defer every buffer-0 writer; remove/control completion copy; gate all callers | Defensible with a real contract, not a token-only change. Smaller buffer-pointer diff, but changes the existing write-during-TX behavior and depends on every writer respecting the active buffer reservation. |
| B: immutable active snapshot only | Copy before start, use snapshot in initial and every ISR fill, never copy into it at completion | Separates staging from active memory. Does not make an early completion truthful or prevent premature snapshot reuse if the old token remains authoritative. Insufficient by itself. |
| C: one-line TX_DONE progress check | Check slot_idx==send_size | Insufficient: a stale TX_DONE can be inspected after refill advanced the counter. Does not cover timeouts, later reads or mode transitions. Reject. |
| B + focused C: snapshot plus qualified result lifecycle | Reuse buffer 1, protect start/retirement, raw-event/progress checks, result/status API, safe cancellation and mode entry | Recommended. Strong local ownership invariant; preserves writable staging and existing allocation. Changes narrowly identified TX paths rather than upgrading the library. |
| New library/fork, DMA transport, autonomous TX task | Broader API and timing changes | Not justified for this step. No presumption that another version fixes these conditions. |

The app retains a single authoritative Controller. Driver buffers are transport
copies, not additional domain-state authorities. No networking or extra command
thread is introduced. The app's latest-pending copy is necessary for coalescing,
error recovery and explicit generation tracking, not a queue per edit.

## 5. Chosen Ownership and Synchronization Contract

### Storage roles

| Storage | Owner and permitted mutation |
| --- | --- |
| Controller working/blackout/restore packets | Existing application task only; unchanged domain rules and NVS format. |
| P: DmxTransport pending[513] | Application task; replaced atomically at command boundaries by the latest complete effective output, start code 0. No ISR access. |
| S: driver buffer 0, WRITE mode | Application-side driver staging. Writes permitted during active TX, but rejected while a snapshot copy reservation is held. No TX ISR reads. |
| A: driver buffer 1, WRITE mode | Snapshot copied S->A only during PREPARING with TX/timer access retired and RX disabled. Immutable from accepted start until terminal quiescence. Initial fill AND all refills read A. |
| Two buffers, READ mode | Existing RX alternating-buffer roles resume only after TX/timer retirement; no concurrent TX snapshot use. RX storage format/event semantics are unchanged. |

The 513-byte snapshot copy moves from TX_DONE ISR context to task-side start.
No end-of-TX copy into A remains. No per-frame allocation. Both buffers already
reside in internal 8-bit-capable RAM; do not move ISR data into PSRAM.

### States and results (proposed, not code)

```text
READ
READY -> PREPARING -> [PRE_BREAK -> PRE_MAB] -> FEEDING -> DRAINING
                                                         |          |
                                                         +-> RETIRING <-+
DRAINING -> READY + terminal COMPLETE
RETIRING -> READY + terminal UNDERRUN / TIMEOUT / CANCELLED / START_FAILED
RETIRING -> FAULT_STOPPED if physical quiescence cannot be established
```

READY describes availability. COMPLETE describes a specific accepted attempt;
initial READY is not a fabricated COMPLETE. A terminal latch survives polling
until a subsequent accepted attempt replaces it. A distinct attempt ID and pending
generation are retained, including when identical values are retransmitted.

One application task owns start/write/mode/cancel/delete calls. A small per-port
spinlock protects every shared state transition and bounded ISR FIFO read. It
must be used by **all** TX ISR branches and the optional framing ISR, not only
the task. No heap operation, logging, 513-byte snapshot copy, frame wait or delay
inside that lock. An ISR refill is bounded to the FIFO's available capacity.

Task snapshot preparation reserves PREPARING under the lock, releases it to copy,
then re-enters to arm the attempt. Start/write/mode/delete refuse conflicting
reservations. This avoids masking interrupts for the full snapshot copy. ISR
branches re-check state under the lock before taking any packet pointer.
RX buffer access must also participate in the mode/ownership synchronization,
or be positively synchronized with the owner/install core during transition.
Masking RX delivery alone cannot retire a handler already executing on another
core. Preserve RX parsing/event policy; do not confuse that with omitting its
buffer-lifetime barrier. Reject driver calls from a different application owner.

Short critical sections and fixed copies are not mathematically lock-free, but
contain no wait for transmission, a timer expiry, another task's application
work or a host. Measure their worst duration; do not advertise a hard real-time
bound from operation counts alone. No pointers into mutable Controller/P storage
are retained by the ISR.

### Invariants required in tests and code review

1. A can change only while neither UART ISR, timer ISR nor RX can reference it.
2. Every TX packet read is from A; a stale RX buf_idx cannot select S.
3. 0 <= queued <= requested <= buffer_size; project requests exactly 513 bytes.
4. COMPLETE(id) implies all requested bytes queued, a qualified current drain,
   no fault for id, no future refill and no active framing override.
5. Any underrun/timeout/cancel permanently disqualifies that attempt from COMPLETE.
6. Every accepted attempt has at most one terminal result. A busy/rejected start
   cannot consume the pending request or fabricate an accepted attempt.
7. Mode READ and deletion require all TX/timer references retired, not just a
   disabled interrupt bit. A timeout never transfers buffer ownership by itself.
8. Hardware interrupts carry no software attempt ID. Stale IRQ exclusion depends
   on mask/quiesce/clear/rearm ordering, not merely attaching a counter to status.

## 6. Completion, Underrun and Recovery Algorithm

### Accept and arm a start

- Validate port, WRITE mode, size, snapshot reservation and quiescent READY.
  Reject BUSY immediately; do not take a frame-length wait.
- Mask/clear the prior TX interrupt sources while quiescent. Confirm the UART
  is drained, framing timer retired and line inversion normal. Assign a new
  nonzero attempt ID. Serialize wraparound at quiescence; compare IDs by equality,
  not unqualified greater-than arithmetic. Old task status is not a new attempt.
- Copy the complete staged packet S->A under PREPARING reservation, outside the
  short lock. Clear prior fault/qualified-done state. Keep a configured deadline.
- For ordinary continuous output, initial FIFO fill reads A. Check actual bytes
  accepted, update queued, then arm FEEDING. For first/long-gap/recovery starts,
  run the framing substate before this fill. Publication acceptance is not yet
  proof of a FIFO start or a completed transmission.

Record the actual first-FIFO-write software timestamp separately from acceptance
and terminal observation. For the one-second restart-framing guard, use a
conservative age bound from that start (or a genuinely established hardware edge
timestamp), not the time a delayed poll/ISR finally reports completion. The UART
may already have been silent for a long time before that report. Using start age
can request an extra break slightly earlier near the one-second boundary, but
cannot mistake an arbitrarily old break for a new one. This affects exceptional
long gaps, not ordinary continuous framing, and must be captured/tested explicitly.

### FEEDING: do not let FIFO-low processing erase an earlier finish

Mask **delivery** of TX_DONE while software slots remain, but observe its sticky
**raw** bit. Existing masked-int status is insufficient for that check; add a
small S3 raw-status HAL helper. Raw bits are not made harmless by masking delivery.

At each refill, under the common lock:

1. Inspect raw TX_DONE before changing queued or writing more FIFO bytes. If it
   is set while slots remain, latch UNDERRUN and enter RETIRING **without refill**.
2. Otherwise copy the bounded next slice from A and advance queued by the actual
   accepted count. Re-read raw TX_DONE after the write, before arming completion.
3. If TX_DONE became latched during this refill, retire as underrun/indeterminate
   failure, even if queued just reached requested. It may be an older drained-FIFO
   event. Conservative rejection is preferable to false success. Under extreme
   preemption this can reject a physically finished packet; do not count it good.
4. If more slots remain, clear only the handled FIFO-low flag and return. Never
   clear a raw done indication and continue as if an underrun did not happen.
5. After the final refill with no raw done observed, disable refill delivery and
   enter DRAINING. Arm done delivery without clearing a newly arriving raw done
   after the post-write check. An event between that check and enabling delivery
   must remain visible. Keep A frozen.

Apply equivalent checks around the initial prefill. Polling may latch a missed
raw underrun as well, using the same lock/state functions; it cannot reset the
fault or advance the FIFO. The input model must include independent sticky raw
bits, mask bits and hardware progress during an ISR, not an idealized done flag.

### DRAINING: a done IRQ is a hint, not immediate success

The qualified TX_DONE path records a current-drain observation and clears its
handled flag. It does not overwrite A or immediately give successful ownership.
If needed, disable repeated done delivery while the task polls the final state.

Finalize COMPLETE only when queued==requested, refill is disabled, no fault is
latched, a post-final-refill done indication has been observed, and the SDK idle
predicate is true (FIFO empty AND TX FSM idle). Timer framing must also be inactive
and TX inversion normal. This can be checked once by poll or a subsequent relevant
IRQ; **never spin in the ISR until idle**. If done arrives before the UART finishes
its automatic break/mark phase, remain DRAINING and finalize on a later poll.

The idle gate may add a service interval to the inter-frame gap compared with
Stage 2. It does not change the configured baud/break/MAB, but identical cadence
cannot be promised. Compare measured gaps and fixture behavior before approval.

The proof depends on the S3 raw-event and MMIO ordering described above. Validate
events immediately before/during/after the final FIFO write and arming window
in host tests and a hardware probe. If real hardware ordering contradicts the
model, stop before app integration; do not weaken the invariant to make a test pass.

### Retirement is not completion

On underrun, timeout, cancellation, inconsistent progress, collision while in
ordinary TX, or a failed start:

- Latch the first error for the attempt; disable further FIFO refill and prevent
  any new start. Future/stale ISR branches must observe RETIRING before reading A.
- Keep A frozen. Already queued bytes/shifter data may drain; do not reset the
  progress counter, enable RX, clear the fault or reuse A simply because a deadline
  expired. A truncated prefix may already be on the wire; software cannot unsend it.
- Quiescence requires UART idle, refill/timer access retired, normal TX inversion,
  and any required final mark settled. Normalize a cancelled pre-break through
  a bounded asynchronous mark phase; UART idle alone cannot detect a forced-low
  TX inversion. Mask/clear old TX/timer flags only after this barrier.
- Then publish the abnormal terminal result, release A, and allow recovery. The
  next attempt requires a fresh break/MAB, not concatenation to a failed prefix.
  Never report that failed packet as successfully delivered.
- If physical quiescence cannot be established, enter FAULT_STOPPED, retain A,
  and report health without blocking the app. Do not free/reuse the allocation or
  automatically reset the device. A future explicitly designed UART reset/reinit
  path is separate; FIFO reset alone is not a proven shifter/ISR cancellation.

Proposed initial policy for review: 100 ms active-attempt deadline, 100 ms retire
deadline, 100 ms retry backoff and at most three consecutive failed attempts before
latching fault-stop. Check qualified completion before expiring an overdue poll;
long NVS/UI work can delay polling after the packet actually finished. Reset the
consecutive-failure count only on a successful frame. These values are engineering
defaults, not measured limits; freeze them in tests and validate baseline workload
before enabling. Retry always uses the latest retained P, not a queue of stale
slider values. No automatic blackout, preset alteration or silent power action.

No finite design can promise delivery under a permanently failed UART or infinitely
starved task. The guarantee is preservation of the latest request and no false
success; eventual delivery requires restored hardware and servicing progress.

## 7. Semaphore and Mode Compatibility

The state/result record is authoritative. Retain the binary semaphore only as an
advisory wake mechanism for the legacy wait API, never a packet counter, lease or
completion proof. Start admission uses the state reservation, not a token alone.

Legacy dmx_wait_send_done must consult the new state, including an initial READY
case explicitly documented as availability. A nonzero wait may retain its old
blocking calling convention by waiting in bounded tick slices and rechecking
state; no new application bridge calls use that form. A wake on abnormal terminal
state returns an error, not ESP_OK. A wake before full idle cannot fabricate
COMPLETE. wait(0) performs at most one bounded poll and returns immediately.

TX-to-RX: stop accepting new starts, finish or explicitly retire the active attempt,
then switch direction and enable RX only at the barrier. Until then the application
is mode-transitioning, not an active Receiver. Do not drain a sender frame after
announcing READ mode to the UI. Unexpected direct set_mode/delete while active
must reject busy rather than truncate/reuse memory. The app adapter requests a
transition and polls; the Receiver's existing blocking read starts only after
READ is established. No automatic background TX in READ.

RX-to-TX: under the shared lifecycle guard, disable/retire RX IRQ access, clear
relevant flags and reset progress/index before assigning staging/snapshot roles.
Use A explicitly for all TX reads, independent of the old RX index. RX event
queue format and receiver parsing remain unchanged. Test both starting RX indices.
Destroy only after UART/timer ISRs are disabled, synchronized and cannot reference
the driver object. Timer callback lifetime is part of deletion, not an afterthought.

## 8. Strict Nonblocking Framing: Separate Necessary Extension

Unmodified v2.02 calls ets_delay_us for a synthesized break and MAB on first send
and after at least a one-second gap (R7). Current settings calculate 176 us break
and 12 us MAB; the code itself notes timing inaccuracy. Keeping it is a valid
minimal ownership-only experiment **only with an explicit bounded-wait exception**.

For the stated strict nonblocking requirement, recommend an isolated fixed
one-shot **hardware timer** framer, used only for initial/long-gap/fault-recovery
framing. Normal continuous frames retain UART automatic trailing break/mark.
No timer per DMX slot, periodic transmit task or FreeRTOS packet queue.

Proposed sequence:

```text
task accepts/fixes A; UART quiescent; assert TX-only inversion; arm break alarm
PRE_BREAK alarm: restore normal TX; arm MAB relative to the actual restore edge
PRE_MAB alarm: prefill from A; enter FEEDING; enable the qualified refill path
```

Use current configured durations, not HAL temporary 45/5 defaults or a guessed
millisecond delay. Preserve direction control and unrelated inversion bits. The
start call returns after bounded copy/arming, without waiting for either edge.
Timer setup/allocation is once before operation; failures leave TX disabled and
are reported. No silent fallback to busy-waiting under strict mode.

The installed S3 SDK has two groups/two timers each. Candidate reservation is
TIMG1/T0 at an audited 1 MHz count, but **reservation is not yet proven on hardware**.
No direct timer allocations were found in the current project/M5Unified/M5GFX
sources. IDF esp_timer uses SYSTIMER in this SDK. This is not proof that all linked
code leaves TIMG1/T0 unused. Establish an explicit reservation and fail rather than
reinitialize a timer belonging to another component. Do not touch watchdog timers.

Use installed IDF 4.4 driver/timer.h, not a newer GPTimer API. Registration,
IRAM-safe callback/LL access, cancellation and ISR-lock ordering require their
own tests. Do not call ordinary locking timer APIs while holding the DMX ISR
lock; use validated ISR-safe LL/accessors and an explicit non-nesting lock order.
Every timer callback re-checks phase/attempt under the DMX guard before touching
UART/A. A queued alarm from an old attempt cannot start another packet. Cancel
and delete must mask/clear/synchronize the timer as well as UART IRQs.

An important installed-binary fact: uart_hal_inverse_signal is currently linked
in flash (`firmware.map` at 0x42035470), and dmx_hal_inverse_signal is not IRAM
annotated. Do not call that wrapper from a cache-safe timer ISR. Use a narrowly
audited S3 inline LL operation (uart_ll.h:828-840) or an IRAM-safe equivalent.
Check the entire timer/FIFO callback call graph and data placement in the ELF.

Ordinary esp_timer callbacks are not an equivalent substitute: this build does
not enable ESP_TIMER_ISR dispatch, and its documented short one-shot behavior
does not provide a 12 us MAB edge. The matching
[esp_timer guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/esp_timer.html)
describes task-dispatch latency and approximately 20 us minimum one-shot timing.
The [legacy hardware timer guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/peripherals/timer.html)
documents its ISR context and alarm rearming. Even hardware ISR edges have latency;
verify actual break/MAB widths under load. A late alarm must never produce a mark
shorter than configured: anchor the second alarm to the actual first transition.

**Review choice:** approve this isolated timer scope to satisfy strict
nonblocking framing, or explicitly approve retaining the legacy bounded wait
for an ownership-only milestone. Do not integrate firmware until that choice,
timer reservation and IRAM feasibility are resolved. The completion/snapshot
core and its host tests can be implemented first without deciding timer hardware.

## 9. Application Bridge and Proposed Interfaces

These are proposed signatures/contracts, not headers added by this review.

Driver extension (C-compatible; names finalized in implementation):

```c
esp_err_t dmx_tx_start(dmx_port_t port, uint16_t slots, uint32_t *attempt_id);
esp_err_t dmx_tx_poll(dmx_port_t port, dmx_tx_status_t *status);
esp_err_t dmx_tx_cancel(dmx_port_t port, uint32_t attempt_id);
```

start snapshots buffer 0 into A and accepts at most one attempt; BUSY cannot
overwrite A. poll advances at most one bounded service turn and reports mode,
state, attempt, queued/requested, first fault, terminal result and can_start.
cancel requests retirement; it does not assert immediate quiescence. Cancellation
of an already completed or different attempt cannot cancel a newer one.
Use dmx_write_packet/slot as guarded staging operations; legacy send delegates
to the same start path. Do not leave a second unqualified send entry point.
Expose a version/capability macro so the adapter fails compilation against
unpatched upstream sources rather than silently reverting to the old contract.

Application API:

```cpp
class DmxTransport {
 public:
    StageResult stage(const uint8_t* packet, size_t size);
    void requestSender(bool enabled);
    void poll(uint64_t now_us);       // maintenance/retirement, never auto-start
    ServiceResult serviceTx(uint64_t now_us); // at most one start, no waits
    const Health& health() const;
};
```

stage validates/copies exactly 513 bytes with start code zero into P and advances
an output-request generation, even for successful identical preset LOAD. It does
not change Controller state or driver A and does not claim wire delivery.
Rejected invalid input leaves the preceding P intact.

At the existing normal/wake TX opportunities, serviceTx polls status; if allowed,
writes the latest complete P to S and starts its snapshot. Mark a generation
in-flight only on accepted start; mark completed only on COMPLETE for its exact
attempt. Keep separate requested/published/started/completed generations, using
a started observation when the first FIFO fill actually occurs (asynchronous
framing acceptance is not the same event). Clear pending only if its generation
still equals the completed generation. Preserve newer updates on any error.
Repeat the last complete state when no update is pending; no idle-only cessation.

CommandDispatcher remains FIFO. Its publisher callback calls stage, not the
driver. An immediate zero result means staged/request accepted, not hardware
publication. Deferred driver errors live in bounded Health; do not rewrite
Controller revisions or issue per-frame Serial output. SAVE/COPY/CLEAR retain
their current no-output publication behavior; domain/NVS errors stay distinct.

Normal view.loop and serviceDmxDuringWakeTouch use the same serviceTx path.
Maintain those scheduling opportunities rather than adding a new periodic sender.
One maintenance poll/mode gate before global early returns handles cancellation
and deadlines, but does not add an extra frame-start opportunity. All Settings,
Precision/keypad, Preset and consumed-wake branches retain service; backlight
sleep is not a request to stop output. Do not alter page/canvas or touch ownership.

Only the app task calls these methods; callbacks from future Wi-Fi tasks must
submit commands, never call stage/start or mutate Controller directly. No network
task is introduced by this design.

## 10. Required Execution Timelines

Notation: P = latest app packet; S = driver staging; A = immutable active snapshot.
Ownership transfer below means permission to overwrite A, not merely an API return.

### 1. Normal 513-byte packet

```text
READY: app writes S=A0 -> PREPARING: task copies S->A
start id7: A fixed; initial <=128 slots; FEEDING refills only A
final refill: queued=513, no prior raw done; refill disabled -> DRAINING
fresh done hint + FIFO/FSM idle + no fault -> COMPLETE(id7)
task may now replace A for id8; queued bytes were copied from one fixed packet
```

Current normal path often behaves this way electrically, but lacks the enforced
immutable source and full completion conditions. Proposed ownership is structural.

### 2. Multiple controller updates during TX

```text
id7 owns A=A0
commands A1, A2, A3 execute FIFO; P becomes A3 (and S may stage independently)
no mutation of A; id7 completes or fails
after quiescence, snapshot latest P=A3 for the next attempt
```

Intermediate domain commands are not lost; intermediate wire states are sampled
out deliberately. Preserve the final released slider value even with no further
touch events. A new P arriving during id7 result handling must not be cleared by
an acknowledgment for an older generation.

### 3. FIFO underrun before all slots queued

```text
id7 A immutable; queued=128; FIFO drains during delayed ISR service
raw done + FIFO-low pending
before any refill: raw done seen with 385 slots unqueued
UNDERRUN -> RETIRING; no resumed tail; A still frozen
FIFO/FSM/timer quiescent -> failed terminal id7; never COMPLETE
fresh break/MAB -> retry newest P, with a new attempt ID
```

This rejects the current refill-first/stale-done sequence. Repeat injection when
only the final slot remains and when done arrives during the last refill.
Already transmitted partial bytes are an electrical error to capture, not a
successful frame or something a retry can retroactively erase.

### 4. Notification before possible late ISR read

Current driver can give the token with refills still enabled. In the proposal,
done/notification cannot release A. A refill branch must acquire the guard and
see FEEDING before reading. Finalization requires refill disabled and ISR/timer
access retired under that same guard. A previously captured status or delayed
notification is not permission to read an old pointer after retirement.
If those conditions are unavailable, poll stays pending/faulted and A is retained.

### 5. Timeout then new send

```text
id7 exceeds deadline -> TIMEOUT latched; RETIRING, A held
new commands update P; start(id8) returns BUSY, never resets id7 progress
if quiescence verified: failed id7 terminal -> backoff -> id8 newest snapshot
if quiescence not verified: FAULT_STOPPED, A held, app responsive; no id8
```

Completion is checked before deadline classification. A late done for timed-out
id7 cannot turn it into success or finish id8. A safe new epoch requires old
hardware sources retired; IDs alone cannot tag a stale physical IRQ.

### 6. Blackout / preset during TX

id7 retains the previous A. Successful preset LOAD stages one whole P; blackout
ON stages the Controller's all-zero effective packet. Neither writes A. The next
accepted snapshot is a complete state, never old prefix/new preset/zero tail.
OFF stages the Controller's saved entry state; edits/loads during blackout are
discarded on release exactly as today. No NVS changes or fixture-state inference.

Latest-state sampling means ON then OFF before a boundary may send no complete
zero frame; consecutive LOADs may send only the last preset. This is not a new
guarantee per command: Stage 2 also cannot guarantee full frames for rapid changes.
The policy must be explicit in approval/tests. A guaranteed blackout pulse requires
a separately approved barrier, not an unbounded queue or silent policy change.

### 7. Shutdown during TX

Current helper writes S zeros and waits/delays, but starts no zero frame (R19).
With immutable A it cannot damage the already active snapshot, but it still
does **not** guarantee a zero frame before power action. Do not label it fixed.

A separate Stage 3B power-action state machine can: finish current attempt, reserve
shutdown output, start and confirm three complete zero snapshots, then execute
restart/powerOff. Confirmed shutdown rejects further output commands; Controller
data is not cleared. Count actual COMPLETE attempt IDs, never polls/tokens.
If zero output fails, use an approved bounded failure policy; recommendation is
cancel the software power action and retain/report the fault rather than claim
blackout succeeded. This changes a failure path and needs explicit approval.

Three nominal frames cost at least about 68.28 ms plus the residual frame, service
gaps and any recovery. This is not the old helper's guarantee or timing. No shutdown
state machine is included in this design's initial implementation prompt.

## 11. Required Patch and Module Boundaries

Future files, only after approval:

| File | Purpose |
| --- | --- |
| `patches/esp_dmx_v202/tx_ownership.patch` | Reviewed finite diff against the exact source version; changes the relevant driver/header/HAL/ISR paths. |
| `patches/esp_dmx_v202/tx_contract.h` | Canonical C-compatible bounded transition core, shared verbatim by patched driver and host tests; no Arduino dependency. |
| `scripts/patch_esp_dmx_tx_ownership.py` | Separate deterministic patch/install hook, including the canonical helper and capability marker. |
| `src/dmx_transport.h/.cpp` | Single-owner P, generations, retry/health and command-publication adapter. |
| `src/dmx_transport_esp_dmx.h/.cpp` | Small installed-version adapter; no shadow copy of private vendor structs in the app. |
| `tests/transport/` | FIFO/ISR simulation, actual-core/actual-ISR adapter tests and integration/source guards. |
| `src/src.cpp`, `src/view_sender.h` | Replace setup/normal/wake publication/start call sites only; preserve unrelated UI/domain behavior. |
| `platformio.ini` | Add the new pre-hook/capability build flag when integration is approved; keep all pins, dependencies, USB and timing configuration. |

Driver changes must cover initial fill, every refill, removal of the TX_DONE copy,
write/slot staging reservation, guarded start, raw-status handling, terminal poll,
timeout/cancel lifecycle, semaphore compatibility, mode transitions and deletion.
RX packet parsing remains unchanged; lifecycle fixes must be isolated from it.
Scope the new contract to the tested ESP32-S3 configuration; other legacy boards
must not silently compile a partially supported safety path.

Keep the existing three-getter S3 patch separate and unchanged. Apply both hooks
after dependency availability, before compilation. Verify expected source hashes
with explicitly specified newline normalization, unique anchors and complete
before/after state for every affected file. Validate the entire patch set before
writing any file. Already fully patched succeeds; missing source, unexpected
version, partial/mixed old/new state, duplicate anchors or altered helper fails
clearly. A failed multi-file write must be detected on the next invocation, not
silently treated as complete. No live application uses files being patched during
the build; fresh `.pio` and repeat-build tests are mandatory.

Installed-library edits without repository patch source are not an acceptable
solution. Do not run the existing patch script merely to inspect it; this review
has not changed any installed file.

## 12. Memory, CPU, Flash and Scheduling Estimate

| Resource | Proposed incremental cost, not measured |
| --- | --- |
| Driver packet storage | 0 bytes: reuse buffer 1; retain current two-buffer allocation. |
| DmxTransport | 513-byte P plus alignment, references, generations/status/deadlines; target <=768 bytes object storage. |
| Driver contract | Roughly 64-128 bytes per active port for state, attempt/result, flags, timestamps and counters; reuse existing queued/send-size fields where safe. |
| Strict framing | One reserved hardware timer, small fixed callback state and SDK initialization allocation; provisionally budget <=512 additional bytes, then measure. No per-frame allocation or new task stack. |
| Total internal RAM | Target <=1.5 KiB added for this one-port application, excluding compiler/linker variation; measure sizeof and actual heap deltas. |
| Flash / IRAM | Planning allowance roughly 4-12 KiB flash plus 0.5-2 KiB IRAM for core, adapter and optional framer. Estimates, not build results or fixed upper bounds. |
| Copies | Move existing per-frame 513-byte ISR copy to task-side snapshot creation. App P adds one bounded copy per output request; no heap packet queue. |

The temporary initial/long-gap timer is not active per slot or every regular frame.
No periodic polling loop is introduced inside one call. Continuous publication
still follows application service opportunities, so rendering/NVS can extend
inter-frame gaps. Waiting for full idle and snapshot preparation can add gaps;
do not promise the same maximum packet rate without capture. Preserve configured
250000 baud, 8N2, break 44 bits / MAB 3 bits (nominal 176/12 us), FIFO threshold
8 and Unit TX2/RX1/EN-1 or Base TX7/RX10/EN6. Do not tune those to mask failures.

Normal nominal payload is 22572 us; including configured framing is 22760 us
(about 43.94 Hz before application gaps). These are calculations, not measured
compliance. No new PSRAM allocation, filesystem or partition change.

## 13. Automated Acceptance Plan

Host tests must model FIFO contents, a separate shifter/FSM, raw sticky bits,
mask bits, timer phases and deliberate ISR starvation. Hardware time advances
independently of task/ISR calls. Never make completion synonymous with the fake
driver deciding to return ESP_OK.

Share the production transition core with tests. In addition, compile the actual
patched ISR header in a host shim with fake HAL/queue/semaphore/critical primitives,
or equivalent verified adapter coverage. Pure translated-model tests alone do
not prove the source hook uses the core correctly. Test patch preimages and real
installed ISR/start pointer wiring separately; build all four target environments.

| Test | Required assertion |
| --- | --- |
| Install / first start | READY is not completed; exactly one complete snapshot, 513 bytes/start code 0; correct initial framing gate. |
| Continuous frames | Repeat unchanged states; one active attempt, no duplicate completion or overwritten active packet. |
| Rapid and multi-channel edits | P/S can change while A remains bit-for-bit fixed; latest final value appears after service resumes. |
| Preset / blackout | Whole known A/B/zero patterns; Controller restore semantics unchanged; explicit short ON/OFF sampling policy. |
| Busy and rejection | Start during PREPARING/FEEDING/DRAINING/RETIRING cannot copy, consume pending or reset counters. |
| Early underrun | Drain first 128 slots with 385 unqueued; assert raw done+FIFO-low; no refill after fault, no success. |
| Every tail length | Inject underrun with each feasible remaining count, especially 1 and <=FIFO capacity; no final-refill stale-done false success. |
| In-refill events | Inject raw done before read, during FIFO write, after write, before DRAINING and before/after done enable. Qualify or conservatively fail, never clear an old event into success. |
| Late ISR | Pause after ISR captured status; attempt cancel/poll/new start; resumed handler cannot reference a retired/reused packet. |
| False idle / shifter busy | Empty FIFO with active shifter or forced TX inversion is not quiescence/COMPLETE. |
| Timeout | Busy beyond deadline -> TIMEOUT, not completion. Packet finishing before a delayed poll is checked first. |
| Retirement | No A reuse until both IRQ sources and UART access retired; stuck FSM retains A and faults without loop blocking. |
| Retry | Fresh framing, new attempt, newest P; stale old IRQ/result cannot complete new attempt; bounded backoff/retry budget. |
| Missed done IRQ | Raw polling recovers a qualified event, without fabricating completion from a generic token. |
| Delayed observation | A completed frame first observed after a long service/ISR delay cannot reset wire-gap age to the poll/ISR time; the next start receives conservative fresh framing. |
| Mode changes | Both RX indices; no TX in established READ, no RX write during A ownership, no direction switch mid-active frame. |
| Timer, if approved | Delayed alarms, minimum actual-edge-relative MAB, cache-disabled-safe call graph, stale alarm, cancellation in each phase, resource failure/conflict. |
| Lifecycle | Reject delete while referenced; no timer callback/ISR after free, no unbounded queue, no steady-state allocation. |
| Errors / bounds | Invalid port/mode/length/null/start code, failed staging/zero FIFO write/collision; preceding P survives; counters never exceed requested. |
| Sequence wrap | Attempt/generation wrap at the defined boundary, repeated polling, no duplicate terminal reporting. |
| Application integration | Same normal/wake service path, maintenance before returns, no remaining active-buffer writers or direct unchecked sends. |

Retain existing Controller and Dispatcher suites and protect UI/preset/battery/
canvas/USB configuration with source-integration guards. Run `git diff --check`.
Patch tests must cover fresh/fully patched/mixed/unknown inputs for both hooks.
Measure object sizes and allocation counts in tests; link-map check ISR functions
and all constants/data they use. Update guards only for explicitly intended TX
call-site changes, never remove unrelated regression assertions to obtain a pass.

No new host suite was implemented or run by this design-only task. Previous
baseline test/build passes are not validation of this proposed state machine.

## 14. Separate Real-Hardware Acceptance

1. Capture unchanged Stage 2 on the known stable Unit first, with exact source,
   environment and binary hash. Preserve user NVS/settings and fixture safety.
2. Primary candidate is **normal cores3se_unit**, not diagnostics. The USB report
   records diagnostic stub failure and normal 115200/1500000 flash_id passes;
   repeated actual automatic uploads and monitor/reboot validation remain pending.
3. Use a safe RS-485 receiver/differential capture arrangement with sufficient
   time resolution, not a raw logic probe on differential bus wires. Record
   break, MAB, baud/stop bits, zero start code, packet length and frame gaps.
4. Compare known full-universe A/B patterns across at least 1000 controlled
   preset/state transitions. Each successful complete frame must match one full
   allowed state, not a cross-generation prefix/tail. Sequential individual edits
   legitimately create intermediate logical states; do not misclassify those.
5. Run idle, rapid sliders, Precision holds/keypad, page drawing, preset save/load,
   blackout entry/release, backlight sleep and consumed long wake gestures. Check
   touch response, one canvas, speaker/MUTE and battery warning restoration.
6. In a dedicated non-release fault-injection build, deliberately withhold refill
   service while the UART drains. Do not disable the CPU/entire system for a long
   interval. Capture partial attempt, error result, quiescence and fresh retry.
   Repeat near the final refill and during timer phases. Record fault injection
   scope and binary hash; never ship it enabled.
7. Correlate aggregate software attempt/result/queued/error records with the
   external capture. A terminal COMPLETE must correspond to a complete clean
   captured packet; abnormal attempts must never be counted as COMPLETE. Software
   records alone cannot establish wire-frame coherence or receiver acceptance.
8. Define allowable worst-case inter-frame gap and command-to-wire latency from
   measured Stage 2 workloads before calling the candidate stable. No numeric
   pass budget is inferred from heap logs. Check restart-framing after >1 s gap.
9. Prove RX/TX direction and timer reservation/cache-disabled behavior separately.
   If strict framing is selected, validate initial and recovery edges as well as
   UART-generated steady frames under NVS/display/speaker load.
10. Run Base as a separate controlled matrix after Unit passes. Existing Base
    glitches are not proof of this driver condition and are not declared solved.

Shutdown zero-frame capture is a separate Stage 3B acceptance gate. Do not use
legacy shutdown as evidence that the normal transport contract sends three zeros.

## 15. Incremental Implementation and Rollback

Only after design approval:

1. Implement the shared state core, raw-event/idle HAL abstraction and deliberate
   fault simulation first. Exercise stale final-refill events before app changes.
2. Add the snapshot/completion patch and actual-ISR tests behind an explicit S3
   capability/opt-in flag. Verify deterministic patching from a separate clean
   build directory; preserve current ignored artifacts/logs. No device upload.
3. Resolve section 8's framing choice. If strict: validate timer reservation,
   cache-safe helpers and cancellation, then add the separately tested one-shot
   framer. Stop on resource/SDK/ordering ambiguity rather than choosing a delay.
4. Integrate DmxTransport P/generations and replace setup/normal/wake publication
   and starts. Preserve domain/UI/driver configuration. Re-run all host suites,
   integration guards, all four builds and whitespace checks; report size deltas.
5. User hardware-validates normal Unit and external captures. No automatic flash,
   commit or release. Resolve any measured cadence/latency regression first.
6. Only after an explicit separate request, implement shutdown Stage 3B and its
   bounded zero-frame sequence. No power-action redesign hidden in steps 1-4.

Rollback baseline is the existing Stage 2 commit/branch, not a reset of this
working tree. Retain exact good binaries plus hashes and do not overwrite old
release artifacts. If a candidate fails, user may flash a verified Stage 2
app-only binary using the previously verified layout, preserving NVS; never
erase or blindly reflash a FULL image. Build rollback sources in a separate
checkout/output directory. Do not pop/delete the stash, discard documents/logs,
force checkout, rewrite Stage 1/2 or modify published tags. A patch-capability
flag must fail closed, not silently fall back to old semantics in a mixed build.

## 16. Review Decisions and Concrete Recommendation

Approve the **existing-buffer-1 immutable snapshot plus qualified lifecycle**
design, not a semaphore-only bridge. It separates memory safety from successful
delivery, handles underrun as abnormal termination and keeps the latest pending
Controller packet without a queue of obsolete frames.

Before firmware integration, explicitly approve:

- Strict hardware-timer initial/recovery framing versus a documented exception
  retaining the legacy bounded wait. Recommend the timer only if strict no-wait
  is required; no unnecessary timer redesign of continuous DMX.
- Latest-state-per-boundary semantics, including rapid blackout ON/OFF sampling.
- Proposed deadlines/retry limits and fail-stopped recovery policy, subject to
  baseline measurement. Failure must not silently imply successful DMX delivery.

The smallest maintainable path is three isolated milestones: tested driver
snapshot/completion core; framing requirement resolution/validation; application
bridge. Shutdown remains Stage 3B. Do not proceed until this review is approved.

## 17. Implementation-Ready Codex Prompt (For After Approval)

> Continue on codex/stage3-dmx-transport from the preserved Stage 2 baseline and
> current uncommitted documentation. Read STAGE3_BUFFER_OWNERSHIP_DESIGN.md and
> the completed transport audit. Implement only the approved Stage 3A milestones;
> no Wi-Fi, source-domain/UI/NVS/USB changes, release, upload or commit.
>
> First confirm the explicit review decisions in section 16, particularly strict
> framing versus the legacy bounded-wait exception. If they are not resolved,
> implement only the isolated driver contract/core host-test milestone and stop
> before firmware integration. Never silently choose a timer or weaken a guarantee.
>
> Use upstream esp_dmx v2.02 with the existing three-getter S3 patch unchanged.
> Add a separate deterministic repository patch/helper hook. Reuse buffer 0 for
> staging and buffer 1 for the immutable TX snapshot; snapshot before start, use
> buffer 1 for initial/all ISR FIFO reads, remove the TX_DONE snapshot overwrite.
> Implement guarded state/attempt/result lifecycle, raw done checks before and
> after refill, conservative failure for ambiguous final-refill events, disabled
> future refill plus qualified full drain before normal completion. No mutation
> or reuse of active storage on timeout; retire/fault-stop safely before retry.
>
> Test the canonical production state core and actual patched ISR adapter with
> fake FIFO/shifter/raw/mask/timer primitives. Deliberately trigger early/final
> underrun, stale done, in-refill events, late ISR, timeout, cancellation, mode
> changes and stuck quiescence. A slot-count-only fix and an idealized fake done
> flag are not acceptable. Keep task/timer/ISR locks short and cache-safe.
>
> If strict framing was approved, separately establish the timer reservation and
> IRAM-safe S3 call graph using installed IDF 4.4 APIs, then implement the fixed
> initial/recovery break/MAB phases without task busy-waits. Preserve all UART
> durations/pins and keep steady frames UART-generated. Stop if MMIO ordering,
> timer ownership or ISR/cache safety cannot be established. No esp_timer task
> callback or loop-delay substitution for 12 us MAB; no unsupported newer APIs.
>
> Once the driver/framing gates pass, add one app-task DmxTransport with a fixed
> complete 513-byte pending packet, latest-state generations, structured deferred
> health/results and bounded retries. Route Dispatcher publication into staging;
> use the same checked transport path for normal and consumed-wake starts, with
> mode/maintenance handling before early returns. Preserve Controller, command
> FIFO, start code, every touchscreen function, preset format, blackout restore,
> shared canvas, power, battery and speaker behavior. No background network task.
>
> Leave the legacy shutdown helper unchanged and explicitly report its missing
> zero-send guarantee; immutable snapshot ownership must still make its staging
> writes safe. Do not implement Stage 3B without separate authorization.
>
> Run Controller, Dispatcher, new fault-injection transport/ISR tests, updated
> scoped integration guards, all four normal/diagnostic builds, deterministic
> fresh/repeated patch tests and git diff --check. Report exact changes, API,
> ownership proof/limits, all results and measured RAM/flash/IRAM differences.
> Provide the normal Unit upload command for user execution only and the separate
> RS-485 capture checklist. Do not claim wire integrity from host tests. Preserve
> historical files, logs, stash, all uncommitted reports and Stage 1/2 history.
