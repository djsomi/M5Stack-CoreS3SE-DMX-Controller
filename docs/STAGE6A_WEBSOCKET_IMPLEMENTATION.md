# Stage 6A: native WebSocket / Controller integration

## Final hardware acceptance (2026-10-25)

The user completed successful physical validation on **M5Stack CoreS3SE + Unit
DMX, macOS Chrome, two concurrent WebSocket clients**. This section supersedes
the earlier software-only acceptance and investigation status. The tested
implementation includes the bounded round-robin pump and retained diagnostics.

Clean-boot diagnostic evidence supplied by the user:

| Measurement | Result |
| --- | --- |
| Uptime | 139,798 ms |
| WS opened / closed | 2 / 0 |
| Active clients / current Controller revision | 2 / both 296 |
| Updates enqueued / taken / sent | 550 / 550 / 550 |
| Outbox high-water mark | 3 (capacity remains 8 per client) |
| Queue-full / send failures | 0 / 0 |
| Wi-Fi disconnects / work failures | 0 / 0 |
| First failure | null |

User-confirmed physical tests passed:

- Native handshake, hello and complete authoritative snapshot.
- High-frequency touchscreen slider changes and browser-to-WebSocket-to-
  Controller control of real DMX fixtures.
- Two-client bidirectional synchronization, including touchscreen changes.
- Blackout ON/OFF on both clients; channel edits while blacked out did not
  energize outputs; OFF restored the pre-blackout snapshot.
- Physical preset loading and WebSocket preset loading, each synchronizing
  all 512 channels.
- Invalid channel 513 and value 256 were rejected; connections continued
  operating after the invalid commands.

Earlier Windows failures occurred alongside severe Intel AX200 Wi-Fi instability
with **26-30% ICMP packet loss**. After that client was disconnected, the clean
macOS test reported zero network/WebSocket errors. This is a user-reported
network caveat, not proof that every previous close had the same cause. The
old pump's throughput defect was independently reproduced in host tests and
its bounded correction is retained; no speculative firmware changes are made
for the earlier Windows results. The investigation history and reproduction
remain in `STAGE6A_WEBSOCKET_DISCONNECT_INVESTIGATION.md`.

**Two concurrent WebSocket clients are hardware-validated; four are configured,
not physically validated.** Stage 5's three-station acceptance is separate from
this two-WebSocket-client result. Base remains experimental. No new heap/HWM
figures or external electrical captures were supplied with these results.

The worker still schedules HTTP pump/housekeeping work approximately every
20 ms even when idle. This relatively frequent scheduling is a possible later
optimization, **not a Stage 6A blocker**. Any future event-driven/less-frequent
idle policy must preserve queue draining, fairness, HTTP deadlines, heartbeat,
cleanup and owner-task isolation and must repeat relevant regression/hardware
tests. No scheduling refactor is included in closure.

## Status and preserved baseline

Implemented on `codex/stage6a-websocket-controller`, directly from hardware-tested
Stage 5 commit `7f09d4c3314d0444a11786aea9ba86793842c9bb`. The focused local Stage
6A closure commit contains only the communication layer, its bounded diagnostics,
tests and reports; the three embedded placeholder assets are unchanged.
Closure performs no upload, erase, merge, push, tag or release.

Stage 5 hardware acceptance remains historical evidence: three associated Wi-Fi
stations, HTTP, iPhone cellular routing, PSRAM framebuffer and local/DMX controls
worked. Stage 6A's user-performed hardware acceptance is recorded above; it is
not a new agent upload or independently collected measurement. Four station
associations and four WebSocket sessions are configured; four-client capacity
remains unverified on hardware. Base remains experimental.

No `AGENTS.md`, `PROJECT_STATUS.md` or original `wifi-architecture-proposal.md`
exists in this checkout. The actual Stage 1/2/3/3E/4/5 reports and implementation
were used. Historical untracked development, the unrelated no-receiver report,
other worktrees, ignored logs/backups/bundles and stash were preserved.

## Exact changes

| File | Responsibility |
| --- | --- |
| `include/websocket_bridge.h` | Owned request/event structures, limits, lock interface, tested bridge API |
| `src/websocket_bridge.cpp` | Fixed queues, session generations, owner-only submission/completion, synchronization |
| `src/websocket_protocol.cpp` | Strict bounded JSON decoder and allocation-free JSON encoder |
| `include/websocket_wire.h` | Incremental raw-frame guard, before native length narrowing; upgrade pipeline guard |
| `include/websocket_service.h`, `src/websocket_service.cpp` | Static bridge construction, short ESP32 critical sections, task-facing facade |
| `include/websocket_pump.h` | Bounded four-round drain policy and rotating fairness |
| `include/websocket_diagnostics.h`, `src/websocket_diagnostics.cpp` | Bounded closure/counter DTOs and JSON serializer |
| `src/network_service.cpp` | `/ws`, six HTTP sessions, native I/O, heartbeat/idle handling, pump, `/status` and `/ws-diagnostics` |
| `src/src.cpp` | Include, setup attachment, owner poll before existing Dispatcher processing: three added lines |
| `tests/network/test_websocket.cpp` | Pure protocol/bridge/Dispatcher/concurrency tests |
| `tests/network/test_websocket_adapter.cpp` | Actual extracted endpoint/I/O under native API and socket mocks |
| `tests/network/check_websocket_integration.py` | Ownership/baseline guards; extracts actual functions for mocks |
| `tests/network/run_websocket_tests.py` | Reproducible host runner writing only ignored `dist/` |
| `tests/network/ws_console.js` | Manual development-only browser-console harness |
| `tests/network/ws_disconnect_probe.js` | Passive bounded disconnect evidence collector; no reconnect or commands |
| Existing `check_integration.py`, `check_softap_integration.py` | Explicitly allow the three owner hooks, native WS, six sockets and 20 ms pump; retain exact domain/UI/driver guards |
| This report, `STAGE6A_WEBSOCKET_DISCONNECT_INVESTIGATION.md` | Final acceptance, protocol and retained investigation/reproduction history |

Controller, Dispatcher, Sender/Receiver views, transport, every driver patch,
preset store, pins, USB setup, partition CSV, dependencies, asset bytes and
packaging scripts remain identical to Stage 5. Source guards reconstruct the
older approved UI/application baseline after removing only named integration
additions. No broad safety-check exclusion was introduced.

## Native support and socket capacity

Installed Arduino ESP32 2.0.16 / IDF 4.4.7 SDK for esp32s3/qio_qspi has
`CONFIG_HTTPD_WS_SUPPORT=1` (`sdkconfig.h:342`) and
`CONFIG_LWIP_MAX_SOCKETS=16` (`:496`). Installed public `esp_http_server.h`
declares WS route fields at lines 405/411, `httpd_queue_work` at 1559,
`httpd_ws_recv_frame` at 1632, `httpd_ws_send_frame` at 1644,
`httpd_ws_send_frame_async` at 1661 and `httpd_ws_get_fd_info` at 1674.
`xtensa-esp32s3-elf-nm` confirmed defined symbols in the precompiled
`libesp_http_server.a`; firmware builds confirm linkage. No framework edits,
feature-enabling macro, new dependency, asynchronous web framework or second
HTTP server is needed.

The native server requires **max_open_sockets + 3** descriptors for sessions,
listener and control infrastructure. Six sessions plus three is below 16;
the remaining descriptor budget is not a promise of unlimited SDK headroom.
The bridge allows four WS registrations, leaving two session positions for
short HTTP requests when four are active. Unupgraded/slow HTTP sockets can
temporarily occupy capacity; their existing three-second deadline remains.
LRU eviction remains disabled. At WS capacity, upgrade is closed with no
bridge registration. Six occupied sockets can refuse further HTTP/WS clients
until cleanup. Wi-Fi stations, HTTP sockets and WS clients are separate counts.

Exact upstream sources inspected:

- [IDF 4.4.7 URI dispatch](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_uri.c#L311): native 101 precedes the application GET callback. Host/Origin/capacity failures therefore close immediately after 101, before registration or command processing; a browser may briefly fire `open` then `close`.
- [IDF 4.4.7 WebSocket implementation](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_ws.c#L257): receive uses caller storage; extended length is assigned to `size_t`. On ESP32 that narrows a 64-bit length. The sender at line 376 checks negative results, not exact byte counts. The override supplies complete writes or failure; the wire guard rejects 64-bit lengths before narrowing. Control frames are delivered explicitly to our handler.
- [IDF receive buffering](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_txrx.c#L97): pending HTTP bytes can bypass the receive override. The upgrade rejects already-received bytes after the HTTP header terminator. Normal browsers wait for 101. Post-upgrade reads pass through the guard, including split headers/masks.
- [IDF server socket reservation](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_http_server/src/httpd_main.c#L405): runtime socket-budget validation matches the compile-time check.

`httpd_ws_send_frame` was inspected; the implementation uses the confirmed
low-level `httpd_ws_send_frame_async` from the HTTP task for both queued data
and control replies. Despite its name this call performs synchronous socket
I/O. No allocating `httpd_ws_send_data*` helper is used.

## Tasks, ownership and public interfaces

```text
HTTP task: masked frame -> raw guard -> native receive -> strict JSON decode
          -> bounded copied inbox
Application loop: ownerPoll -> Dispatcher.submit -> existing process/admission
          -> Controller -> unchanged UI publisher -> safe transport
          -> Dispatcher completion -> copied state event + applied/error result
HTTP task: queued pump -> per-client outbox -> native send -> browser
Touchscreen: existing executeLocal -> same completion -> same state broadcast
```

Only `Bridge::ownerPoll`, `Bridge::state`, `Bridge::reply` and its Dispatcher
completion callback access Controller/Dispatcher, on the existing application
task. `submit()` is **not** called from a network task. Setup installs the
previously unused Dispatcher completion callback. The existing UI Controller
observer, selected-channel synchronization, transport admission and publisher
are untouched. No second observer, Controller cache or mutation engine exists.

The facade exposes initialize/ownerPoll to the owner, and open/close/alive,
receive/take/sendFailed/metrics to networking. A session token is slot plus a
monotonic generation, never an fd or producer pointer. Generation exhaustion
refuses new clients rather than reusing a generation. Stale requests, results
and old close callbacks cannot address a reconnected client.

The ESP32 critical section covers bounded message/counter copies only. JSON
parsing/formatting, Controller calls, preset/NVS access and socket work happen
outside it. No application-task socket call, server work submission, wait,
delay or network task creation was added. The HTTP worker posts at most one
pending pump every 20 ms; station reconciliation, memory/health checks and
sampling remain 1 Hz. HTTP server and worker remain core 0, with their existing
8,192-byte stacks. The application owner and existing driver ISR stay separate.

## Version 1 protocol

Endpoint: `ws://192.168.4.1/ws`. Text JSON only. Request objects are flat,
maximum **256 wire payload bytes**, explicit `v:1`, and nonzero unsigned 32-bit
`id`. IDs strictly increase per connection; reconnect before wrapping.
No duplicate, unknown or command-inappropriate fields. Integers use unsigned
decimal JSON syntax, no leading zeros, signs, floats or exponents. Keys/types
are literal ASCII, no string escapes, nesting or arrays. JSON whitespace is
accepted. These restrictions describe a small command schema, not general JSON.

| Request | Exact fields beyond `v`, `id`, `type` | Dispatcher mapping |
| --- | --- | --- |
| `state.request` | none | Owner snapshot, no mutation |
| `channel.set` | `channel`: 1..512, `value`: 0..255 | `set_channel`, source external |
| `blackout.set` | `active`: JSON boolean | `set_blackout` |
| `preset.load` | `slot`: 1..8 | `load_preset`, converted to zero-based slot |
| `preset.save` | `slot`: 1..8 | `save_preset`, existing overwrite semantics |
| `preset.clear` | `slot`: 1..8 | `clear_preset`, existing logical-cleared mask |
| `preset.copy` | `slot`, `target`: 1..8 | `copy_preset`, existing occupied/corrupt protections |

No arbitrary method calls, receiver control, remote reboot/power, Wi-Fi reset,
credential changes, reset_sender, relative adjustment or patch-array command is
exposed. Additional UI functions remain future work.

Examples:

```json
{"v":1,"id":1,"type":"channel.set","channel":12,"value":180}
{"v":1,"id":2,"type":"blackout.set","active":true}
{"v":1,"id":3,"type":"state.request"}
```

Every server message includes `v`, `type`, `id`, `revision`, `seq`. Revision and
sequence are **decimal strings** to preserve 64-bit precision in JavaScript.
Request IDs remain JSON numbers. ID 0 denotes unsolicited/initial messages,
or a malformed request whose ID could not safely be parsed.

- `hello`: protocol version, 512 channels, message/client/outstanding limits.
- `state.snapshot`: 512-element `values` (channels 1..512), `blackout`, `sender`,
  eight-element `presets`. Preset states are 0 missing/logically cleared,
  1 valid, 2 corrupt, 3 storage/metadata error. DMX start code is implicitly zero.
- `state.update`: for single-channel changes, `channel`, `value`, `blackout`,
  `sender`. Bulk/preset/blackout/reset changes include the full `values` and
  `presets` arrays. Mode-only updates contain `blackout`/`sender`. Ordinary
  slider changes do not copy/broadcast 512 values.
- `command.ack`: only actual Dispatcher `applied` or `no_change`, with final
  revision and `status`, `domain`. It is **not** a queued/admission acknowledgement
  or proof that a frame reached a fixture.
- `command.error`: `invalid`, `busy`, `unsupported_mode`, `rejected`,
  `storage_error` or `transport_error`. Pre-owner errors carry `reason` such as
  `invalid`, `version`, `type`, `sync_pending`, `id_order`, `backpressure` and
  revision/seq 0 (no authoritative state observation). Applied-result errors
  carry the Controller revision. Native framing failures close the connection
  instead of trying to parse/report an untrusted payload.

`domain` on applied results is the existing Controller result code:
0 ok, 1 invalid_channel, 2 invalid_value, 3 invalid_slot, 4 empty, 5 same_slot,
6 occupied, 7 corrupt, 8 read_failed, 9 write_failed, 10 metadata_failed,
11 invalid_patch. No storage transaction/rollback guarantee was added.
If a transport publication fails after a domain change, state is published
before `transport_error`; the client must not assume that error rolled back
the Controller. A rejected save can retain existing domain/NVS partial-write
semantics; the adapter never masks a storage error.

## Synchronization and concurrent clients

1. Native handshake, policy validation, new generation and initial-sync request.
2. On owner task, capture hello and full authoritative snapshot; enqueue both
   before marking the client ready for broadcasts.
3. Controller mutations cannot interleave this owner-task snapshot boundary.
   Changes before it are in the snapshot; changes after it are queued behind
   it. HTTP only drains copies and never reads live Controller data.
4. Each subsequent completed command that changed revision emits an update,
   including touch/system-originated commands and commands processed by
   `executeLocal` while draining older Dispatcher work.
5. A command result follows its state update in the submitting client's outbox.
   No-change commands acknowledge with the unchanged revision.
6. `state.request` enqueues an ordered fresh snapshot, with that request ID.
   It returns the snapshot, not a second command.ack.

`revision` belongs to the Controller, not the bridge. `seq` orders owner-produced
messages and may have gaps for a client (other clients' snapshots/results).
State events must have increasing seq and nondecreasing revision, including
mode-only events. Do not require contiguous seq, apply optimistic values or
compare pre-owner error seq 0 as a state event. Reconnect/reboot starts a fresh
session: discard the old synchronization baseline and wait for a new snapshot.

There is no browser master/lock. Web commands use FIFO inbox order, one admitted
web command at a time; touch may interleave before inbox entries reach the
Dispatcher. The Dispatcher then determines actual serialized application order.
Two simultaneous writes to one channel resolve in that order. Input arrival is
not a cross-task transactional clock. No web command is coalesced or reordered.

Blackout ON/OFF remains ordered through existing admission, Publisher and
transport barriers. While blackout is active the advertised working values
may change but transmitted output is zero. OFF restores the entry snapshot;
intervening edits/preset loads are discarded under existing semantics. The
OFF bulk update advertises restored values. Commands never access snapshot
ownership or bypass an occupied barrier slot.

Commands from a disconnected generation are discarded if not yet submitted.
Once submitted to the Dispatcher, the intent may still apply, including a
blackout transition; the result is not routed to a replacement session. This
avoids inventing cancellation or dropping an already ordered safety command.
Already applied final values remain authoritative even if delivery fails.

## Bounds, I/O, resource accounting

| Resource | Limit / behavior |
| --- | --- |
| WS registrations | 4, independent from Wi-Fi stations |
| HTTP accepted sessions | 6; existing listener/control reserve +3 |
| Shared inbox | 16 owned requests, no wait |
| Per-client outstanding | 4; explicit busy/backpressure, no speculative success |
| Dispatcher bridge | One web command in flight; unchanged Dispatcher capacity 32 |
| Per-client outbox | 8 fixed event slots; overflow invalidates client and discards its queue |
| Incoming text | 256 bytes; strict flat schema, no heap allocation |
| Incoming rate | 20 text attempts/client/fixed 1-second window; excess closes |
| All frames | 24/client/fixed 1-second window, including control frames |
| Upgrade attempts | Shared existing 12 HTTP/upgrade requests/second window |
| Format buffer | One 2,560-byte HTTP-task buffer, not per client |
| Status buffer | One static 4,096-byte HTTP-task buffer, avoids stack growth |
| One frame I/O | 100 ms absolute deadline across partial socket calls |
| Pump | At most one queued/active callback; four bounded rounds, one event/client/round, no owner wait |
| Heartbeat / idle | Server PING every 20 s; no incoming activity for 60 s closes |

The wire guard validates FIN/opcode/RSV/mask, canonical short/16-bit lengths,
256-byte text limit, 125-byte control limit and invalid one-byte CLOSE. All
64-bit lengths are rejected immediately (none can be canonical within 256
bytes), preventing the native narrowing edge case. Binary, continuation,
fragmented messages, compressed/extensions and zero-length text are closed.
Control PING receives matching PONG; PONG refreshes activity; CLOSE receives an
empty CLOSE then native cleanup. Standard browser PONG handling needs no JS.
No negotiated extensions or subprotocol are required. This is not a claim of
exhaustive RFC6455 CLOSE-code/UTF-8 conformance testing of the old native server.

The socket override retries partial writes until the requested byte count or
frame deadline, and supplies exact header/mask reads. A partially sent frame
causes close, never successful delivery accounting. Timeouts do not retry an
ambiguous WebSocket frame. Fds stay in the HTTP task, queued work scans current
sessions rather than retaining fd pointers. Close clears bridge generation and
native session state. Overflow/disconnect requires a fresh snapshot, rather
than silently skipping state or safety transitions. A rejected well-formed ID
is consumed: retry with a larger ID. IDs rejected as replay never mutate state.

Slow socket work can delay HTTP responsiveness on its task. Drain work starts
at most four rounds (16 data events with four active WS clients), checks a
20 ms elapsed budget between sends, and can overshoot by one in-progress frame's
100 ms deadline. Heartbeats/housekeeping are separate bounded work. This is
not a hard real-time guarantee for native SDK execution or scheduling. It never
makes the owner wait for I/O. The fixed outboxes can disconnect clients during
aggregate command bursts faster than the 20 ms drain. Stage 6B must throttle
sliders, respect backpressure and reconnect; configured rates are bounds, not
a guaranteed sustainable four-client throughput claim.

No task/stack is added. Native target bridge storage is 19,864 bytes from ELF;
host ABI reports 20,128 bytes (host pointers/alignment differ). Fixed snapshots
are only message copies, never an authoritative second universe. Normal native
RAM delta also includes formatter/status buffers, larger session metadata and
alignment. Wi-Fi, TCP and native HTTP still allocate bounded/configured SDK
resources at runtime; static size is not the full runtime cost. No DMX or
DMA-required allocation was moved to PSRAM.

`/status` retains all existing samples/transport fields and legacy HTTP
`stage:5` marker. New `websocket` fields: clients/max_clients/http_sockets,
inbox/current and high-water, outbox high-water, accepted/rejected/applied,
broadcasts/send_failures/slow_closes/static_bridge_bytes. Accepted counts inbox
admissions, not applied success. Disconnect cancellations and no-change success
mean these counters are not a simple accounting identity. `dma_free` reports
current free internal DMA heap. Existing sample fields internal/minimum/largest/
dma/psram report internal free/minimum, largest internal 8-bit block, largest
internal DMA block and free PSRAM. GET /status captures HTTP stack HWM, including
previous WS work; worker HWM, maximum owner-loop gap and TX errors/stopped remain.
There is no separate WS-task HWM because no WS task exists.

The bounded `/ws-diagnostics` resource and one-shot `WS_FIRST_FAILURE` log retain
session lifecycle, enqueued/taken/sent updates, queue occupancy/high-water/full,
native send return, first/last closure reason and source, work progression,
Controller revision/fanout and Wi-Fi disconnect counts. The first failure is
immutable, and normal peer CLOSE is distinct from abnormal TCP termination.
The SDK does not expose a close/eviction reason or SoftAP disconnect reason;
unobservable values remain explicit rather than fabricated. See the
investigation report for the complete diagnostic field contract.

Initial metadata capture and preset/reset invalidations inspect eight preset
states on the owner task; other snapshots/blackout updates copy a derived
eight-byte occupancy cache. Load/save/copy/clear invalidate it even on failure,
so corrupt/storage states remain distinct. This cache cannot mutate Controller
or storage. No periodic NVS polling is introduced. Storage errors retained in
the cache are revisited after a preset operation/reset, rather than silently
converted to missing. These are bounded read-only queries, not a firmware-time
WCET claim; include preset operations in hardware owner-gap tests.

## Security model

The latest approved product decision overrides the earlier Stage 5 suggestion
to add command authentication. WPA2 Wi-Fi access is the only access boundary.
No login, accounts, roles, pairing or mandatory factory password change.
Possessing or deriving the predictable factory Wi-Fi password permits remote
DMX/preset control. Origin checking is a browser cross-site mitigation, **not
authorization**: a native client can omit/spoof it. Missing Origin is permitted;
browser Origin must be exactly `http://192.168.4.1` or its `:80` form. Host uses
the same fixed address policy. `Origin: null`, unrelated origins, other hosts
and query-suffixed `/ws` requests are refused. No wildcard CORS is added.
The existing HTTP CSP gains only `connect-src 'self' ws://192.168.4.1` so the
local socket is explicit, without broadening script/asset origins. This avoids
relying on every browser treating HTTP self and WS as equivalent. See the
[CSP connect-src specification](https://www.w3.org/TR/CSP/#directive-connect-src).

No Wi-Fi credential getter, storage namespace, arbitrary memory access or
remote reset is exposed. Existing factory/custom credentials and presets are
not regenerated. Bounded input, rates, queues and socket cleanup reduce resource
abuse but do not prevent an associated attacker from controlling DMX or denying
service. No TLS or Internet-facing deployment claim. Future Stage 6B optional
password change and nonblocking first-use warning remain required product work.

## Software validation

Final closure reruns validation with pinned existing dependencies and the same
16 MiB two-slot layout. The user supplied hardware evidence above; the agent
does not open a monitor or access the device during closure.

- PASS: new protocol/bridge host tests: schema, limits/invalid values/version,
  duplicate/unknown fields, wire stream splits/64-bit length/fragment/RSV/mask,
  four sessions, initial/live race, touch and two web sources, monotonic state,
  real Dispatcher completion, blocked admission/no early ack, blackout restore,
  unsupported mode, preset success/storage failure, ID replay, queue saturation,
  slow-client close, stale generations, in-flight disconnect, reconnect,
  sustained latest value and cross-thread locked-queue stress.
- PASS: actual application endpoint and I/O functions extracted at test time,
  compiled against native API/socket/time mocks: Host/Origin/capacity/repeated
  handshake, controls, oversized/binary/fragment/incomplete input, deadlines,
  short writes, split reads, early upgrade pipeline rejection. These mocks do
  **not** execute a real browser/native binary handshake over TCP.
- PASS: existing Controller, Dispatcher, transport/actual patched ISR fault
  injection, Wi-Fi policy at capacities 1/4/8, PSRAM canvas, startup history,
  trace-disabled normal/diagnostic variants, HTTP policy/generated asset API:
  **16 freshly compiled host cases**, including the two new WS cases.
- PASS: exact-baseline integration, Dispatcher/UI guards, startup trace, canvas,
  DHCP/credential/station/reset integration, new WS integration, asset pipeline,
  six infrastructure fixtures and driver fresh/idempotent/mixed/unknown source
  patch-state checks. Whitespace and final preservation checks recorded below.

Reproduce new tests (no device):

```powershell
python -B tests/network/run_websocket_tests.py --compiler .\dist\host-toolchain\ziglang\zig.exe --zig
```

The local Zig installation is existing ignored development tooling, not a new
firmware dependency. The runner also accepts a system C++ compiler. Native API
mock headers are generated into ignored dist from current production functions.
Existing test commands remain in Controller/Dispatcher/transport/network and
infrastructure READMEs. Development logs are in ignored
`dist/stage6a-validation-20261025/` and
`dist/stage6a-disconnect-investigation-20261025/`; fresh closure results are
isolated in `dist/stage6a-closure-20261025/`.

### Final builds and static-size comparison

All four closure builds PASS, including wire guard, preset cache, explicit
local WebSocket CSP, bounded batch drain and diagnostics. Firmware source is
unchanged during closure; all four builds are rerun for this gate.

| Environment | Static RAM bytes | RAM delta vs Stage 5 | Application Flash bytes | Flash delta |
| --- | ---: | ---: | ---: | ---: |
| `cores3se_unit` | 87,276 | +27,432 | 1,040,693 | +16,192 |
| `cores3se_base` | 87,276 | +27,432 | 1,040,661 | +16,188 |
| `cores3se_unit_diagnostics` | 89,892 | +27,432 | 1,042,821 | +16,124 |
| `cores3se_base_diagnostics` | 89,892 | +27,432 | 1,042,789 | +16,124 |

Stage 5 comparison: normal RAM 59,844; diagnostic RAM 62,460;
Unit Flash 1,024,501; Base 1,024,473; diagnostic Unit 1,026,697;
diagnostic Base 1,026,665 bytes. Flash means PlatformIO linked application size,
not full-flash file length. A 6 MiB application slot remains ample.
Final firmware.bin lengths: Unit 1,041,104; Base 1,041,072; Unit diagnostic
1,043,232; Base diagnostic 1,043,200 bytes. These are application images, not
release FULL.bin files.

All four receipts match current source digest
`038d455bbb0b59c5ec8093582c7f9bf450e3e220f4faacfe284edbe877d8be97`.

All builds run the unchanged artifact validation hook: 16 MiB image header,
two 6 MiB slots, gzip assets retained and separate-image receipt. No release
FULL.bin or migration bundle was generated/overwritten. Asset bundle SHA-256
remains `5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9`;
partition image remains
`6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`.

## Manual hardware acceptance (Windows)

Use a tested Unit DMX U183 + Battery Bottom, a safe test fixture and the already
migrated Stage 4B/5 16 MiB device. No erase/reset of NVS or credentials. Record
current preset occupancy; save/copy/clear tests must use user-approved disposable
slots. Do not change live show data accidentally. This procedure is for the
user to run; it was not executed by the agent.

1. Manually upload normal Unit:

   ```powershell
   pio run -e cores3se_unit -t upload --upload-port COM8
   ```

2. Verify Sender startup/touch/main/Precision/keypad, channel values and actual
   fixture output. Confirm Unit battery power and USB startup behavior remain.
   Normal build does not initialize HWCDC separately. Serial monitor is optional
   (`pio device monitor -p COM8 -b 115200`); primary evidence is HTTP/status and
   physical measurement, not on-demand Arduino Serial input.
3. Connect to the existing SoftAP using Settings credentials. Open
   `http://192.168.4.1/` and `http://192.168.4.1/status`; save baseline JSON.
   Check READY, configured stations, current heap/DMA, HWM, gap, TX health.
4. In Chrome/Edge on Windows, open DevTools (F12) **on the device HTTP page**.
   Paste the contents of `tests/network/ws_console.js`. Do not disable browser
   security or run it from a file/foreign origin. Wait for hello and a snapshot
   with exactly 512 values and eight preset states. No dependency install needed.
5. Send `dmxTest.channel(12,180)`. Expect an authoritative channel update then
   applied/no_change ack; verify channel 12 in the touchscreen/physical fixture.
   Change it on touch and observe a new state.update in the console.
6. Open second and third devices/tabs with the same harness. Compare state
   revisions, channel changes and touchscreen. Also test four WS sessions, and
   a fifth refused session, while / and /status remain usable. Separate same-
   station multiple tabs from multiple Wi-Fi stations in the test record.
7. Alternate client writes to the same channel and verify actual applied order.
   Test bounded slider-style changes, waiting for results; then deliberately
   exceed outstanding/rate bounds and verify busy/close, no false applied ack.
   Reconnect by rerunning the harness; full snapshot must recover final state.
8. Run `dmxTest.blackout(true)`, edit a channel, then
   `dmxTest.blackout(false)`. Verify zero output and restoration of entry values.
   Repeat brief ON/OFF sequences under channel load and preserve transition
   ordering. Console updates/acks alone cannot prove transmitted frame barriers.
9. Optional disposable preset load/save/copy/clear tests:
   `dmxTest.command("preset.save",{slot:8})` etc. These use real persistent user
   slots. Verify touch/web occupancy, existing data format, empty/occupied errors
   and no unexpected missing-key spam. Never clear an occupied user slot merely
   to complete a test.
10. Malformed request examples (no intended state changes):

    ```javascript
    dmxTest.ws.send('{"v":2,"id":100,"type":"state.request"}');
    dmxTest.ws.send('{"v":1,"id":101,"type":"channel.set","channel":513,"value":180}');
    dmxTest.ws.send('x'.repeat(257)); // closes: reconnect for a fresh snapshot
    ```

    Browser WebSocket API cannot generate arbitrary fragmentation/mask/64-bit
    lengths. Host tests cover guard logic; a separate raw WebSocket test tool is
    needed for end-to-end wire-malformation tests. Never infer those from a
    browser `send()` call. The server does not expose arbitrary methods.
11. Close clients during queued changes and broadcasts; repeat reconnect cycles.
    Test idle/ping behavior >60 s, browser suspension, slow readers, rapid page
    HTTP reloads and capacity exhaustion. Native browsers automatically PONG.
    Rejected Origin must receive no snapshot and apply no command.
12. Exercise Settings/Presets/Precision/keypad, grid scrolling, hold steps,
    display sleep/wake, normal mute and battery-warning volume restoration,
    existing startup/restart controls, mode selection/Receiver separation.
    Remote commands must not become an extra local wake gesture or owner task.
13. After each phase, run `await dmxTest.status()`; save JSON/time/client count.
    Sample no clients, 1/2/3/4 clients, sustained edits, preset operations, slow
    clients, disconnect/reconnect and sleep/wake. Compare internal free/minimum,
    largest internal/DMA block, free DMA heap, PSRAM, worker/HTTP HWM, max owner
    gap, TX errors/stopped and WS queue/rejection/send-failure counts.
14. Require no sustained post-cleanup heap decline, no reset, no TX errors or
    fail-stop and adequate tested stack/internal/DMA reserves. Existing guards
    remain 32 KiB free internal, 16 KiB largest DMA and 1 KiB HWM; they are
    provisional thresholds, not demonstrated safe margins under WS load.
    Compare owner gaps against measured Stage 5, including snapshot bursts.

The final two-client runtime counters above are hardware-validated. Detailed
heap/HWM/CPU headroom values and longer/four-client soak results were not supplied
at closure. For those follow-ups, capture complete /status snapshots,
not only one free-heap number. A large minimum-heap decline is historical and
does not reset after disconnect; compare current free/fragmentation separately.

External RS-485/logic-analyzer capture is still needed for 513-slot completeness,
immutable whole-frame coherence, blackout ordering on the wire, baud, break/MAB,
inter-frame gaps/cadence, signal quality and stress/sleep/reconnect behavior.
Host tests, owner-gap metrics, transport counters and visible fixture response
cannot establish electrical correctness or worst-case waveform timing.

## Remaining limitations, Stage 6B and rollback

- macOS Chrome with two concurrent clients and real fixture control passed.
  Three/four concurrent WS clients, other browsers/networks, long soaks and
  detailed memory/CPU/latency margins remain unverified; Base is experimental.
- The approximately 20 ms idle HTTP work schedule is a deferred optimization,
  not a closure blocker. No timer/task/architecture change is made here.
- Full NVS/SDK execution has no hard real-time bound proven here. Queues are
  bounded and the main owner never waits for networking; overload can reject
  commands or close a client and requires explicit resynchronization.
- An admitted web command blocked by existing transport admission remains
  pending, without an applied ack; no new cancellation, timeout/retry or driver
  recovery policy was invented. Existing failure/stop behavior is preserved.
- Existing upper-flash write, OTA rollback, shutdown-zero frames and external
  electrical measurement limitations remain. Diagnostic version text was not
  changed; artifact receipts fingerprint the actual firmware source independently
  of the documentation-only closure edits.
- Stage 6B must consume snapshots/revisions/results, throttle input, handle
  reconnect and errors without optimistic state, distinguish working vs blackout
  output, and add the planned optional password-change/first-use warning flow.
  The warning must not block DMX. No Stage 6B UI, credential API or preset
  backup/import/export implementation is included.
- If hardware acceptance fails, preserve this branch and logs; build the tested
  Stage 5 commit in a separate safe worktree and manually restore its normal
  Unit image on the same 16 MiB layout. Do not reset/stash/discard this working
  tree, erase NVS, change partitions or overwrite migration/backup bundles.

## Final review and local closure

Review confirms application-task-only Controller/Dispatcher access, retained UI
observer/admission/Publisher, owned cross-task copies, strict request and raw
frame validation, generation-checked lifecycle, atomic owner snapshot boundary,
monotonic state ordering, FIFO blackout transitions and bounded queues/drain.
No material defect requiring a firmware change was identified during closure.
No firmware source, driver, UI, timing or network architecture was changed to
close the stage or in response to the Windows Wi-Fi caveat.

All 16 host cases and nine integration/asset/infrastructure/patch commands are
rerun for closure, as are all four established firmware builds. `git diff
--check` and the exact staged diff are checked before the local commit. New
files are also checked for trailing whitespace and a normal terminating newline.
Logs are isolated in `dist/stage6a-closure-20261025/`.

The commit is restricted to the 21 Stage 6A source/header, test/helper and report
files listed above. It preserves the Stage 5 parent and prior Stage 3E/4B/5
contents; no main merge or history rewrite occurs. Historical files, ignored
backups/migration bundles/logs, the unrelated untracked Stage 3 report and
stash@{0} (`996a2f9378208540b0fd3b544e7d9908affe8e84`) are excluded and preserved.
The final Git working tree retains only the pre-existing unrelated untracked
work after the focused local commit. No upload, erase, push, merge, tag, release
or Stage 6B implementation is included.
