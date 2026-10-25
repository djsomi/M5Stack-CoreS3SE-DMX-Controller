# Stage 6A: seven-update disconnect investigation

## Evidence and conclusion

The user observed two independent browser connections each delivering seven
state.update messages before abnormal closure. The second connection started
at revision 261, delivered 262..268, lasted about 17 seconds and closed with
browser code 1006. HTTP /status then returned 200; the Wi-Fi station count was 1.
These are user hardware observations, not new device access by the agent.

**A reproducible source defect was found in the outbound pump:** it removed
only one event per client per 20 ms callback. Its nominal service ceiling was
50 events/s/client, lower than a possible sequence of touchscreen changes.
State events and command results share each eight-slot outbound FIFO. Once
full, the existing safety policy invalidates that client and closes its TCP
session. HTTP and the SoftAP can remain operational throughout.

A deterministic test of the original policy reproduces exactly seven delivered
updates: touchscreen produces one actual value change every 10 ms and the
network consumes one event every 20 ms, offset by 10 ms. On the 16th mutation,
seven have been drained and eight remain queued; enqueue fails. The connection
is invalidated at 150 ms, with **zero inbound requests/outstanding credits**.
With another phase, the original policy delivers six before overflowing. A
slow producer passes 20 updates without closing. Seven is not a protocol limit.

This confirms an under-draining defect and a mechanism consistent with the
reported count. **It does not yet prove that the two hardware disconnects were
caused by overflow.** The earlier firmware did not retain the necessary close
reason. A 17-second connection could include waiting followed by a short drag;
gesture timestamps and total Controller revisions were not recorded. Send
timeouts, native closures, validation errors and association churn must remain
separate hypotheses until the new counters are collected.

If the user's initial revision 261 is followed by this exact synthetic pattern,
the expected signature is:

| Observation | Predicted value |
| --- | --- |
| Browser/native last successful update | 268 |
| Last update successfully enqueued | 276 |
| Mutation whose enqueue failed | 277 |
| FIFO depth at failed enqueue | 8 |
| Outstanding client requests | 0 |
| Bridge stop reason | queue_full |
| TCP closure initiator | HTTP pump observing the invalidated generation |
| WebSocket CLOSE frame required by that path | None |

This prediction assumes only those channel mutations affect revision and no
other messages occupy the FIFO. Compare actual diagnostics rather than force
the hardware evidence to fit these numbers.

## Source review

- `include/websocket_bridge.h`: outbound capacity remains 8/client, ingress 16,
  four WS clients and four outstanding inbound requests/client.
- `Bridge::push` writes `(head + count) % 8`; `take` copies an owned Event,
  advances `head % 8` and decrements count. Tests exercise repeated wraparound.
  No off-by-one or missing release was found. Slots are released at dequeue,
  before sending; the HTTP task owns a separate copied Event while transmitting.
- `max_outstanding=4` means network-admitted client requests awaiting owner-task
  snapshot/Dispatcher processing. A credit returns when the owner queues the
  snapshot/result, not when a browser acknowledges or receives it. There is no
  browser ACK message. Touch/system updates do not consume these credits.
- Original `sweep` sent at most one Event/client, then waited for the next
  worker-posted callback. That couples a small FIFO to a 50 Hz drain ceiling.
  The previous host tests drained directly and did not model that scheduler.
- `wsSend` uses the confirmed IDF 4.4.7 low-level send function synchronously
  on the HTTP task. Its buffer is valid through the whole call; no pointer is
  retained in another task. The existing exact-write loop and 100 ms absolute
  frame deadline remain. A native ESP_OK means socket submission succeeded,
  not proof of browser delivery or of a DMX frame reaching a fixture.
- `httpd_queue_work` is still called only by the network worker. The callback
  executes on the HTTP task; at most one pump callback is pending/active.
- Six HTTP sessions, LRU purge disabled and the SDK's three reserved sockets
  are unchanged. Four Wi-Fi stations are not four WS sockets. Native close_fn
  exposes an fd, not an eviction/error reason; unexplained closes are counted
  as unattributed instead of labelled eviction. HTTP availability alone does
  not establish that an individual WS session survived.
- The handler deliberately returns failure on malformed/fragmented/binary or
  oversized input, rate violations, invalid sessions and peer CLOSE. Native
  errors cause cleanup too. Each application-observable termination path is
  now classified. Browser code 1006 is local abnormal-closure reporting, not
  evidence that the server transmitted a CLOSE frame with that code.

The installed native implementation and previously inspected exact v4.4.7
sources retain their original behavior. No framework/library patch was added.
The independent Stage 3E esp_dmx patch and all domain/driver sources are unchanged.

## Minimal correction

`include/websocket_pump.h` defines the actual shared/tested scheduling policy:

1. At most four rounds per invocation, one FIFO event/client/round.
2. Rotate the first socket each invocation for fairness.
3. Stop starting frames after 20 ms of drain work; an already-started frame
   retains its original 100 ms absolute I/O deadline.
4. Stop early when no client has queued data.

`sweep` performs existing validity, idle, heartbeat and HTTP expiry checks, then
uses that policy. There are at most 16 data sends with four active WS sessions
per callback, regardless of how quickly producers run. The nominal per-client
ceiling becomes 200 events/s when sends are fast; this is not a guaranteed radio
throughput or hard CPU timing claim. The drain time check can overshoot by one
bounded frame. Heartbeat/housekeeping work is separate and remains bounded by
the existing send deadline. A slow client may still be closed on real overflow
or send failure; that safety protection is intentionally retained.

No capacity increase, coalescing, overwrite, reconnect/retry policy or new
task was introduced. In particular, blackout ON, intervening edits, OFF,
restored snapshot and applied results remain FIFO-ordered. Controller and
Dispatcher ownership, admission and Publisher hooks, start code, immutable DMX
snapshots, driver completion, timing/pins, UI, sleep, power and preset formats
are untouched. The application owner never waits for socket I/O or formatting.

## Bounded diagnostics

Read **GET `/ws-diagnostics`**, on the same protected/rate-limited native HTTP
server. `/status` remains unchanged and continues to report memory, stack,
owner-gap and DMX health. The diagnostic resource reuses its existing static
4,096-byte response buffer; it does not allocate another large output buffer.
The JSON serializer is host-tested with six populated session entries and
maximum-width counters/revisions, including safe small-buffer rejection.

| Fields | Meaning |
| --- | --- |
| http_opened/http_closed/open_rejected | Native open/close callbacks and application open refusals |
| ws_opened/ws_closed/upgrade_rejected | Registered WS handshakes, actual native closes and policy refusals |
| bridge_opened/bridge_closed | Logical generation lifecycle; may close before native TCP cleanup |
| updates_enqueued/updates_taken/updates_sent | Per-recipient update copies queued, dequeued and native-send successful |
| outbox/outbox_high/outstanding | Current total depth, maximum per-client depth, current inbound credits |
| queue_full/send_failures/last_send_rc | Failed FIFO admissions, native send failures, exact latest native send return |
| close_frames_sent | Only successful native CLOSE sends, not plain TCP shutdowns |
| first / last_close | Immutable first abnormal termination and latest termination record |
| bridge_first | Immutable logical failure at producer: timestamp, generation, stop, depth, credits, last/rejected revision |
| sessions | Current HTTP/WS fds, generations, ages/activity, queued depth, credits, sent count and last sent revision |
| work_pending/queued/started/completed/failed/last_work_rc | Work posting/execution progress and exact post errors |
| controller_events/publications/last_controller_revision | Actual changed-command completions and attempted fanouts, not browser delivery |
| wifi_disconnects/last_wifi_ms | Cumulative association disconnect events and latest timestamp |
| lru_purge/native_evictions/native_unattributed_closes | false / null / observed unknown native closes |

Current SDK `wifi_event_ap_stadisconnected_t` has MAC, AID and mesh-child flag,
but **no reason field**. `wifi_reason_available=false` is explicit; no MAC,
password, SSID, preset payload or credential is logged. This catches a transient
disconnect even when the station has re-associated before a /status sample.
The public close callback provides no native eviction reason. Native eviction
count is therefore null, not an invented zero. Automatic LRU eviction is disabled
by the preserved configuration; source analysis plus native/unattributed close
counts provide the available evidence without patching the framework.

Each closure record includes timestamp, fd/generation, reason/source, exact API
rc, socket errno where observed, last successfully sent revision, triggering
revision, update count, depth and credits. Queue-full depth is retained before
the queue is discarded. Producer failure time (`bridge_first.ms`) and later
HTTP termination observation (`first.closure.ms`) can differ. An in-progress
send can finish between those points. Revisions are strings to retain 64-bit
precision in JavaScript. `last_send_rc` can become zero when another send later
succeeds; the first failure record's rc is immutable. Counter names distinguish
dequeue/socket submission from browser receipt.

`WS_FIRST_FAILURE` is emitted **once per boot** by the network worker, using
the existing ESP-IDF warning log output, outside all locks/ISRs and outside the
Controller task. It includes reason/source/rc/errno, last good and triggering
revision, count, queue depth and outstanding credits. There is no periodic
Serial logging, HWCDC initialization or reliance on Arduino Serial input.
The same marker/data remain readable as JSON if console capture misses the line.
Normal peer CLOSE does not consume the first-abnormal-failure record.

## Automated acceptance

New regression cases are integrated into the existing WS host case, preserving
the 16-case compiled suite:

- Reproduce the original seven-delivery failure with zero inbound requests;
  assert 15 enqueued, 7 taken, 8 pending, rejected revision 16 and no double close.
- Same producer with the actual new scheduling helper: four clients each receive
  1,000 ordered updates; queue depth returns to zero and queue_full remains zero.
- Twenty slower updates survive; there is no intrinsic seventh-message limit.
- Round and elapsed-time caps terminate, rotate fairness and never drain
  without a bound.
- Batched blackout ON/edit/OFF updates and command results retain exact ordering
  and restore-entry values across drain invocations.
- Actual extracted endpoint/I/O code records a simulated native send failure
  with exact rc and last-sent/trigger revisions; first failure remains immutable
  after a different later close.
- Worst-width diagnostic JSON fits 4,096 bytes; insufficient output storage fails
  safely. Existing protocol/validation/queue/ownership and transport fault tests
  remain enabled.

Validation outputs are isolated in
`dist/stage6a-disconnect-investigation-20261025/`. The old policy reproduction
and log were saved before modifying production code. Prior Stage 6A validation
logs, historical files, stashes and backups were not overwritten.

### Final software results

All **16 compiled host cases PASS**: updated WS bridge/pump and actual extracted
native-adapter cases, Controller, Dispatcher, patched transport/ISR faults,
Wi-Fi policies at 1/4/8 stations, canvas, startup history, four trace-disabled
variants, HTTP policy and generated asset API. After adding the final batched
blackout-order and maximum-width JSON cases, the relevant WS cases were rebuilt
and passed again. No unrelated validation was repeated for documentation edits.

All nine integration/asset/infrastructure/patch commands PASS, including six
infrastructure fixtures, protected-source reconstruction and fresh/idempotent/
mixed/unknown driver patch-state tests. `git diff --check` and explicit checks
of new/edited files' whitespace/newlines PASS. All four final build logs contain
no compiler warning/error. No hardware result is inferred from these tests.

| Environment | Result | Static RAM bytes | Flash bytes | RAM delta vs initial Stage 6A | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87,276 | 1,040,693 | +480 | +6,508 |
| cores3se_base | PASS | 87,276 | 1,040,661 | +480 | +6,524 |
| cores3se_unit_diagnostics | PASS | 89,892 | 1,042,821 | +480 | +6,408 |
| cores3se_base_diagnostics | PASS | 89,892 | 1,042,789 | +480 | +6,404 |

Native bridge storage is 19,864 bytes (+120); diagnostic counters/records occupy
208 bytes. The remaining static delta is session metadata/alignment. The
existing output buffers, queue capacities and task stacks are unchanged. Runtime
heap, fragmentation, CPU and stack headroom under the corrected pump remain
hardware measurements. Application binary lengths are 1,041,104 / 1,041,072 /
1,043,232 / 1,043,200 bytes in table order, not merged/full-flash release files.

All four artifact receipts match current source digest
`038d455bbb0b59c5ec8093582c7f9bf450e3e220f4faacfe284edbe877d8be97`.
Embedded asset bundle and partition image hashes remain unchanged:
`5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9`
and `6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`.

### Files changed in this investigation

- New `include/websocket_pump.h`: bounded shared scheduling helper.
- `src/network_service.cpp`: use the helper; classify closes; count sends/work/
  Wi-Fi disconnects; one-shot worker log; GET diagnostic projection.
- `include/websocket_bridge.h`, `src/websocket_bridge.cpp`: queue/publication/
  lifecycle counters and immutable first logical failure; copied client info.
- `include/websocket_service.h`, `src/websocket_service.cpp`: expose that copied
  info and provide the target monotonic failure clock.
- New `include/websocket_diagnostics.h`, `src/websocket_diagnostics.cpp`: bounded
  diagnostic records and tested JSON encoding, with no domain/socket ownership.
- `tests/network/test_websocket.cpp`, `test_websocket_adapter.cpp`,
  `check_websocket_integration.py`, `run_websocket_tests.py`: regressions and
  source-placement/size/diagnostic tests.
- New `tests/network/ws_disconnect_probe.js`: passive, bounded manual evidence
  collector without commands or reconnect.
- This report and a follow-up link in `STAGE6A_WEBSOCKET_IMPLEMENTATION.md`.

Pre-work hashes confirm other current source files and historical/unrelated
files were preserved. Index is unchanged/empty, HEAD remains
`7f09d4c3314d0444a11786aea9ba86793842c9bb`, and stash@{0} remains
`996a2f9378208540b0fd3b544e7d9908affe8e84`.

## Hardware acceptance: distinguish A, B, C and D

No upload, monitor or device access was executed automatically. Use the existing
16 MiB Unit device, tested power arrangement and a safe test fixture. Preserve
NVS/credentials/presets; do not erase or upload the experimental Base image for
this Unit investigation.

Manual upload:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

Optional console capture, opened manually before reproducing:

```powershell
pio device monitor --port COM8 --baud 115200 --filter log2file
```

Use the existing USB console path. Close the monitor before an upload. A missed
USB marker does not invalidate the retained JSON record. Save console output
under ignored logs/dist; never publish device credentials or flash backups.

For each scenario, manually reboot first to start a fresh immutable first-failure
record. Open `http://192.168.4.1/` in Windows Chrome/Edge, then DevTools/F12 and
paste `tests/network/ws_disconnect_probe.js`. This probe sends **no commands**,
does not reconnect and retains only 128 recent update records. Wait for hello
and the complete snapshot. Run `await dmxProbe.sample()` to save /status plus
/ws-diagnostics, and `copy(JSON.stringify(dmxProbe.result(),null,2))` to export
browser evidence. On close it fetches both resources once automatically.

| Hypothesis | Controlled test and needed evidence |
| --- | --- |
| A: idle survives | No touch or web commands for at least 65 s, preferably 120 s. Record initial and final diagnostics. Browser should auto-PONG server PINGs every 20 s. Any close needs first reason/rc, activity time and lifetime; zero updates alone does not test queue saturation. |
| B: update-count effect | After an idle wait, make 20 discrete value changes about two seconds apart, then separately do continuous fast slider drags. Record update times, initial/final Controller revisions, enqueued/taken/sent counts and queue_full. Count actual revision changes, not finger movements. Repeat fast drags with a fresh boot and a different initial wait. |
| C: elapsed-time effect | Repeat idle, slow updates and fast updates with different start times. Compare close age relative to handshake, first change and the 20 s PING/60 s inactivity rules. A fixed-age close with queue_full=0 points away from the reproduced drain defect. |
| D: multiple clients | Repeat with two, three and then four WS sessions; distinguish tabs on one station from separate stations. Capture each browser's last revision and each generation's closure; one slow client may close while others and HTTP remain alive. Four clients are a new hardware gate. |

Expected correction acceptance: more than seven updates, including slow and fast
input, reach each client in order; representative normal editing produces no
queue_full, no send failures, no unexpected WS closes and no first-failure marker.
Under deliberate overload, existing bounds still reject/close and diagnostics
identify why. There is no reconnect loop hiding a failed session.

Also test touch/web channel edits, blackout ON/edit/OFF and preset load while
clients are active. For storage-mutating commands use disposable user-approved
slots only. Confirm local UI, display sleep/wake, battery/speaker and physical
DMX remain operational. Compare /status heap minimum/current, largest blocks,
PSRAM, HTTP/worker HWM, owner gaps and TX errors/stop against Stage 6A. Require
no sustained heap decline, reset, transport errors or unsafe owner gaps.

Capture immediately after any disconnect:

1. Full browser evidence: close code, wasClean, connection lifetime, initial and
   last revision, update timestamps/count and when the slider moved.
2. `/ws-diagnostics`: first and bridge_first records, last_close, all counters,
   current generations, work progress and Wi-Fi disconnect timestamp/count.
3. `/status`: HTTP status code plus complete response, not only station count.
4. `WS_FIRST_FAILURE` console line if available.

Do not interpret successful GET 200 or stations=1 as proof against a WS-only
close or transient association loss. Do not interpret server ESP_OK as browser
receipt. If the first reason is send_failed, idle, validation or native
unattributed with queue_full=0, investigate that evidence rather than declaring
the old pump the hardware cause. Browser 1006 alone cannot select a path.

External RS-485 capture is still necessary for frame coherence, all 513 slots,
blackout barriers on the wire, baud, break/MAB and frame gaps under load. Host
tests and UI/fixture observations do not establish electrical timing correctness.

## Scope and rollback

All work remains on `codex/stage6a-websocket-controller`, uncommitted, over the
same Stage 5 HEAD. No capacity, native framework, DMX transport, timing/buffer,
partition, source-history, credential or preset-format change. No upload,
erase, reset/stash/discard, commit, push, merge, tag or release was performed.
If hardware reveals another cause, retain this working tree and evidence.
Use a separate safe worktree at tested Stage 5 for rollback; do not destroy
current work or backups. Stage 6B remains deferred.
