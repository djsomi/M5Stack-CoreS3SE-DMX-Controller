# Stage 6B.2 - Live read-only WebSocket Controller projection

Status: the user accepted the Stage 6B.2 hardware baseline with iPhone, Windows
and MacBook clients, including two/three-client operation. The user also confirmed the silent-probe/production-trace cleanup smoke test.
The final scoped bank-arrow touch rule passed automated validation and the user
confirmed the final physical iPhone test PASSED: rapid bank navigation no longer
triggers double-tap zoom. Stage 6B.2 is approved for local closure on
`codex/stage6b-web-ui`, based on `2c69aa541dc404c2cef3deb821265ee7d45f26b3`.
The closure commit contains only the reviewed Stage 6B.2 files. No upload, erase,
push, merge, tag or release was performed by this closure task. Stage 6B.1 visual
acceptance is preserved. Earlier dated validation notes below record interim states.

## Scope and exact files

The browser now displays validated Controller working values, Blackout, Sender
state and preset occupancy. It remains read-only: faders, Blackout and every
preset mutation button stay disabled, with no mutation handlers/encoder. There
is no HTTP polling, external dependency, credential/OTA work, new endpoint,
partition change, NVS access, driver or touchscreen modification. Firmware
changes are limited to bounded network diagnostics (including temporary AP
station tracing) and the proven
sub-millisecond socket-timeout guard described below. Foreground liveness now
uses the existing read-only state.request contract; no protocol schema changed.

New files:

```text
web/src/js/protocol.js
web/src/js/commands.js
web/src/js/transport.js
tests/web/test_live.mjs
tests/web/fixture_server.py
tests/network/test_station_diagnostics.cpp
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
```

Modified files:

```text
scripts/web_ui_bundle.py
web/src/js/state.js
web/src/js/fader.js
web/src/js/editor.js
web/src/js/presets.js
web/src/js/status.js
web/src/js/shell.js
web/src/js/app.js
web/src/index.html
web/src/style.css
web/src/app.js
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/style.css.gz
web/generated/manifest.json
tests/web/run_tests.mjs
tests/web/check_pipeline.py
tests/web/README.md
include/websocket_diagnostics.h
src/network_service.cpp
src/websocket_diagnostics.cpp
tests/network/test_websocket.cpp
tests/network/test_websocket_adapter.cpp
tests/network/check_websocket_integration.py
tests/network/check_softap_integration.py
tests/web/check_scope.py
```

The Stage 6B.0 plan, Stage 6B.1 report, historical folder, unrelated report,
existing logs/backups and stash are preserved. No files are staged.

## Architecture, ownership and rendering

`protocol` decodes bounded v1 server messages. `commands` tracks one read-only
snapshot request and one last outcome. `transport` owns a single native socket,
generation and timer. `state` owns the sole private Uint8Array(512), eight preset
codes, ordering baseline and lifecycle. Views receive only its frozen read
interface/selectors, never writable universe references or ingestion methods.
`app` wires one store/client per document and guards repeated startup.

Validated snapshots/bulk updates replace all values and metadata synchronously
before notification. Channel updates change one indexed byte; mode updates do
not discard values/presets. No browser Blackout restore snapshot exists. The
server's OFF/full update alone determines restored values. Incoming updates are
applied in order even when rendering is coalesced.

One pending requestAnimationFrame paints the latest projection. Hidden pages
cancel painting; inactive views skip stream-triggered rendering and refresh on
navigation. Editor creates only 4/6/8 visible cards, rebuilding on bank changes,
then changes only differing values/positions/text/attributes. Stream updates
never overwrite an in-progress Go-to-CH entry. Normal preference/navigation
handlers stay local. Shell scroll reset, same-tab preservation, keyboard focus,
100svh/safe-area geometry and persistent nav are retained. CSS adds only a live
connection-dot color using the existing blue token and stale-grid opacity.

Before synchronization, values are unavailable (em dash), not illustrative
Controller values. After loss, at most one last-known universe remains visible,
dimmed and explicitly stale. Blackout, Sender and preset labels become unknown,
and live revision/sequence fields become unavailable. The banner becomes
READ-ONLY LIVE only after a valid snapshot. Live status separates synchronization
from socket lifecycle. Unsupported firmware/profile/memory/client-count/physical
transport-health fields remain unavailable; no device provenance is invented.

## Actual Stage 6A protocol mapping

Inspected sources: `src/websocket_protocol.cpp:encode`,
`src/websocket_bridge.cpp:state/ownerPoll/reply/completed`,
`include/websocket_bridge.h` and `docs/STAGE6A_WEBSOCKET_IMPLEMENTATION.md`.
The abbreviated user examples omit required fields; the client uses the actual
encoder schema. All server messages require `v/type/id/revision/seq`.

| Message | Validated payload / behavior |
| --- | --- |
| hello | id=0, 512 channels, max_message=256, max_clients=4, max_outstanding=4 |
| state.snapshot | Exactly 512 integer bytes, 8 preset codes, boolean blackout/sender; id=0 initially or matching state.request ID |
| state.update channel | id=0, channel 1..512, value 0..255, blackout/sender |
| state.update bulk | id=0, complete values AND presets plus blackout/sender |
| state.update mode | id=0, blackout/sender, no value/preset replacement |
| command.ack | Nonzero ID, applied/no_change and domain 0..11; never treated as a snapshot |
| command.error | Known result/reason, domain 0..11; pre-owner revision/seq=0 is not an authoritative reset |

Preset codes: 0 Empty/logically cleared, 1 Occupied, 2 Corrupt, 3 Storage error.
Unsynchronized is Unknown, never Empty. Physical preset metadata changes arrive
as bulk updates; there is no independent occupancy-only update in this encoder.

Payloads must be text, ASCII as emitted by this encoder, at most 2560 bytes,
with exact allowed fields, correct arrays/booleans/integers and v1. This is the
server formatter bound, distinct from the 256-byte incoming-command limit.
Malformed/partial snapshots are never applied. Sequence/revision are canonical
unsigned 64-bit decimal strings, validated with BigInt and kept exact in UI.
Sequence must increase and revision must not decrease within a socket epoch.
Gaps are valid, as are equal revisions on mode/no-change observations.

The owner enqueues hello/full snapshot before making the session broadcast-ready.
An update before this initial boundary is a protocol error, not something to
buffer indefinitely. Same/older sequence messages are safely ignored. A newer
sequence with regressed revision triggers one bounded fresh-snapshot request.
While awaiting it, updates are discarded without queuing; the ordered snapshot
and its subsequent updates recover current state atomically. A second ordering
fault retires the socket and retries with backoff rather than creating a resync
loop. Isolated schema/initial-order faults also recover on a new socket. Explicit
version incompatibility or three failures without a valid synchronization require
Retry. See the stability investigation below for the change and its limits.

## Lifecycle, requests and recovery

URL is derived from the page origin: ws://host/ws, or wss on an HTTPS-served
page. The firmware still serves its existing HTTP origin/policy; no TLS or
alternate-host support is added. No navigator.onLine dependency is used.

Lifecycle: Connecting -> Synchronizing -> Connected/Live. Opening HTTP or the
socket never establishes readiness. Initial server hello/snapshot are automatic;
normal active monitoring sends no JSON while authoritative updates arrive.
Idle-only state.request probes are described in the final investigation below. Clean/unexpected close and socket errors
mark state unavailable and reconnect with jittered 0.5/1/2/4/8-second capped
backoff, reset only by valid synchronization. One versioned timer handles connect,
snapshot deadlines (5 s), resync, backoff and idle health-probe deadlines.
There is no HTTP polling or new heartbeat schema.
The existing server control PING/native browser PONG mechanism is unchanged.

Only `state.request` can be serialized, using increasing uint32 IDs per socket.
One pending read-only request is stricter than the advertised four incoming
request credits. This limit is unrelated to server-generated state updates.
A matching snapshot resolves the request; unknown IDs and id=0 errors cannot
free its slot. A matching error closes/reconnects. An ack for state.request is
an incompatible response: this server returns a snapshot instead. No result is
presented as fixture delivery or successful browser mutation. Timeout/disconnect
marks a pending request uncertain, clears it and never replays it on a new socket.
Only one ordering-recovery request is permitted in a session; there is no queue
of read requests and no retained command history beyond one outcome.

Visibility hidden/pagehide pauses monitoring, cancels the timer/render and retires
the socket. Visible/pageshow waits for native closure, creates a fresh generation
and requires a new hello/full snapshot. Repeated navigation/start/resume does
not create duplicate sockets. Old message/error/close and cancelled timer callbacks
cannot overwrite a new generation. No new socket is opened while the prior native
socket is CLOSING. Browser/OS close delays can therefore delay recovery; this
avoids overlapping native connections rather than forcing an unsafe fake abort.
Manual Retry is available in Status. No state or request is replayed after resume.

## Initial implementation automated and browser validation (before stability investigation)

- PASS: existing foundation/preferences/bank/module tests and navigation tests,
  including 100 switches, scroll owners, same-tab position, keyboard/AT focus,
  history deduplication and persistent nav.
- PASS: 19 live-integration groups using actual production factories and mocked
  socket/timer/DOM contracts: strict schemas, uint64 precision, malformed arrays,
  atomic snapshots, channel/mode/bulk/Blackout/presets, seq gaps/duplicates,
  one-request resync, pre-owner/foreign results, deadlines/backoff/reset,
  uncertain timeout/no replay, generations/stale timers, closing/resume, bounded
  request tracking, independent stores/preferences, visible-card reuse, typed
  input preservation, status labels, real bootstrap and inactive-view behavior.
- A scripted 10000-update burst took 43 ms in the recorded Node VM run, retained
  one pending RAF and caused zero card rebuilds after initial bank creation.
  This is a host-test observation, not phone/ESP32 throughput or latency evidence.
- PASS: eight pipeline cases, six infrastructure fixtures, all 16 freshly
  compiled host cases (including actual patched ISR/transport and native WS
  adapter), network/WS/Dispatcher/canvas/startup/SoftAP integration and driver
  patch-state regressions. The unchanged scope guard freezes every Stage 6A
  firmware/header/driver/partition/build input except named UI composer/pipeline
  exceptions. Asset-specific tests now allow the intended native socket and
  bounded transport/RAF while still prohibiting mutations/HTTP polling.
- Actual generated gzip assets/CSP were served on loopback by the finite
  standard-library fixture helper. Chromium showed LIVE only after snapshot,
  changing values, exact counters above Number.MAX_SAFE_INTEGER, Blackout state,
  preset states and disabled actions. Fixture stop produced Reconnecting/stale/
  unknown metadata; restart recovered automatically without page reload, and
  final hashed assets were reloaded. Native connection-refused console messages
  during deliberate fixture outage are expected; no application JS/CSP errors
  were observed while connected. This is not a real ESP32/native-IDF handshake,
  two-device acceptance, electrical measurement or Safari validation.

Reproduce from the repo with existing tools (Node is test-only):

```powershell
node tests/web/run_tests.mjs
python -B tests/web/check_pipeline.py
python -B tests/network/check_websocket_integration.py
python -B tests/infrastructure/test_infrastructure.py
python -B tests/web/fixture_server.py --port 8766
```

The fixture serves synthetic data at http://127.0.0.1:8766/ only; it rejects
mutation requests and never contacts hardware. It is not shipped in the bundle.
Ignored validation logs/evidence are under `dist/stage6b2-validation-20261025/`.

## Initial asset, firmware and allocation impact (before stability investigation)

Existing Python 3.11.7/zlib 1.2.13 composition/gzip/manifest/hash URLs are retained.
Three flash-backed assets only; no filesystem or runtime device decompression.

| Asset | Raw bytes | Gzip bytes | Gzip delta vs 6B.1 |
| --- | ---: | ---: | ---: |
| Rendered HTML | 9540 | 2772 | +86 |
| CSS | 12397 | 3521 | +33 |
| JS bundle | 27554 | 7746 | +4545 |
| Total | 49491 | 14039 | +4664 |

JS raw delta is +17696 bytes. Each compressed asset is below 16384 bytes.
Authored modules and generated outputs are checked in as normal project changes;
a clean clone needs no Node/npm/Python regeneration step to build verified assets.

| Environment | Build | Static RAM | RAM delta | Flash | Flash delta | firmware.bin |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87276 | 0 | 1054069 | +4672 | 1054480 |
| cores3se_base | PASS | 87276 | 0 | 1054037 | +4672 | 1054448 |
| cores3se_unit_diagnostics | PASS | 89892 | 0 | 1056197 | +4656 | 1056608 |
| cores3se_base_diagnostics | PASS | 89892 | 0 | 1056165 | +4656 | 1056576 |

All four receipts match source SHA-256
`a9258f210a0b7df2d0135517be7103f442922a58dc4654e4cc13059b2dca5509`
and asset bundle `0ce824cda2ddd9fa4956e8dcfdfa473ef33bf962af677f728c6dda5a63b1868b`.
16 MiB/two 6 MiB slots and partition SHA-256
`6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`
are unchanged. No release/FULL.bin or migration package was generated.

The initial implementation added no firmware tasks, persistent buffers or caches.
The later stability diagnostics add scalar state as measured below. Existing SDK
TCP/WS allocations are exercised by one connection per visible client; actual
runtime free/minimum/fragmentation/stack/owner-gap impact must be measured on
hardware. Browser allocations include the 512+8-byte private typed arrays,
bounded transient decoded JSON, 4/6/8 cards, one pending timer/RAF/request and
native browser WebSocket resources. JS-engine/native-buffer overhead is not
measured here; native inbound buffering cannot be bounded by a JS parser alone.

## Hardware acceptance and limitations

The initial implementation used local synthetic fixtures. The later hardware
observations are recorded below; the stability revision has not been uploaded or
hardware-validated. When approved, manually upload:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

Use the existing migrated 16 MiB CoreS3SE + Unit DMX setup and credentials.
No erase, NVS reset, credential regeneration, filesystem upload or FULL.bin is
needed. Base builds remain compatible but hardware output remains experimental.

1. Cold-load the page offline on iPhone Safari, Home Screen mode and a second
   stable browser. Confirm Connecting/Synchronizing precede LIVE and exactly
   one `/ws` per visible instance. Before snapshot values/Blackout are unknown.
2. Physically edit channels 1, 21 and 512, including rapid slider/Precision
   updates. Verify both clients see identical authoritative values; each can
   navigate banks independently, with 4/6/8 layout, scroll reset, current-tab
   offset preservation, percentages, input focus and safe-area nav unchanged.
3. On the physical device toggle Blackout ON, edit working values, then OFF.
   Browser shows working changes and ON state; verify physical zero output and
   server-provided restoration on OFF. Neither page sends a mutation.
4. Load an existing valid preset on touch (no destructive test writes). Verify
   all 512 values and occupancy update on both clients. Host fixtures cover
   corrupt/storage codes; do not corrupt real NVS merely to test their labels.
5. Close one client; the other remains live. Test temporary Wi-Fi loss, device
   reboot, idle >60 s, lock/unlock, background/foreground and Home Screen resume.
   Hidden clients intentionally release their socket; resume must use fresh
   synchronization, without replay or duplicate connections.
6. Check outgoing WS text: normally none; only `state.request` is permitted for
   ordering recovery. Native control PONG is normal. No fetch/status polling,
   external request, channel.set, blackout.set or preset mutation may be sent.
   Clicking/dragging disabled controls cannot change fixtures or presets.
7. Manually collect `/status` and `/ws-diagnostics` before/after 1/2 clients,
   frequent touch changes, bulk loads and repeated reconnects. Compare free/
   minimum internal heap, largest internal/DMA block, PSRAM, stack HWM, owner
   gaps, transport faults and WS queue/full/send/work/closure counters against
   6B.1. Require no sustained heap decline/reset/new TX fail-stop and responsive
   touch/display/speaker/battery/sleep functions. Four clients remain a separate
   unvalidated capacity/stress gate, not a new claim.

Browser/OS networking decides when a silent Wi-Fi/TCP loss becomes an error or
close event. LIVE means synchronized with no observed failure, not instantaneous
physical-link monitoring. The final revision below adds bounded idle state.request probes. Native
closure may still delay reconnect; explicit version mismatch or repeated initial integrity
failures require deliberate Retry; isolated integrity faults retry automatically.
Only the browser-current origin is used; existing fixed Host/Origin/CSP policy
is unchanged. Predictable WPA2 access remains the existing security boundary,
not a new authorization mechanism. No real fixture delivery or DMX waveform
correctness is inferred from snapshots, tests, acknowledgements or counters.

Stage 6B.3 and later must add separately approved mutation scheduling, final
gesture retention and context barriers. This phase provides no such controls.
If hardware testing fails, preserve this work and evidence; use the committed
6B.1 baseline in an isolated checkout for rollback on the same layout without
erasing user NVS. No commit, flash, push, merge, tag or release occurred.


## Hardware Stability Investigation and Recovery

Status: targeted fixes and bounded instrumentation implemented, UNCOMMITTED.
Hardware validation of this revision is pending. The underlying iPhone TCP send
stall is NOT established or claimed fixed. No firmware upload was performed.

### Hardware observations supplied by the user

These are physical-device measurements, not results from the local mocks.

| Trial | Setup | Controller events | Enqueued/taken/sent | Full/send failures/slow closes | Outbox high | Wi-Fi disconnects |
| --- | --- | ---: | --- | --- | ---: | ---: |
| A | Desktop | not supplied | 298/298/298 | 0/0/0 | 3 | 1 |
| B | Desktop full sweep | not supplied | 345/345/345 | 0/0/0 | 3 | 0 |
| C | Desktop + iPhone short sweep | 512 | 1022/1022/1022 | 0/0/0 | 2 | 0 |
| D | Desktop + iPhone | 722 | 884/875/873 | 1/2/1 | 8 | 0 |
| E | Desktop + iPhone sustained | 3591 | 6311/6279/6277 | 4/2/4 | 8 | 0 |
| F | iPhone alone | 2390 | 1191/1183/1182 | 1/1/1 | 8 | 0 |

B/E/F reported zero DMX TX errors. F reported 2391 publications and minimum
internal heap around 140 KB. Differences between events and deliveries do not
prove loss: connection intervals and numbers of recipients differ.

D first failure: 59238 ms, gen 2, fd 52, send_failed/-1/errno116, last sent
revision246, last enqueued248, queue not full. E: queue full at71194 ms gen2
slot1 queued8 rejected1253; send failure71254 ms gen1 fd51 last sent1244.
These are different sessions, not proof of causal blocking between them.
F: queue full81943 ms gen1 slot0 queued8 last enqueued1161 rejected1162;
send failure81951 ms same gen fd52 last sent1152 queued8, errno116.
This establishes that one client suffices. It does not establish when the send
started blocking or why TCP stopped accepting output. F work queued/started/
completed were14033/14033/14033, failures0. User recording shows LIVE, frozen
CH001, terminal Connection error, and successful manual Retry.

### Source findings: actual send path and ownership

- `src/websocket_bridge.cpp:Bridge::completed/state/broadcast/push/take`:
  application-task command completion publishes each revision change; channel
  events carry a single value, preset/blackout operations carry a coherent full
  universe. No publication throttle or coalescing exists. Every ready client
  receives a copy in its eight-event ring. Snapshot/update/result order is FIFO.
  max_outstanding4 limits incoming requests, not these notifications.
- `src/network_service.cpp:worker/sweep/wsSend/io`: the network worker requests
  one HTTP-task work item every20 ms, only if none is pending. The HTTP task
  alone formats into its existing shared2560-byte output buffer and invokes
  `httpd_ws_send_frame_async`. The name does not make this invocation deferred:
  IDF4.4.7 synchronously calls the socket override for header then payload.
  [Exact IDF source](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_ws.c#L375).
  Payload lifetime is valid until return; no frame pointer is queued afterward.
- The override loops on short writes to provide the exact-write contract IDF
  needs. Header and payload share one100000-us absolute deadline. Before each
  send it sets that socket's SO_SNDTIMEO to remaining time. Native socket/lwIP
  scheduling granularity can add overhead; this is not a measured100-ms wall
  time guarantee. `http.send_wait_timeout=1` is superseded for WS by this path.
- The installed toolchain's `xtensa-esp32s3-elf/include/sys/errno.h:147` defines
  ETIMEDOUT116. The old log cannot distinguish the explicit deadline check,
  setsockopt, or send returning that error. New first/last closure `io_op` does:
  1=deadline,2=setsockopt,3=send,4=recv;0=unclassified. No Linux mapping assumed.
- One stalled send blocks the shared HTTP server task. The pump's20-ms budget
  is checked between frames, not inside the socket call. It rotates the first
  slot and allows four rounds, but cannot promise independent sub100-ms latency.
  While blocked, the application continues publishing. At100 events/s, eight
  empty queue slots represent only80 ms; it can fill within one stalled send.
  A queue-full event may precede final send timeout even if the stall began first.
  Normal ideal capacity is four events/client per20-ms work opportunity; real
  capacity depends on frame size, socket behavior, Wi-Fi and scheduling.
- Saturation drops the bridge client; timeout closes it without continuing a
  partial frame. closeSession/native close cleans the bridge idempotently.
  Tokens include monotonically allocated generations; stale commands/events
  cannot access a replacement client. Sweep work carries no fd/client pointer,
  resolves current sessions in the HTTP task and has at most one pending item.
  LRU purge remains disabled; an available /status does not prove a WS session
  remains alive. No completion ack from the browser is involved in sending.
- The20-ms maintenance/pump explains roughly46-47 runs/s including idle runs.
  Existing idle control PING20s and receive-idle60s rely on native browser PONG,
  not channel activity. This is a possible later scheduling optimization, not a
  proven defect. Work busy/last/max/delay samples now distinguish idle drain
  calls from blocked execution. No cadence optimization was implemented.

The network cause of the iPhone stall remains unknown: RF/TCP buffering, browser
background/scheduling and delayed network delivery are candidates, not findings.
No evidence justifies increasing queue depth/timeouts, changing TCP settings or
weakening message validation. Zero station disconnects does not prove loss-free
TCP delivery. Earlier Windows AX200 problems do not justify firmware changes.

### Frontend findings and corrections

`web/src/js/transport.js:fault` was the only route selecting the old terminal
Connection error phase. onerror and unexpected onclose already retried with
capped delay and no attempt limit. A server timeout followed solely by1006 does
NOT, through those paths, explain the terminal screen. Before this change every
schema/order violation was terminal and its reason was collapsed into one string.
The hardware recording cannot establish which validation failed. This remains
an important missing measurement; no fabricated parse error is attributed to iOS.

Implemented changes:

1. Isolated schema/order failures retire the suspect socket, mark cached state
   stale and retry. Three failures without a successful full synchronization
   stop visibly; explicit integer protocol version mismatch stops immediately.
   They remain distinguishable from recoverable transport failure. No malformed
   data is applied. Retry clears the integrity guard deliberately.
2. Jittered500/1000/2000/4000/8000-ms capped delay continues indefinitely for
   transport failures. Attempt diagnostic saturates at UINT32_MAX. Valid fresh
   snapshot resets attempts and consecutive integrity failures. One pending
   request and timer, native closing ownership, generation guards and no replay
   are preserved. One same-session resync remains bounded; further recovery
   uses a new socket. Manual Retry still works.
3. Corrected an observed diagnostic edge case: onerror can see native CLOSED
   before onclose is delivered. The later close event is captured only within
   its retiring generation; unavailable synthetic finalization never erases the
   last observed close code. Late events cannot overwrite a newer generation.
4. State carries compact frozen health metadata: transportOpen, last browser
   close code/wasClean, last classification, attempt, valid message timestamp/
   revision, rendered revision and background state. Status displays these in
   its existing panel style. No payload/credentials/history are retained.
5. Rendering remains one requestAnimationFrame batch, active view only,4/6/8
   persistent visible cards. Authoritative processing continues independently.
   Last received, applied and rendered revisions separate delivery, acceptance
   and projection. Existing app subscription/atomic snapshot tests remain valid.

LIVE means a validated snapshot and no observed subsequent failure, not proof
of present TCP liveness. OPEN alone is insufficient. A quiet Controller is valid;
no channel-idle timeout was added. Message age is sampled at render and explicitly
labelled that way; it is not a ticking health watchdog. A stale native TCP path
can remain OPEN until browser/OS error/close. Native CLOSING can delay replacement;
JavaScript provides no safe force-abort while guaranteeing no overlapping socket.

The first stability revision deferred idle state.request health checks pending
review. They are now explicitly authorized and implemented in the final section
below, with bounded request/response timing and no protocol schema change. No unsupported heartbeat,
HTTP polling, retries of uncertain mutations or browser mutation feature added.

### Bounded server instrumentation

Only network_service and websocket_diagnostics firmware surfaces changed.
No Controller, Dispatcher, bridge, pump, transport, UART, Wi-Fi, USB, partitions,
credentials or NVS code changed. No new task or heap/frame/log buffer was added.

`/ws-diagnostics` keeps its4096-byte static response buffer and adds compact
numeric samples (microseconds unless stated):

- `timing`: [send_timeouts, maximum_send_us, maximum_work_us,
  maximum_queue_to_work_us, work_items_with_outbound_state, last_work_us].
  Busy means an event was taken, not that all maintenance/PING work was absent.
- per WS session `tx`: [last_successful_send_ms, last_send_us,
  maximum_session_send_us, queued_at_send_start, queued_at_send_end]. Start is
  after take removed the current event. End may be0 if the bridge was dropped;
  immutable bridge_first preserves occupancy8 at saturation.
- first/last closure retains send_us and io_op. First failure is not overwritten.

Timing uses esp_timer monotonic timestamps. Existing opened_ms/activity_ms and
current ms establish session lifetime. Existing counters retain generations,
revisions, queue/full/send/slow closure and publication counts. The installed AP
station-disconnect event does not supply a reason; it remains explicitly
unavailable. No repeated Serial trace or diagnostics in DMX timing paths.

### Deferred design requiring review

Do not merely enlarge queues. If measurements show ordinary intermediate channel
notifications exceeding sustainable delivery, propose a separate bounded state
notification design: per-client latest-channel dirty state between ordered
blackout/universe barriers, retaining command acknowledgements/errors and request
snapshots in FIFO order. It must never replace unrelated-channel changes with a
single last delta, cross blackout transitions, lose preset occupancy, regress
revision or discard command results. Define atomic catch-up/full-snapshot rules,
slow-client fairness, maximum latency and bounded memory before implementation.
A resumable nonblocking native send path would also need review; current shared
HTTP task can delay healthy clients by a bounded failed-frame interval. Neither
bridge redesign, coalescing, new tasks nor protocol changes were implemented.


### Automated acceptance, resources and remaining hardware gate

All final checks passed:

- 23 frontend live groups plus unchanged navigation and foundation suites:
  abnormal close/error,30 consecutive retries/delay cap, manual Retry, one
  native socket/timer, stale callbacks, already-CLOSED/later-close capture,
  initial/bulk/Blackout/restore order, matching snapshot completion, no replay,
  lifecycle, full0..255..0 sweep, repeated integrity guard and explicit version
  incompatibility. Only visible banks render; navigation/scroll/focus and local
  preferences remain intact. Faders/blackout/preset mutations remain disabled.
- Eight pipeline cases, six infrastructure fixtures and all16 compiled host
  cases (Controller, Dispatcher, actual ISR transport, WS bridge/pump, actual
  extracted endpoint/I/O, AP policy, framebuffer/startup, HTTP/assets).
  Added actual-bridge single slow and fast+slow burst tests: the slow queue
  closes at capacity while the drained client converges; replacement receives
  atomic latest snapshot. Actual extracted send path simulates timeout, exact
  short writes and first-failure retention. This does not simulate real lwIP/RF.
- Maximum-width diagnostic JSON with six synthetic WS sessions parses as JSON
  and occupies3571/4096 bytes. No static response-buffer increase.
- Protocol/task integration, startup/canvas/AP, Dispatcher and all patch-state
  regressions pass. Patch tests cover fresh/repeated/mixed/unknown states.
- Deterministic composer, pinned gzip, content hashes and per-asset16-KiB cap
  pass. No HTTP polling or mutation encoder was added. Git whitespace passes.

Local Chromium fixture testing at390x844 confirmed stable bottom-nav geometry
(bottom844px), document scroll0, no horizontal overflow, valid live diagnostics,
forced fixture outage -> stale/Reconnecting -> fresh LIVE without reload and
retention of1006/wasClean=false after reconnection. Manual Retry resets attempts.
This is not iOS Safari or real-firmware evidence. Expected native connection
errors during the deliberate outage are not application script/CSP failures.

Final build and asset measurements follow (deltas compare the same profile).

| Environment | Result | Static RAM | Flash | Flash delta vs pre-fix6B.2 | Flash delta vs6B.1 |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87460 | 1055809 | +1740 | +6412 |
| cores3se_base | PASS | 87460 | 1055773 | +1736 | +6408 |
| cores3se_unit_diagnostics | PASS | 90076 | 1057961 | +1764 | +6420 |
| cores3se_base_diagnostics | PASS | 90076 | 1057929 | +1764 | +6420 |

Static RAM increases184 bytes in every profile against both baselines. Added
scalars/timing state in existing six sessions, counters and closure records plus
one64-bit pending-work timestamp account for this change; no new persistent
buffer, task or heap allocation. Diagnostics copies consume a little more of
the existing HTTP task stack; stack HWM/heap/timing costs need hardware checks.
Extra clientInfo calls take brief existing bridge locks; zero timing impact is
not claimed. No transport-critical code was instrumented.

| Asset | Raw bytes | Gzip bytes |
| --- | ---: | ---: |
| index.html | 10199 | 2905 |
| style.css | 12397 | 3521 |
| app.js | 31033 | 8708 |

JS delta vs pre-fix6B.2: +3479 raw / +962 gzip bytes.
JS delta vs6B.1: +21175 raw / +5507 gzip bytes.
Final source fingerprint: `dbe934853a665e211850c7025aad0435a8df4b221b64eba45a3ac8695edba3a4`.
Asset bundle: `3f39d61f32e7f5074562fd3840147177f162067a9ca6f7c15ccb803ded2b959d`.

Browser health data is fixed-size metadata with constant classification strings.
It adds transient frozen objects per message; native browser receive buffers
remain outside application memory control. JSON, frame, counter, universe,
preset and field-set failures have distinct non-sensitive classifications.

### Exact uncommitted inventory against approved 6B.1

```text
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
include/websocket_diagnostics.h
scripts/web_ui_bundle.py
src/network_service.cpp
src/websocket_diagnostics.cpp
tests/network/test_websocket.cpp
tests/network/test_websocket_adapter.cpp
tests/network/check_websocket_integration.py
tests/network/check_softap_integration.py
tests/network/test_station_diagnostics.cpp
tests/web/README.md
tests/web/check_pipeline.py
tests/web/check_scope.py
tests/web/fixture_server.py
tests/web/run_tests.mjs
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/commands.js
web/src/js/editor.js
web/src/js/fader.js
web/src/js/presets.js
web/src/js/protocol.js
web/src/js/shell.js
web/src/js/state.js
web/src/js/status.js
web/src/js/transport.js
web/src/style.css
```

Only 19 already-present files changed during this investigation (the new
firmware changes are limited to the three diagnostic surfaces listed above).
Historical `_kezdeti kezi fejlesztes/` (actual directory name retains accents),
untracked Stage3 regression document, ignored logs/backups, baseline commit,
index and stash are preserved. Branch remains codex/stage6b-web-ui. No commit,
upload, erase, NVS change, push, merge, tag or release.

### Real-device acceptance procedure

Manually, after reviewing this revision:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

Use the existing16-MiB setup; no erase, filesystem upload or NVS operation.

1. Desktop alone:0->255->0 physical sweeps, inspect live values and baseline
   /status plus /ws-diagnostics before/after. Keep this quiet-controller session
   idle at least60s too; lack of channel changes must not produce a false fault.
2. iPhone alone (Safari and Home Screen): sustained rapid sweeps. If frozen,
   capture Status received/applied/rendered/age/classification/close data and
   manually collect server diagnostics immediately. Do not reload first.
3. Desktop+iPhone: same sweeps and preset loads. Compare each generation's
   last revision/send sample; closing/backgrounding one must not stop the other.
   Some shared HTTP delay is a known limitation, not declared eliminated.
4. Recovery: switch the test client's Wi-Fi away from the AP and back (leave
   ESP32/presets untouched). Require automatic capped-backoff recovery with a
   fresh hello/snapshot and matching latest physical values. Separately test
   manual Retry during an interruption. No browser DMX commands may be sent.
5. Foreground/background, screen lock/unlock and Wi-Fi return for Safari and
   standalone Home Screen. No duplicate active socket after native closure,
   stale LIVE values or command replay. Record native CLOSING delays explicitly.
6. Run20-30min with two clients, periodic sweeps and preset loads; verify
   physical blackout ON, edits while zero, OFF entry-snapshot restoration and
   uninterrupted fixtures/touch/battery/speaker/sleep functions.

Record before/after queue_full, send_failures, slow_closes, wifi_disconnects,
updates_enqueued/taken/sent, first/last closure, timing/tx samples, minimum free
internal heap, HTTP/worker stack HWM and DMX TX errors. Correlate monotonic ms,
session generation and send_us/io_op; send duration/worker delay are new hardware
measurements still required. Require no unexplained terminal frontend fault;
any protocol classification must be retained and investigated, not dismissed.

Remaining gates: actual iPhone sustained-delivery reliability, exact reason for
TCP stalls and prior validation faults, latency/fairness under blocked native
send, runtime heap/stack and external electrical DMX verification. Automated
checks do not prove RF reliability, fixture behavior or wire-frame integrity.
Larger coalescing/health/send architecture requires review before implementation.


## iOS stale-socket detection and 518 ms send-stall analysis (2026-10-26)

This section supersedes the previous revision's passive foreground liveness
limitation. Current changes remain uncommitted and have not been flashed.

### New physical evidence and what it establishes

User-provided iPhone-only evidence: a send began approximately29.747s; queue full
at29.980s rejected revision173; failure at30.265s measured518330us, errno116,
io_op1, generation1, last sent revision163. ws_opened1/ws_closed1, no CLOSE frame
sent, no station disconnect. At103.219s the server still had zero WS clients and
no subsequent successful opening. The foreground app continued to show Connected.
Moving the Home Screen app to background and foreground restored updates.

These measurements establish a server send-path stall and failure of passive
foreground liveness detection. Zero Wi-Fi disconnects does not establish a
healthy TCP path. No CLOSE frame is expected from the failed-frame cleanup path;
a browser is not guaranteed to observe transport failure immediately.

### Why foreground became stale but resume recovered

Before this revision, only observed onerror/onclose, synchronization deadlines,
protocol faults and visibility transitions could retire the socket. After a good
snapshot, no foreground health timer existed. A native WebSocket could remain
OPEN without another JS event, leaving ready=true and cached state labelled LIVE.
The server-side close cannot directly change that browser-side state.

`app.js:pause` calls client.suspend on visibility hidden/pagehide, cancels pending
rendering and invalidates the generation. Visible/pageshow calls resume; after
old native closure it establishes a new socket and requires hello/full snapshot.
That existing path explains the observed recovery. It is preserved, not replaced
or bypassed by the health check. No stale command is replayed on either path.

### Verified existing request/response contract

`commands.js:snapshot` encodes only {v:1,id:N,type:"state.request"}. In
`src/websocket_protocol.cpp:decode`, it sets Request.snapshot without a domain
mutation. Bridge.receive validates monotonic per-session IDs, queues the request
and accounts for an outstanding credit. `Bridge::ownerPoll` captures the atomic
working universe, preset occupancy, blackout, sender, revision and next sequence
on the application task; it queues one state.snapshot carrying the same ID and
releases the request credit. There is NO separate command.ack. An unchanged
Controller revision is valid; the snapshot still has a newer sequence. It does
not save presets, alter blackout, publish DMX or mutate the Controller.

All existing server validation and eight-event outboxes remain unchanged. A
single matching complete snapshot resolves the frontend request. Other updates,
foreign IDs, command results or partial payloads cannot satisfy the probe.

### Implemented foreground liveness policy

- Keep the existing single versioned timer; do not add an interval or a second
  pending request. Start an idle watch after successful initial synchronization.
- After15000ms without an APPLIED authoritative snapshot/update, send one existing
  state.request. Normal accepted updates refresh the idle timestamp. They do not
  cancel/recreate a timer per slider event: the watch rechecks remaining idle time.
  Stale/duplicate messages and unrelated results do not postpone this health test.
- Wait10000ms for the matching complete snapshot. While pending, the socket can
  still deliver normal ordered updates; they are applied but do not resolve the
  request. A probe does not spend the one-per-session ordering-resync allowance.
  If ordering recovery is needed during a probe, reuse that pending snapshot.
- An idle Controller is not declared disconnected: a valid unchanged-revision
  snapshot completes the check. Return to ordinary idle watching afterward.
- Keep the last synchronized projection during the response allowance, but show
  Checking connection in the header and pending health-check state in Status.
  A probe is not represented as proof of current delivery. On deadline failure,
  record health_timeout, invalidate the generation, clear the uncertain request,
  mark cached state stale and close the old native socket. Existing backoff then
  creates a new socket AFTER native closure. LIVE requires a fresh hello/snapshot.
- Use performance.now for foreground elapsed-time decisions, separate from the
  wall-clock receive timestamp shown in Status. If an idle timer is delayed,
  issue one probe with a fresh response budget, not a burst of overdue requests.
  If a response timer fires more than2s late, grant exactly one5s grace timer so
  queued receive events can run. No repeatedly extendable deadline or timer loop.
- Visibility suspend cancels watch/probe timers and uncertain requests; foreground
  resume keeps its working fresh-generation path. Late callbacks cannot alter a
  replacement session. Manual Retry remains available and still resets backoff.

Detection normally starts recovery within about25s of the last accepted update;
background/browser scheduling, the one bounded grace and native close/backoff
can extend it. JavaScript cannot force-abort a native CLOSING socket. Replacement
still waits for its close to avoid overlapping native connections; measure this
on the real iPhone. This revision does not promise an unobservable zero-latency
physical-link health signal or a strict wall-clock reconnect upper bound.

Worst-case quiet traffic is one small state.request plus one approximately2.2KB
snapshot per15s per visible client (about150B/s/client of snapshot payload). Active
authoritative traffic suppresses it. Hidden pages issue none. No HTTP polling,
unsupported ping command, mutation encoder or mutation replay was introduced.

### What io_op1 and the measured duration actually mean

`network_service.cpp:io` records io_op1 at the application deadline check before
another send/recv invocation. It supplies ETIMEDOUT116 itself. io_op3 would mean
a native send operation failed; those are distinct. A preceding synchronous
call may have returned a positive partial result AFTER its nominal timeout, or
the shared frame deadline may have expired before payload processing. The old
sample does not identify header versus payload or every individual native call.

The518330us covers the synchronous wsSend path, including socket operations and
any scheduler preemption. The shared HTTP task cannot service other sessions or
execute queued cleanup while inside that call. It does not establish that the
CPU was actively occupied for518ms. Queue full233ms after the inferred start,
before the failure518ms after start, is consistent with application-task
publications continuing while the HTTP task cannot drain. The prior queue depth
and exact native blocking intervals are not available, so other contributors
cannot be excluded. A slow client can delay healthy sessions during this call.

The application configures a100000us total frame budget, reused for header and
payload. The WS override sets per-socket SO_SNDTIMEO from the remaining budget;
http.send_wait_timeout=1 is not the effective per-call WS setting. A budget checked
between blocking calls cannot preempt one already in progress.

For this audit the official ESP-IDF v4.4.7 lwIP submodule was pinned through its
GitHub contents metadata to `a45be9e438f6cf9c54ec150581819c3b95d5af6b` (not a
moving branch or a newer driver). Audited source copies/hashes are preserved
under ignored dist/stage6b2-health-20261026. The installed SDK enables socket
send timeout support. The precompiled SDK was not replaced or rebuilt.

Exact upstream references:

- [sockets.c:timeval conversion](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/api/sockets.c#L201):
  tv_usec is divided by1000, truncating sub-millisecond values.
- [sockets.c:lwip_send](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/api/sockets.c#L1442):
  blocking send uses netconn_write_partly.
- [api_msg.c:writer and timeout](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/api/api_msg.c#L1725):
  send timeout is checked when writemore runs. A timeout with partial progress
  can return success with a partial byte count; callbacks can resume a blocked
  writer. The HTTP override must still check its own remaining deadline.
- [api_msg.c:poll/sent callbacks](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/api/api_msg.c#L369)
  and [tcp_priv.h:timer constants](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/include/lwip/priv/tcp_priv.h#L119):
  delayed writer completion depends on callback/timer scheduling, with500ms TCP
  slow ticks and a two-tick netconn poll interval. A100ms configured send timeout
  is not a hard preemptive100ms task deadline. This explains how overruns are
  possible, not which ACK/window/RF event produced this specific duration.

A proven local defect was also found: remaining1..999us became a zero-ms socket
timeout, which means no timeout. The guard now rejects left<1000us with io_op1
instead of configuring an unlimited wait. It neither increases a timeout nor
changes the shared queue. The historical trace does not establish whether it
reached that narrow window. This guard does NOT make every native call hard-
bounded or claim to solve the underlying iOS send backpressure.

Cleanup remains serialized and safe: after wsSend returns failure, the bridge
session is dropped and native close is queued (or shutdown wakes select). Native
close retires the token idempotently and releases the session. It cannot cancel
a currently blocked call from that same HTTP task. The recorded closed1 proves
cleanup completed in the test; zero clients afterward is not evidence the browser
noticed. No partial frame is retried or reported successfully after deadline.

### The three HTTP rejections

The old top-level rejected counter records only calls to the normal HTTP reject
helper. It stores no status code, URI or time. Consequently the exact three
historical reasons CANNOT be recovered from the supplied aggregate. They might
be missing optional browser resources, Host/Origin/encoding validation, rate
limit or a diagnostic response rejection; none is asserted as fact.

Actual WS handshake failures use a separate handlerFailure/upgrade_rejected
path; successful/attempted WS reconnects do not increment this HTTP reject
counter. The old frontend has no HTTP recovery requests. Thus these three
counts are not evidence of three attempted frontend WS recoveries. This finding
assumes the reported value is the top-level HTTP rejected, not bridge.rejected.

Added bounded, non-sensitive fields to /ws-diagnostics for the next reproduction:

- http_rejected: counters for [403,404,406,413,429,431,503,other], saturating.
- http_last_rejection: [monotonic_ms,status,route_class]. Route classes0..7 are
  /, /status, /ws-diagnostics, /favicon.ico, /apple-touch-icon.png,
  /apple-touch-icon-precomposed.png, /manifest.json, /ws;8 means other.

Raw paths, queries, headers, MACs and credentials are not retained. This records
all HTTP rejection statuses and the latest classified route, not an unbounded
request log. A403 still groups the existing Host/Origin policy. For unknown raw
paths or all earlier events, a contemporaneous browser Network capture is needed.
No HTTP route, rejection policy, rejection response body or Wi-Fi credential
changed; only the existing diagnostics response gains the bounded fields.

### Larger designs deferred for review

Latest-state delivery is warranted for evaluation under sustained backpressure,
but neither coalescing nor a bridge rewrite was implemented here.

1. Coalesce only a trailing, not-yet-taken run of channel-state notifications.
   Same-channel updates may retain the last delta. Multiple-channel changes can
   use a coherent existing full-universe state.update captured by the application
   owner. Never drop unrelated channels by keeping just the last delta. Carry the
   newest sequence/revision, which the existing v1 receiver already permits.
   Do not cross initial/request snapshot, mode, preset, blackout ON/OFF or command
   result barriers. Preserve a FIFO for every ack/error and snapshot completion;
   never mutate the HTTP task's in-flight Event. Existing ring slots already hold
   full Event packets; budget at most256B additional static tagging metadata and
   one temporary roughly560B owner snapshot, subject to ABI verification. No new
   wire schema is necessary, but intentionally skipping intermediate notifications
   needs an approved Stage6A contract update and ordered multi-client tests.
2. A bounded nonblocking socket-send loop (MSG_DONTWAIT plus readiness wait within
   the unchanged frame budget) could avoid waiting on lwIP's blocking-send callback
   completion. It must preserve exact header/payload writes, partial-write offset,
   EAGAIN/EINTR handling, absolute deadline and close on incomplete frames. It still
   occupies the shared HTTP task for that budget and requires its own design/tests.
3. Fully resumable per-client sends would improve fairness further, but need
   immutable in-flight events/offsets per client and a reviewed framing integration.
   A naive per-client2560B serialization buffer costs roughly10KiB for four clients;
   alternatively retain roughly560B immutable Events plus offsets and regenerate
   into the existing formatter between nonblocking writes (roughly2.3KiB persistent
   overhead, extra CPU), subject to measurement. Returning a short positive write
   to native httpd_ws_send_frame_async is NOT safe: it checks negative return only.
   No extra task, buffer architecture or driver change is authorized here.

All alternatives retain Controller/Dispatcher ownership and DMX/blackout safety.
Do not raise queue depth, send deadlines, change UART scheduling or suppress faults
as a substitute. The chosen scope is compatible liveness plus diagnostics and
the narrowly proven zero-timeout guard; transport redesign remains a separate gate.


### Final liveness regression validation and resources

All30 frontend live groups pass, along with the existing navigation/foundation
coverage. Added tests specifically exercise TCP apparently OPEN after server loss,
no probe response, stale generation invalidation, new hello/full snapshot, a quiet
Controller with unchanged revision over repeated checks,9000ms slow responses,
normal traffic suppressing probes without timer churn, matching request identity,
interleaved updates, foreground suspension/grace, background/resume and ordering
recovery reusing a health request. The actual app bootstrap checks Checking
connection -> fresh snapshot -> Connected / Live, without enabling mutations.

All16 compiled host cases pass against final assets. The extracted actual I/O
mock deliberately returns a positive partial send only after518330us: the next
application deadline check records io_op1/ETIMEDOUT, preserves that duration and
closes the session. A999us remaining-budget case makes no socket write. Existing
queue-full, slow/fast client, blackout ordering, timeout cleanup and immutable
first-failure tests still pass. HTTP status/route counters, saturation and unknown
URI nonretention are tested. Maximum-width JSON (six synthetic sessions, all
counter widths) parses and fits3718/4096 bytes; no response-buffer increase.

Eight asset-pipeline cases, six infrastructure fixtures, protocol/task/Dispatcher,
PSRAM/canvas/startup/SoftAP, driver patch-state and Git whitespace checks pass.
Generated source/asset fingerprints match all four build receipts. The partition
layout, Stage3E patch, AP settings, credentials, NVS and DMX source remain intact.

A real local Chromium check at390x844 used the loopback-only fixture's new
--stall-after2 mode. It sends normal initial state/updates, then keeps TCP open
but drops authoritative updates and state.request replies. The page visibly
entered Checking connection with transport still OPEN. Without Retry, page
reload or lifecycle transition, it timed out, closed and opened a new session,
received a fresh snapshot and returned to LIVE. The fixture logged new handshakes
about27-28s apart as each synthetic connection independently stalled. Received,
applied and rendered revisions matched after recovery; health_timeout remained
visible as the last classification. No script/CSP error, horizontal overflow or
navigation displacement was observed; nav bottom844px, document scroll0. The
fixture/tab were stopped/closed. --quiet is also available for a responsive idle
fixture; repeated quiet-response behavior was verified in the automated cases.
This is not an ESP32, Safari, Wi-Fi or electrical hardware acceptance result.

Final build resource measurements:

| Environment | Result | Static RAM | Flash | RAM delta vs previous stability | Flash delta vs previous stability |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87508 | 1056881 | +48 | +1072 |
| cores3se_base | PASS | 87508 | 1056849 | +48 | +1076 |
| cores3se_unit_diagnostics | PASS | 90124 | 1059053 | +48 | +1092 |
| cores3se_base_diagnostics | PASS | 90124 | 1059021 | +48 | +1092 |

The added eight rejection counters and three last-rejection scalars use44B of
fields; target structure alignment yields48B additional static RAM. Total Unit
RAM is232B above the87276B Stage6B.1/initial6B.2 baseline. No new queue, packet
buffer, task, heap allocation or networking framework was introduced. Snapshot
handling reuses the existing server bridge/formatter. HTTP diagnostics copies
carry the extra counters on the existing stack. Browser probes reuse one timer,
one pending request and current generation; full snapshots create bounded
transient JSON and520B typed-array replacements, without retaining old snapshots.
Real device stack HWM, fragmentation, delivery latency and minimum heap remain
hardware measurements. The earlier184B diagnostic overhead is still included.

| Asset | Raw bytes | Gzip bytes |
| --- | ---: | ---: |
| index.html | 10199 | 2904 |
| style.css | 12397 | 3521 |
| app.js | 33204 | 9258 |

JS grows2171 raw/550 gzip bytes versus the preceding stability build; every
compressed asset remains below16384B. The same three assets, deterministic
Python3.11.7/zlib1.2.13 composer, content hashes and manifest checks are preserved.
The original pre-stability6B.2 Unit flash1054069B increases2812B; Stage6B.1
Unit flash1049397B increases7484B. Current Unit flash is about16.8% of its6MiB slot.

Final source SHA-256: `5c20aedc849e12a354dccd837b25e621ee94f4c9830cdd331f3ba0161a109a61`.
Asset bundle SHA-256: `f3e3f058f7b397f292bc5fe68b1fca28faad35e4f77fd645756bff559df511af`.

Target DWARF reports sizeof(websocket_bridge::Event)=560B; host ABI reports568B.
The future design estimates above use the target size, not an assumed552B.
No proposed coalescing or per-client serialization buffer was implemented.

### Exact files changed in this liveness pass

The previous uncommitted Stage6B.2 implementation is preserved. These19 existing
files changed during this pass:

```text
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
tests/web/fixture_server.py
tests/web/test_live.mjs
web/src/js/transport.js
include/websocket_diagnostics.h
src/network_service.cpp
src/websocket_diagnostics.cpp
tests/network/check_websocket_integration.py
tests/network/test_websocket.cpp
tests/network/test_websocket_adapter.cpp
tests/web/README.md
tests/web/check_scope.py
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/src/app.js
web/src/js/app.js
web/src/js/state.js
web/src/js/status.js
```

The complete uncommitted inventory against6B.1 now has31 intended files (listed
in the earlier inventory, including check_websocket_integration.py). No file was
staged. HEAD/branch/stash and unrelated/historical files remain unchanged.

### Required real-iPhone validation for this revision

Upload manually only after reviewing these changes:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

1. iPhone alone, Safari and Home Screen: reproduce rapid physical fader sweeps.
   If updates freeze, leave the app foregrounded (do not use background/resume or
   Retry). Expect Checking connection after about15s without applied messages,
   then a matching snapshot or health_timeout and a fresh connection. Record
   native close delay and compare latest physical values after recovery.
2. Leave the Controller quiet for at least2min. A response every idle cycle must
   keep it synchronized without reconnects; request snapshots may have the same
   Controller revision. Active normal updates should suppress idle probes.
3. Background/foreground, screen lock/unlock and return from temporary Wi-Fi loss.
   Preserve the already-working lifecycle recovery; require one current socket,
   no accumulated requests/timers and a fresh authoritative snapshot on resume.
4. Add a stable desktop client. Repeat sweeps, full preset loads and blackout
   ON/edit/OFF. Require correct restored values, read-only web controls and
   unchanged physical DMX. Compare per-session send delay and whether the other
   client's delivery pauses during a slow send. Shared-task fairness is not yet
   redesigned or declared solved.
5. Manually capture /status and /ws-diagnostics before and immediately after a
   stall/recovery: generation, last/first closure, io_op/send_us, timing, queue
   counters, health classification, request outcome, HTTP rejection buckets/
   timestamp/route class, minimum heap, stack HWM and DMX TX errors. Capture the
   browser Network request status/path if the new HTTP counters increment.
6. Run20-30min with periodic activity, idle periods and lifecycle changes. Retain
   any unexpected protocol classification or prolonged CLOSING state as evidence.

Remaining uncertainty: the precise RF/TCP/ACK/window or scheduling trigger for
the518ms stall and the historical three HTTP rejection reasons. The local guard
prevents the proven sub-ms unlimited-wait bug, not every possible blocking-call
overrun. Major delivery/coalescing work stays suspended pending separate review.
No commit, flash/upload, push, merge, tag, release, erase or NVS operation occurred.


## SoftAP station count investigation (2026-10-26)

Historical investigation status: temporary bounded USB instrumentation. Later
physical validation confirmed the unexpected station was an Apple Watch; no
firmware counting defect was demonstrated. Detailed trace is now diagnostics-
build-only (see final review below). No counting correction was necessary. All previous uncommitted Stage6B.2
WebSocket health/reconnect work is preserved. The physical display and frontend
layouts are unchanged. No Wi-Fi association behavior was changed to alter a count.

### User-reported hardware evidence

- Cold boot0, iPhone joins1; iOS Settings Wi-Fi OFF produces1->0->1 and can leave
  the physical CoreS3SE screen at1 with all known clients' Wi-Fi disabled.
- Desktop connection/disconnection returns the physical count to0 and it stays0.
  This narrows the trigger in the tested setup; it does NOT establish an iOS bug.
- Software reset and full power cycle sometimes differ. Both reset the firmware
  RAM count; no count is stored in NVS. Radio/client reconnection histories and
  event ordering may differ; the actual cause requires captures from each case.
- Desktop+iPhone run: application history recorded event4 with counts1,2,3 at
  uptimes14254,24356,31431ms. No known client setting changed when3 appeared.
  wifi_disconnects stayed0; WS delivery was1020/1020 with two receiving sessions.

These are user observations, not results of the new diagnostic build.

### Exact existing data path

`src/softap_policy.cpp:Stations::connect/disconnect` maintains a bounded FULL-MAC
set, not an increment/decrement accumulator. Duplicate connects cannot add an
entry; duplicate/unknown disconnects cannot underflow. Capacity remains four.
The full MACs remain internal, as they did before this investigation.

`network_service.cpp:stationEvent/publishStations` updates that set for AP station
connect/disconnect events and clears it on AP_STOP. publishStations copies its
cardinality into shared.clients and changes shared.ui_revision only when the
count changes. AP_START is now observed for diagnostics/cycle counters only;
it does NOT publish a new current count or change the original count generation.

The READY worker already called esp_wifi_ap_get_sta_list once per second. It
captures station_generation before the call and applies the driver list only if
no count-changing callback intervened. It then clears/rebuilds the MAC set and
publishes its cardinality. The extracted `sampleStations` helper preserves that
query cadence, stale-query guard, MAC deduplication and failure/range policy.
There is still only one native station-list call site. No query runs in the
application/DMX loop, HTTP handler or Wi-Fi event callback.

The physical Settings screen (`view_sender.h:renderSettingsCanvas`, near line1991)
reads network_service::status() and prints network.clients. The actual current
source calls this field `Clients N/4`; the user describes it as Stations.
serviceSettingsNetwork near2198 detects ui_revision changes and requests redraw
when the count row is visible. `/status` similarly snapshots shared and emits
s.clients as top-level stations. Neither uses WebSocket session count, DHCP lease
count nor browser cache. A renderer can still lag its model: keep the row fully
visible and compare USB app= with the physical pixels before assuming a data bug.

[Installed-version API documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/network/esp_wifi.html#_CPPv424esp_wifi_ap_get_sta_listP15wifi_sta_list_t)
describes esp_wifi_ap_get_sta_list as the driver's associated-station list. An
associated MAC record is not proof of an awake physical client, valid DHCP lease
or active WS session. Two WebSocket sessions do not prove only two associations.
If the native driver retains an association, the application must not force0.

### Meaning of the additional event4 timestamps

`worker` selects `record(... ? (s.clients ? 4 : 5) : ...)` AFTER its station query.
Application history event4 means a count change whose new value is nonzero;
event5 means a change to zero. It is NOT WIFI_EVENT_AP_STACONNECTED, not an
association ID and not a command or DMX event. Its timestamp comes from the
subsequent application memory sample. In particular,2->3 is also event4.

| History time | Application count | What can be established |
| --- | ---: | --- |
| 14254ms | 1 | Nonzero count changed; preceding native query succeeded and passed the configured range check |
| 24356ms | 2 | Same; does not identify whether a callback or accepted reconciliation changed it |
| 31431ms | 3 | Same; cannot identify a third live device or an actual connect event from history alone |

The previous history stored no list/MACs, connect event counter, connect timestamp
or query-generation acceptance flag. Therefore historical raw associations and
native counts at those exact times CANNOT be reconstructed from those samples.
A successful query could have been skipped for publication if an event arrived
during it. No nonexistent native-list result is inferred from the memory sample.

wifi_disconnects counts cumulative AP station-disconnect CALLBACKS, including
duplicates; it does not represent current stations, TCP loss or WS closures.
Zero means no such callback was recorded in that boot interval, not that only
two devices existed. The source rules out arithmetic inflation from a duplicate
connect for the SAME full MAC. Different MACs, retained event entries, native
association records or delayed event ordering still require evidence. One device
can use different MACs over time; no OS/physical-device identity is inferred.

### Added instrumentation and interpretation

The existing ESP-IDF ESP_LOGW USB console path emits AP_STATIONS from the network
worker only. No Arduino Serial input/output setup or HWCDC initialization was
added. Event callbacks just update fixed metadata under the existing mux.
No formatting, printing, native query, heap allocation or M5 call is done there.

A change can emit at most one line per second. An unchanged state emits one fresh
sample every30 seconds so the comparison remains observable without HTTP or
connected Wi-Fi clients. Worker teardown may emit one additional final/error
line. Fields are bounded; there is no heap-backed history or per-packet logging.

Example (ILLUSTRATIVE, not hardware evidence):

```text
AP_STATIONS ms=32000 app=1 drv_last=1 rc=0 query=1 use=1 qms=32000 gen=2/2 ev=disconnect ems=31000 eapp=0 id=4455 C=1 D=1 starts=1 stops=0 cycle=1/1 ap=start ams=1200 ids=4455
```

- app: current shared.clients, the source used by physical UI and /status.
- drv_last/rc/qms: most recent native query count, exact esp_err_t, completion ms.
  drv_last=-1 means query failed/not available; it is NOT a measured zero.
- query=1: a native query attempt has been recorded since the last AP event;
  it does not itself prove success. query=0 marks older/not-yet-queried data.
- use=1: query succeeded, within configured count bounds, with matching before/
  after station generations and was applied to the set. use=0: not applied.
- gen: count-generation immediately before query and when publishing its result.
- ev/ems/eapp/id: latest station connect/disconnect callback, dispatch ms, count
  immediately AFTER it, and ONLY its last two MAC bytes. AP lifecycle events do
  not overwrite that last station-event evidence. id=---- means none available.
- C/D: saturating cumulative connect/disconnect callback counts since firmware
  boot, intentionally including duplicate notifications. They never drive app.
- starts/stops and ap/ams: AP lifecycle events/callback timestamps. cycle=C/D
  resets on AP_START; cumulative boot C/D and existing wifi_disconnects do not.
- ids: sorted final-two-byte tags for the last native list, at most the configured
  capacity. Sorting affects diagnostics only, not private-set reconciliation.

The SDK timestamps here are callback dispatch/query completion times, not RF
packet-arrival times. Memory history timestamps can follow printing/query work
and need not exactly equal qms. Tags are16-bit correlation hints, not unique device
identifiers. Complete MACs, OUI prefixes, SSIDs, passwords and raw packet data are
not printed by this instrumentation, nor are these tags exposed by a new HTTP API.

Decisive patterns for the next run:

| Pattern | Interpretation |
| --- | --- |
| ev=disconnect, eapp=0; later app=1 drv_last=1 rc=0 query=1 use=1, C unchanged | Accepted native query restored1; it was not an extra connect callback or increment bug |
| C increases and a new id appears before app increases | An actual SDK connect notification was dispatched; confirm the same tag in an accepted native list |
| app=3 drv_last=3 query=1 use=1 with three native records | Driver reports three associated MAC records; application is reflecting that list, not proving three awake physical devices |
| app differs from drv_last, query=1 rc=0 use=0 and gen differs | Query lost the event race; the newer callback state was preserved. Check the next accepted query |
| app=0 drv_last=0 query=1 use=1 repeatedly, but fully visible physical row remains1 | Investigate physical redraw or flashed-build mismatch; do not attribute it to driver associations |
| rc nonzero/use=0 | Query failure is explicit; existing worker error teardown follows, not a fabricated zero-success sample |

No stale accounting defect is confirmed yet. The smallest authorized step is
observation of this existing hybrid count path. No inactivity timeout, association
limit, Wi-Fi mode/channel, deauthentication, event filter for client events,
password, DHCP option, WS/session policy or current-count rule was altered.

### Focused regression tests

`tests/network/test_station_diagnostics.cpp` compiles the ACTUAL production
station-event, query/reconciliation and USB-formatting code extracted by
check_softap_integration.py. SDK lists/events, time and console are mocked.
Tests cover cold0, start/connect/disconnect, duplicates, delayed callbacks,
reconciliation to native0/1, a callback DURING query, sorted/truncated IDs,
replacement/extra native MAC entries, overflow/error returns, AP stop/start,
cycle reset versus cumulative counts, fresh reset state,1Hz change/30s steady
logging and final teardown output. Capacities1/4/8 are exercised; firmware remains4.

An explicit three-count scenario uses14.254/24.356/31.431s as synthetic timestamps:
a duplicate notification does not create a third set entry; a subsequent native
list containing three entries DOES publish3 without a new callback. This is a
source-level distinction, not a reconstruction of the real hardware timeline.

Native query errors propagate to the original worker check/break/teardown path.
The helper does not clear a current count merely because the API failed. AP_STOP
and the original teardown clear current count; boot totals reset on MCU reboot,
cycle totals on AP_START. These tests do not establish RF association correctness.

### Hardware procedure (no HTTP connection required)

After review, upload manually and capture through the existing USB console:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
pio device monitor -e cores3se_unit --port COM8 --baud 115200 --filter log2file
```

Monitor command options were checked against the installed PlatformIO CLI.
Open the monitor before a physical reset to capture the boot and first native
sample. Do not send a diagnostic command, initialize HWCDC, erase flash, reset
Wi-Fi credentials or clear presets. log2file reports its capture path; retain the
AP_STATIONS lines with monotonic ms and note which physical row is visible.

1. All known client Wi-Fi off: cold boot and wait at least60s. Require app/native0
   if the driver really has no associations; collect the30s steady samples.
2. Desktop alone: join, disconnect, keep Wi-Fi disabled and wait60s. Capture all
   connect/disconnect lines and verify the driver query, not just the screen.
3. Fresh iPhone-only run: join, note its truncated tag, disable Wi-Fi in SETTINGS,
   leave it disabled and capture90s. If0->1 returns, determine whether C increased
   or only an accepted native query restored1. Do not assume iOS caused the defect.
4. Desktop+iPhone: reproduce1->2->3 without changing known settings. Compare each
   app increase to ems/eapp/C, qms/use/gen and the native tags. Record other possible
   clients' actual state; keep station MAC records distinct from two WS sessions.
5. Repeat software Restart and full cold power-on with the SAME client Wi-Fi state.
   No count is persisted; compare boot/cycle events and native samples. Do not
   alter Wi-Fi association policy to make the number match an expectation.
6. Keep the physical count row fully visible; compare its value with app= at nearby
   timestamps. Optional /status can be captured only when a client is intentionally
   rejoined; that new association itself changes the count and is not an offline test.

If the native driver repeatedly returns1 or3, retain that evidence rather than
forcing0. Determining whether those records correspond to currently reachable
stations versus retained driver associations may need a subsequent driver/RF
investigation. No such conclusion or association-policy change is made here.


### Station investigation validation and measured resource deltas

PASS: all30 frontend live groups and existing navigation/foundation tests; all
16 existing compiled host cases plus3 new station cases (capacities1,4,8); eight
asset-pipeline cases, six infrastructure fixtures, native protocol/Dispatcher,
startup/PSRAM/SoftAP integration, transport patch-state and whitespace checks.
The new cases compile extracted production code, not an independent counting
implementation. The firmware capacity is unchanged at4 in every profile.

| Environment | Result | Static RAM | Flash | RAM delta vs preceding health build | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87604 | 1059241 | +96 | +2360 |
| cores3se_base | PASS | 87604 | 1059209 | +96 | +2360 |
| cores3se_unit_diagnostics | PASS | 90220 | 1061441 | +96 | +2388 |
| cores3se_base_diagnostics | PASS | 90220 | 1061413 | +96 | +2392 |

The station diagnostic structure is76B at capacity4 (also verified by host code),
plus revision/log timestamps and target alignment:96B additional static RAM.
Temporary formatting/snapshot/query metadata uses bounded worker stack space.
No task, heap-backed event history, packet buffer, DMA/PSRAM allocation or extra
Wi-Fi query was introduced. Native list enumeration is the existing1Hz call.
The 30s steady line and rate-limited change lines use the existing console sink;
logging can add worker latency, so zero timing impact is NOT claimed. Actual
worker stack HWM, heap and delivery responsiveness need physical verification.

The browser modules, all three gzip assets/manifest, WS framing/protocol/health
checks, physical UI, Controller/Dispatcher/DMX code, AP configuration, DHCP router
suppression, credentials and NVS format are byte-for-byte unchanged from the
start of this investigation. No asset regeneration was necessary. Gzip sizes
remain HTML2904B, CSS3521B and JS9258B, all below16KiB. The same16MiB layout and
Stage3E reproducible driver patch remain intact.

Final source SHA-256: `82d0e01090f84429d9bddfb0638615f759a0ce0859b1625c495c668c8b67d571`.
Unchanged asset bundle SHA-256: `f3e3f058f7b397f292bc5fe68b1fca28faad35e4f77fd645756bff559df511af`.

Exact files changed in this station investigation:

```text
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
src/network_service.cpp
tests/network/check_softap_integration.py
tests/web/README.md
tests/web/check_scope.py
tests/network/test_station_diagnostics.cpp
```

Only network_service.cpp changes production firmware; the other changes are the
source-extraction/test wiring, scope documentation and this report. Existing
historical files, logs, backups and stash remain intact. The full Stage6B.2 work
is still uncommitted/unstaged on codex/stage6b-web-ui at the unchanged approved
HEAD2c69aa541dc404c2cef3deb821265ee7d45f26b3. No firmware upload, erase/NVS change,
commit, push, merge, tag or release occurred. Stop here for actual USB/display
hardware correlation; neither an iOS defect nor a third live station is proven.


## Final UI polish, diagnostic cleanup and review (2026-10-26)

### User-confirmed hardware baseline

The user successfully validated the preceding Stage6B.2 firmware on a real
CoreS3SE + Unit DMX with iPhone, Windows desktop and MacBook clients:

- iPhone/MacBook responsive and smooth; slightly less smooth Windows behavior is
  treated as a client/environment issue for now, not justification for firmware
  timing changes. No speculative Windows-specific correction was introduced.
- Two-client and three-client operation succeeded. Latest test sent2325 updates,
  outbox high-water3/8, zero send failures, slow closes, queue overflow, station
  disconnects and DMX TX errors. Do not generalize this to untested four-client
  capacity or all network conditions.
- Three real SoftAP stations were confirmed. An Apple Watch automatically joining
  accounted for the previously unexpected station. Application/native counts
  agreed; no firmware count bug was demonstrated. Earlier ambiguity and test
  timelines are retained above as investigation history, not current claims.
- A separate receive-side io_failure at690240ms/gen5 remains recorded and analyzed
  below. Zero TX/queue failures does not mean that record was erased or ignored.

This hardware acceptance predates the final one-line presentation change and
compile-time trace cleanup. The final artifacts were not uploaded in this task.
The historical518ms synchronous ESP32 send stall remains a potential limitation;
this successful run does not establish a permanent transport fix.

### Silent routine health-check presentation

Only `web/src/js/app.js:render` changes authored frontend behavior: the main
connection label now uses the existing lifecycle phase directly. A ready LIVE
connection remains Connected / Live throughout a routine pending health probe.
The existing LIVE color/tag and approved layout/style are unchanged. Status
continues to show Snapshot synchronized / checking connection while pending.

No transport/state/protocol code was changed. The15s inactivity trigger,10s
matching-snapshot deadline, delayed-timer grace, one pending request/timer,
generation invalidation, initial full snapshot, revision rules, reconnect/backoff
and manual Retry remain byte-for-byte as at the start of this task. A timeout or
observed socket failure still marks ready=false and cached state stale immediately
in the state machine; the existing next animation-frame projection shows
Reconnecting / OFFLINE. It never substitutes a successful probe for a complete
matching authoritative snapshot or replays uncertain mutations.

Expanded actual-app frontend tests check: main label/LIVE tag/color unchanged
while a probe is pending; Status alone shows pending; a response after9000ms
leaves the main indicator LIVE; a subsequent unanswered probe at the unchanged
10000ms deadline clears LIVE and shows stale/reconnecting/Not synchronized.
Navigation, focus, responsive geometry and disabled mutation controls are preserved.

### Detailed AP trace removed from normal production execution

`network_service.cpp` includes the existing hardware_diagnostics.h definition of
DMX_HW_DIAGNOSTICS (default0). The detailed station state/revisions, event metadata,
MAC-tail copies/sort, change detection, timing, formatting, periodic/teardown
logging and associated calls are all inside compile-time guards. Merely lowering
the log level would leave overhead; that approach was not used.

- cores3se_unit / cores3se_base: no AP_STATIONS marker or station trace state in
  the firmware images/symbol tables; no detailed trace work or console output.
- cores3se_unit_diagnostics / cores3se_base_diagnostics: retain existing bounded
  station observations, including1Hz maximum change lines and30s steady samples.
  No new PlatformIO environment or custom runtime switch was added.

The current station MAC set, callback updates, AP_STOP clear, generation guard,
existing1Hz esp_wifi_ap_get_sta_list query and UI/publication behavior are unchanged.
Original lightweight wifi_disconnects and last_wifi_ms counters remain in normal
builds, along with useful WS/status diagnostics. There is no change to limits,
passwords, channel, inactivity/association policy, DHCP router suppression or NVS.
No Serial/HWCDC initialization was introduced. Diagnostic environments inherit
the established Stage0 diagnostics behavior; no new USB path was added here.

The production host variants execute500 steady queries with one native call per
query, no additional diagnostic clock reads and no console formatting/log calls.
The detailed station scenarios still run at capacities1/4/8 with the flag enabled.
The actual firmware capacity remains4. A source-only toggle cannot accidentally
leave periodic detailed logs or MAC-tag collection active in production artifacts:
post-build binary/symbol checks explicitly verify their absence/presence.

### io_failure / source io / io_op4 / errno0 finding

Observed record supplied by the user:690240ms, generation5, reason io_failure,
source io, io_op4, rc-1, errno0, send_failures0, slow_closes0.

Exact application source path:

1. `network_service.cpp:io` sets io_operation=writing?3:4 immediately before
   send/recv. Therefore io_op4 is the receive-side `recv()` call, NOT a send,
   queue-full close, explicit application deadline check (op1) or setsockopt (op2).
2. The n<=0 branch calls ioFailure(io_failure,n<0?errno:0). With ordinary socket
   semantics, recv returning0 is TCP EOF and intentionally records errno0,
   regardless of an unrelated stale errno value. Negative receive errors preserve
   their errno. The sample stores no raw n/offset, so a negative result with an
   unset errno or earlier retained I/O context cannot be categorically excluded.
3. `handlerFailure` records source io when s.io_reason exists. rc-1 is the HTTP/WS
   receive routine's ESP_FAIL result, not proof that the underlying recv returned-1.
4. A complete received WebSocket CLOSE takes the existing separate peer_close path
   after successful frame parsing/reply. EOF without a complete CLOSE does not.

An ordinary TCP peer departure without a completed WS closing handshake CAN
produce precisely this tuple. IDF4.4.7 `httpd_ws_get_frame_type` treats a failed/
zero first-byte read as a synthetic CLOSE and returns ESP_OK. Because this server
handles control frames itself, its user callback then asks httpd_ws_recv_frame
for the frame details; reading the next header byte can EOF again and return
ESP_FAIL. The application records io_failure/source io/op4/errno0. There is no
server send attempt required, so zero send_failures/slow_closes is consistent.

References: [IDF4.4.7 frame-type handling](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_ws.c#L399)
and [pinned lwIP TCP receive path](https://github.com/espressif/esp-lwip/blob/a45be9e438f6cf9c54ec150581819c3b95d5af6b/src/api/sockets.c)
(the audited local copies retain their source hashes in earlier validation folders).
This explains a valid execution path, not the client's actual action at690240ms.

EOF can also arrive in a truncated header, mask or payload. The available record
cannot identify that boundary, prove an orderly WebSocket close, identify the
client, attribute it to a lifecycle/user action, or establish an unexpected
server-side defect. If it repeats during uninterrupted active use, retain timing
and browser close/probe information for further investigation. A capture of raw
recv outcome/frame progress or TCP traffic would be needed for stronger attribution.

No error suppression, reclassification, reset of first failure, receive-success
fabrication, bridge change, retry-policy change or counter cleanup was made.
Existing strict complete-frame and close-on-failure behavior stays intact.
Added extracted-production I/O tests model EOF with a synthetic CLOSE/read failure,
actual negative reset/timeout errors preserving errno, and a full explicit CLOSE
that still records peer_close. EOF tests reproduce op4/errno0/rc-1/source io with
zero send failures; reset/timeout cases remain genuine logged failures.

### Final automated validation

All30 frontend live groups plus navigation/foundation coverage pass, including
silent-probe success and timeout presentation. All21 compiled host cases pass:
the16 existing cases, three detailed station variants and two production station
variants. Network/native WS endpoint/Dispatcher, SoftAP, PSRAM/canvas/startup,
transport ISR/fault/patch-state and infrastructure regressions pass. Eight asset
pipeline cases and six infrastructure fixtures pass. Deterministic composer/gzip,
content hashes/manifest,16KiB per-asset cap and whitespace checks pass.

All four builds use the same final source and asset fingerprints. Normal binaries
omit AP_STATIONS and all detailed station trace state symbols; diagnostic binaries
retain them. No queue size, send/receive timeout, task architecture, controller
ownership, blackout restoration, preset data, physical UI or partition changed.


### Final build and resource measurements

Deltas below are against the immediately preceding hardware-validated station
investigation build, not a different release/library configuration.

| Environment | Result | Static RAM | RAM delta | Flash | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87508 | -96 | 1056893 | -2348 |
| cores3se_base | PASS | 87508 | -96 | 1056861 | -2348 |
| cores3se_unit_diagnostics | PASS | 90220 | +0 | 1061425 | -16 |
| cores3se_base_diagnostics | PASS | 90220 | +0 | 1061397 | -16 |

Normal builds recover the96B detailed station trace allocation, including its
revision/timestamp state; they add no replacement diagnostic allocation or task.
Diagnostics builds retain the previous detailed station state and bounded logging.
No queue, packet buffer, DMA/PSRAM allocation or existing memory guard was changed.
These are linker measurements; actual runtime heap/stack behavior of this final
revision was not measured on hardware. Both profiles retain the16MiB/two6MiB-slot
layout and the same reproducible Stage3E driver patch.

| Asset | Raw bytes | Gzip bytes |
| --- | ---: | ---: |
| index.html | 10199 | 2905 |
| style.css | 12397 | 3521 |
| app.js | 33133 | 9246 |

The one-line authored JS change removes71 raw bytes and12 gzip bytes. Rendered
HTML gzip changes+1B because its content-hashed JS URL changes; CSS is unchanged.
Total gzip15672B,11B below the preceding build; every individual asset is below
16384B. There are still exactly three offline assets, no framework/CDN/runtime
loader/filesystem asset changes, and no partition or firmware packaging change.

Final source SHA-256: `e074eafb978b5f49beb960c3e23da76a99f93233bbff0b54c443e41adad5e2c7`.
Asset bundle SHA-256: `b24377451cbc477f53e5e5104456e0b7b4d76b05950233672e5cba9514a31551`.

### Exact files modified in this final-review pass

No new production module or environment was added. These existing Stage6B.2 work
files changed; all other previous/historical/unrelated work is preserved:

```text
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
src/network_service.cpp
tests/network/check_softap_integration.py
tests/network/test_station_diagnostics.cpp
tests/network/test_websocket_adapter.cpp
tests/web/README.md
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/src/app.js
web/src/js/app.js
```

### Remaining limitations and review gate

- Hardware acceptance supports the tested Unit DMX baseline and two/three-client
  scenarios; the final presentation/trace cleanup itself has not been flashed.
  DMX Base remains experimental; no new electrical waveform claims are made.
- Native synchronous sends may exceed their nominal deadline and delay the shared
  HTTP task. The prior sub-ms zero-timeout guard and foreground health recovery
  address specific risks; they are NOT a proven permanent send-stall correction.
- Browser/OS suspension and native CLOSING can delay reconnect. The existing
  bounded probe/backoff and lifecycle recovery remain intact without overlapping
  native connections. Four-client hardware capacity is not newly validated here.
- The receive-side io_failure can be EOF on peer departure or an incomplete frame;
  its recorded tuple cannot prove the user's/client's actual action. Genuine
  errors and the immutable first-failure record remain visible, not reclassified.
- Browser DMX/blackout/preset mutations remain disabled; Stage6B.3 was not started.

Final review should confirm normal firmware stays silent for AP_STATIONS, Status
can display a pending probe without main-indicator flicker, and a forced loss still
clears LIVE until a fresh matching snapshot. The diagnostics environments remain
available for bounded station evidence. No source/API/configuration change is
needed merely because the Apple Watch accounted for the third station.

Everything remains uncommitted and unstaged on codex/stage6b-web-ui with unchanged
HEAD2c69aa541dc404c2cef3deb821265ee7d45f26b3. Existing stash, historical development
folder, logs/backups and unrelated files are preserved. No commit, upload/flash,
push, merge, tag, release or NVS/flash erase occurred. Stop for final user review
before Stage6B.2 closure.


## Final iOS bank-arrow touch fix and conditional closure (2026-10-26)

The user confirms the preceding final firmware smoke test passed: routine probes
no longer flicker the main LIVE indicator, normal production firmware emits no
periodic AP_STATIONS lines, and the accepted multi-client features still work.
The bank-arrow gesture rule subsequently passed the final physical iPhone check
(see the closure acceptance below).

### Narrow patch

Actual controls are #bank-previous and #bank-next in web/src/index.html. Both are
native buttons with class icon-button, within the existing bank-buttons wrapper.
Their existing editor.js click listeners synchronously move the browser-local
channel bank; neither element is replaced during bank rendering.

The ONLY production change in this pass is one stylesheet rule beside the
existing bank control styles:

```css
.bank-buttons .icon-button { touch-action: manipulation; }
```

The rule scopes to those controls, not body/html, the scrolling shell or other
buttons. It adds no dimensions, animation, styling or JavaScript. The viewport
meta tag remains unchanged, without user-scalable=no/maximum-scale. No global
touch-action:none, broad preventDefault handler or custom touch gesture was added.
Bank math, bound disabling, local preferences, channel values and event semantics
are byte-for-byte unchanged. All C++/network/DMX source is unchanged in this pass.

[WebKit's documented behavior](https://webkit.org/blog/5610/more-responsive-tapping-on-ios/)
allows panning and pinch zoom for touches beginning on manipulation controls,
while excluding their double-tap gestures. This is also covered by the
[Pointer Events touch-action specification](https://www.w3.org/TR/pointerevents3/#the-touch-action-css-property).
Pinch-to-zoom remains permitted, including elsewhere in the interface. Browser
version/gesture behavior must still be verified on the target iPhone; desktop or
host mocks cannot prove native Safari zoom suppression. No fallback is deployed
without evidence. If this narrow rule fails, first verify the newly hashed CSS
was loaded and establish the exact iOS mode/version/target before considering a
targeted fallback and its accessibility tradeoffs. Do not apply global restrictions.

### Automated validation and full Stage6B.2 review

-31 live integration groups plus navigation/foundation tests PASS. Added actual-
 editor repeated-forward/back and100 alternating pairs at4/6/8 bank widths,
 first/last bounds, unchanged512 authoritative channel values and zero outgoing
 device requests. These model click delivery, not native Safari gestures.
-Nine pipeline cases PASS, including the exact scoped CSS rule, actual arrow
 markup/class match, unchanged viewport zoom restrictions and absence of touch
 gesture handlers in editor.js. Deterministic gzip/composition/hash/manifest and
 the16384B per-asset cap PASS. No frontend JS source or bundle changed.
-Existing native/network/Dispatcher/SoftAP/canvas/startup/transport patch-state
 and infrastructure regressions PASS; all21 compiled host cases PASS against
 the final built assets. Git whitespace checks include new untracked sources.
-All four firmware builds PASS. Normal AP trace remains compiled out, detailed
 trace retained in diagnostics builds, and all hardware-profile/preset/blackout/
 controller behavior stays unchanged.

The complete tracked diff against approved6B.1 HEAD was saved and reviewed with
its33-file intended inventory. Authored modules, bounded networking diagnostics,
the tested sub-ms timeout guard and production trace gates match the previous
reviewed/hardware-tested implementation; generated JS/gzip are validated against
authored source rather than treated as unrelated manual edits. No unexpected
source, configuration, release packaging or historical file was included.
Historical untracked development files, the unrelated Stage3 report, existing
logs/backups and stash are excluded from the candidate commit and preserved.
The complete candidate file list follows:

```text
docs/STAGE6B2_LIVE_WEBSOCKET_STATE.md
include/websocket_diagnostics.h
scripts/web_ui_bundle.py
src/network_service.cpp
src/websocket_diagnostics.cpp
tests/network/check_softap_integration.py
tests/network/check_websocket_integration.py
tests/network/test_station_diagnostics.cpp
tests/network/test_websocket.cpp
tests/network/test_websocket_adapter.cpp
tests/web/README.md
tests/web/check_pipeline.py
tests/web/check_scope.py
tests/web/fixture_server.py
tests/web/run_tests.mjs
tests/web/test_live.mjs
web/generated/app.js.gz
web/generated/index.html.gz
web/generated/manifest.json
web/generated/style.css.gz
web/src/app.js
web/src/index.html
web/src/js/app.js
web/src/js/commands.js
web/src/js/editor.js
web/src/js/fader.js
web/src/js/presets.js
web/src/js/protocol.js
web/src/js/shell.js
web/src/js/state.js
web/src/js/status.js
web/src/js/transport.js
web/src/style.css
```

### Final touch-fix resources

| Environment | Result | Static RAM | Flash | Delta vs preceding smoke-tested build |
| --- | --- | ---: | ---: | ---: |
| cores3se_unit | PASS | 87508 | 1056909 | +16 flash / 0 RAM |
| cores3se_base | PASS | 87508 | 1056877 | +16 flash / 0 RAM |
| cores3se_unit_diagnostics | PASS | 90220 | 1061441 | +16 flash / 0 RAM |
| cores3se_base_diagnostics | PASS | 90220 | 1061413 | +16 flash / 0 RAM |

| Asset | Raw bytes | Gzip bytes |
| --- | ---: | ---: |
| index.html | 10199 | 2903 |
| style.css | 12456 | 3540 |
| app.js | 33133 | 9246 |

CSS increases59 raw /19 gzip bytes; HTML gzip decreases2B because its stylesheet
content-hash URL changes; JS is unchanged. Total gzip15689B (+17B), each asset
below16KiB. Static RAM is unchanged, and flash increases16B in each profile.
No task, buffer, protocol, partition, NVS or firmware packaging change occurred.

Final source SHA-256: `c87db1e1feaee3d2310e641a9edc443bb0b0bf86c74a22c4c04102aea0ff5123`.
Asset bundle SHA-256: `85e1ad4af03714ebac56d6457f40d6dfe337f9461938f6a022d228f72a305256`.

### Final physical acceptance and local closure

The user confirmed: "Final physical iPhone verification PASSED. The bank selector
double-tap zoom issue is fixed." Stage 6B.2 is approved for closure. This is actual
user-reported iPhone evidence, separate from automated click/navigation tests.
No targeted JavaScript gesture fallback was required.

The user authorized one local commit containing exactly the reviewed 33 Stage
6B.2 files, with message:

```text
Stage 6B.2: Add live WebSocket state and recovery
```

The production source fingerprint, deterministic assets and complete reviewed
tracked diff were rechecked before staging; only this acceptance documentation
was updated after the successful final validation. All four successful build
receipts still match the unchanged source fingerprint. Historical/unrelated
files, logs, backups and the existing stash are preserved and excluded.

No push, merge, firmware upload/flash, tag/release, NVS erase or Stage 6B.3 work
was performed during closure. The known synchronous-send and receive-EOF
limitations above remain documented; this touch fix makes no additional
transport-safety claim.
