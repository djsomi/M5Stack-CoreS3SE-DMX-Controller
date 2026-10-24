# Stage 3 Transport Safety Gate

Status: implementation stopped at the requested source-verification gate.
Baseline remains `4c973007d74a1f24319e0e375a56107bc261b784` on
`codex/stage3-dmx-transport`. No transport module or firmware change was made.

The requested bridge is a useful improvement for ordinary, non-underrunning
transmission. The earlier audit explicitly qualified that recommendation by the
driver's completion assumptions. It is not sufficient to establish the stronger
requirement that a driver-buffer write can never overlap future ISR reads when
the actual v2.02 completion implementation is considered.

## Exact Installed Source Rechecked

Relevant paths below are under `.pio/libdeps/cores3se_unit/esp_dmx/src/`:

| Source | Observation |
| --- | --- |
| `esp_dmx.c:701-721` | dmx_write_packet validates WRITE mode, then memcpy writes buffer[0] with no completion protection. |
| `esp_dmx.c:743-756` | dmx_send_packet takes the binary completion token with zero wait. |
| `esp_dmx.c:800-812` | Initial FIFO fill reads buffer[buf_idx], sets slot_idx, then enables all TX interrupts. |
| `esp_dmx.c:817-829` | dmx_wait_send_done only takes/gives that token; no progress or FIFO check. |
| `impl/intr_handlers.h:48-65` | TXFIFO_EMPTY reads buffer[0], increments slot_idx, and disables that refill interrupt only when slot_idx == send_size. |
| `impl/intr_handlers.h:66-74` | TX_DONE copies buffer[0] to buffer[1] then gives the token, unconditionally with respect to slot_idx/send_size. It does not disable pending/future refill interrupts. |
| `esp_dmx.c:173-175,297-301` | Installation and entry to WRITE give a token before any frame completes. Initial availability is not previous-frame completion. |
| `impl/driver.h:30-37,52` | slot_idx, send_size, buf_idx and driver object table are private implementation state; not accessible through a public progress API. |
| `esp_dmx.c:771-798` | Initial/long-gap send generates break/MAB with ets_delay_us. Calling send from a cooperative bridge does not make this pre-existing library path delay-free. |

SHA256 values still match the completed audit:

```text
esp_dmx.c
9ddc50e211e768ff1c1612fc803e973071b22e6b18c60ebae9800cd288e497b4
impl/intr_handlers.h
f841719683ff5c8aaf24c07a650c6746feb5a5ef52a2dc7d78df187117adbd43
impl/dmx_ll.h (existing three-expression S3 patch)
e2c294fd91101ce2ac3b18e73e77ffcdb8e2b2c20c3579a8e1857f8598e3487c
scripts/patch_esp_dmx_v202.py (repository file)
c47110e3c1cc082a810676438cc55e6e48c9fc02dfa50731d4a3480b425e6ed1
```

The installed SDK `tools/sdk/esp32s3/include/soc/esp32s3/include/soc/uart_struct.h`
lines 156-160 describe TX_DONE in terms of transmitting **FIFO data**, not all
513 bytes remaining in software. Lines 91-95 describe the independent FIFO-low
interrupt. The hardware FIFO holds 128 bytes, so it cannot hold a whole universe.
The current low-water threshold is eight bytes. There is no established bound
on worst-case ISR latency in the supplied hardware results.

## Counterexample to Semaphore-Only Ownership

This is an allowed source-level interleaving conditional on a FIFO underrun,
not a claim that an underrun or corrupt frame has been measured on the Unit:

1. A 513-byte send puts its first 128 bytes in the FIFO. slot_idx is 128.
2. ISR servicing is delayed until the FIFO drains. Both FIFO-low and TX_DONE
   can now be pending. The latter records the drained FIFO, not the unqueued tail.
3. The ISR prioritizes FIFO-low. It fills at most another 128 bytes and advances
   slot_idx to 256. Refill interrupts remain enabled because 256 != 513.
4. The next ISR iteration can handle the previously latched TX_DONE after the
   FIFO-low condition is cleared. It gives the token without a full-progress test.
5. Application wait_send_done(port, 0) can return ESP_OK, although another 257
   bytes still need future ISR reads. Publishing packet B now can replace packet
   A's remaining source data. Starting another send also reuses/reset progress.

Single-task ownership prevents another application caller from stealing the
window; it does not prevent this ISR-generated early token. Moving to a separate
pending packet only isolates application staging. It does not by itself make the
subsequent memcpy into the driver's shared TX buffer safe in this interleaving.

Whether this sequence occurs on physical hardware remains unmeasured. It is
enough to invalidate an unconditional safety proof based on that semaphore alone.
It is not evidence that the experimental Base glitches share this cause.

## Why Available Substitutes Are Not a Verified Fix

- Public dmx_get_mode checks lifecycle, not full-packet progress.
- Public dmx_read_packet/slot and the second driver buffer do not expose a
  coherent completed-generation/progress handshake.
- Polling again can observe the same binary token; it does not certify a later
  completion. Delaying one nominal frame is not a correctness proof.
- UART physical idle alone does not prove that all software slots were queued;
  it is precisely possible to be idle during an underrun. A stronger combined
  hardware/ISR-progress handshake would need its own race analysis and tests.
- A task mutex or interrupt protection of memcpy alone does not repair an old
  FIFO prefix plus a new software tail, nor a token that was emitted too soon.
- The stock UART driver is not installed on this port. Its wait/queue APIs are
  not a replacement for esp_dmx's private progress tracking. Espressif explicitly
  warns about high-level APIs with custom ISR handling in the
  [IDF 4.4.7 UART guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/peripherals/uart.html#using-interrupts).
- Forcing a mode change, clearing FIFO state, stopping/reinstalling the driver,
  or masking interrupts to manufacture a window would change transport behavior.
  None was implemented.

The audit's RX-to-TX buf_idx mismatch also remains: RX toggles the index, WRITE
does not reset it, initial fill uses that index and later refills use buffer[0].
The normal cold-start Sender path starts with index zero; no new Receiver path
or workaround was introduced.

## Decision and Next Required Approval

The instruction says to stop if a safe publication boundary cannot be established
and prohibits library/compatibility-patch changes. Under those constraints, do
not implement a semaphore-only DmxTransport and label it unconditionally safe.

Recommended next step is a separately authorized **driver-completion/snapshot
design and test step**, retaining the upstream version and deterministic patch
mechanism if a patch is eventually approved. It must account for FIFO underrun,
latched stale TX_DONE after a refill (including the final refill), actual ISR
source-buffer ownership, startup, errors and mode transitions. Merely adding a
slot_idx == send_size test should not be assumed sufficient: all slots can be
queued in the final refill while an older TX_DONE indication is still pending.
No specific driver patch has been implemented or claimed verified here.

Alternatively, the user could explicitly approve a best-effort normal-path bridge
with the no-underrun assumption and its limitations. That weakens the current
primary guarantee and is not taken as implicit permission by this implementation
request. External capture of several clean runs would not prove that the source
interleaving is impossible under future load, especially Wi-Fi.

After resolving the completion contract, the intended application bridge remains:
one fixed 513-byte pending packet; latest-state coalescing; zero-wait completion
checks; complete safe publication; checked send; explicit requested/published/
started/completed generations and deferred errors; shared normal/wake service;
mode gates; no task, timer, dynamic queue or per-frame logging. No new public API
was introduced in this stopped step. Controller and Dispatcher remain unchanged.

## Shutdown Remains a Separate Unresolved Path

`src/view_sender.h:1626-1653` writes zero, waits and delays three times, but never
calls dmx_send_packet. It can overwrite an active frame's tail and does not
guarantee a full zero frame before restart/power-off. Waiting alone never starts
one. This function and its callers are untouched; the controller cannot currently
be described as safely sending three complete zero frames before shutdown.

A future separately scoped correction needs the trustworthy completion contract,
then a nonblocking sequence that finishes in-flight output, starts and confirms
each agreed zero frame, and only then takes the power action. A bounded deadline,
failure policy and added shutdown latency require explicit treatment and tests.
No shutdown redesign was silently bundled into this step.

## Verification and Resource Impact

No application code changed: RAM/flash impact relative to the current local
Stage 2 baseline is zero. No new transport fake-driver suite was added because
there is no transport implementation to test. A fake that always asserts a
trustworthy completion would conceal rather than resolve this gate.

Existing Controller tests: six groups PASS. Existing Dispatcher tests: six groups
PASS. Existing source-integration guards: two groups PASS. `git diff --check`
passed. All four unchanged environments built successfully:

| Environment | Static RAM bytes | Flash use bytes | Result |
| --- | ---: | ---: | --- |
| cores3se_unit | 32932 | 604117 | PASS |
| cores3se_base | 32932 | 604065 | PASS |
| cores3se_unit_diagnostics | 35548 | 606361 | PASS |
| cores3se_base_diagnostics | 35548 | 606345 | PASS |

These are baseline build results, not validation of a Stage 3 implementation.
Diagnostic USB behavior and upload_speed remain unchanged. The user-supplied USB
A/B results are recorded in USB_UPLOAD_REGRESSION.md; normal Unit is the preferred
future hardware target. No automatic upload or device operation was performed.

Requested normal Unit command, for user execution only:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

That command currently flashes unchanged Stage 2 firmware, **not a Stage 3 fix**.
There is no need to flash it merely because this gate report was added.

## Hardware Checklist Once a Candidate Is Authorized

- Normal Unit: repeat actual automatic uploads three times with app boot between
  them; verify monitor/reboot operation separately from flash_id tests.
- Exercise channel grid, selection/scrolling, both sliders, Precision STEP and
  accelerated/constant holds, keypad clamping and final released slider value.
- Test P1-P8 save/load/copy/clear without erasing user data; test multi-channel
  transitions and exact blackout snapshot restoration, including edits during it.
- Check Settings, shared canvas, sleep/consumed-wake gestures, sound/MUTE,
  high-volume battery warnings/restoration and both power configurations.
- Confirm no output in Receiver mode, and separately test any enabled RX-to-TX
  transition rather than assuming the known private index issue is absent.
- Test delayed ISR service/early completion recovery, copy/start errors, repeated
  unchanged output, coalescing and explicit shutdown-zero policy in software.
- Externally capture RS-485 start code, 512 channel slots, complete known A/B
  packet patterns, break/MAB, gaps and command-to-wire latency under load. Include
  induced delay/underrun testing where safe. Host tests cannot certify the wire.
- Keep stable Unit evidence separate from experimental Base fixture glitches.

No commit, push, merge, tag or release. Existing baselines, stash, logs and
historical work remain intact on the Stage 3 branch.
