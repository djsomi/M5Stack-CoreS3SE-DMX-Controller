# Stage 6B.3 - Active web DMX channel control

Status: HARDWARE ACCEPTED. The user completed physical Stage 6B.3 acceptance
on the CoreS3SE and approved local closure, including the final Editor refinement.
Branch `codex/stage6b-web-ui`; parent/baseline Stage 6B.2 closure commit
`3eb72f31c61c53ae1617a3c7385d1327c884b5c8`. Earlier pending acceptance notes
below are historical checkpoints, superseded by the final closure section.
The closure task preserves historical development files, the unrelated Stage 3
report, logs/backups and the existing stash. Only one focused local commit is
authorized; no push, merge, tag/release, firmware upload/erase or NVS operation.
Stage 6B.1 shell/navigation and 4/6/8 banks are preserved. Scoped selection rules
and final Editor control geometry are documented below.

## Existing architecture and exact reuse

The installed v1 protocol ALREADY implements `channel.set`. This stage adds the
browser encoder/interaction path; it does not extend or replace the server
schema, introduce a new protocol or change the firmware command architecture.
Production C++/headers, Controller/Dispatcher, immutable DMX snapshot transport,
esp_dmx patch, physical UI, blackout barriers, presets/NVS, SoftAP/DHCP and HTTP task/
socket behavior are byte-identical to Stage 6B.2. The JS composer lists the new
bounded control factory. Three existing gzip assets embed it through the existing
manifest/content-hash pipeline and 16 MiB layout.

Source references:

- `web/src/js/fader.js`: native range events, captured channel identity,
  disposable preview rendering and availability gating.
- `web/src/js/editor.js`: existing bank mapping and card lifecycle; resolves
  active intents before replacing a bank or responsive card set.
- `web/src/js/control.js`: pacing, per-channel latest intent, final priority,
  feedback and invalidation. No independent universe or browser channel lock.
- `web/src/js/commands.js`: strict channel encoder, shared session IDs, separate
  single snapshot and single channel acknowledgment trackers.
- `web/src/js/transport.js`: existing native WS generation/recovery/health
  machinery, plus one channel ack deadline and result dispatch.
- `web/src/js/app.js`: wires factories; existing RAF batches UI updates. Leaving
  Editor resolves active intents without replacing persistent navigation.
- `src/websocket_protocol.cpp:decode`: bounded flat JSON parser; `encode`: v1 events.
- `src/websocket_bridge.cpp:receive`: HTTP-task validation, copied inbox and credits.
  `ownerPoll`: application-task Dispatcher admission only.
  `completed`: broadcasts state changes before reply to originating client.
- `src/command_dispatcher.cpp:apply/process`: serialized domain operations and
  existing publisher. `src/dmx_controller.cpp:setChannel/setBlackout` owns values,
  revision and restore snapshot. These firmware files are unchanged.

Flow: native range input -> local disposable preview -> bounded `channel.set`
-> HTTP-task strict decode/copy -> application-owner Bridge -> Dispatcher ->
Controller -> existing DMX publisher -> authoritative broadcast -> all browser
stores. Origin receives application result after that publication. WebSocket
handlers never write the Controller or driver directly.

## Exact v1 wire contract

Request (ASCII text, at most 256 bytes):

```json
{"v":1,"id":7,"type":"channel.set","channel":21,"value":128}
```

Applied mutation, broadcast to every ready client:

```json
{"v":1,"type":"state.update","id":0,"revision":"297","seq":"820","blackout":false,"sender":true,"channel":21,"value":128}
```

Result only to origin, after application/publication:

```json
{"v":1,"type":"command.ack","id":7,"revision":"297","seq":"821","status":"applied","domain":0}
```

A repeat of the existing value produces `command.ack` / `no_change`, unchanged
revision and no invented state.update. IDs increase across both `state.request`
and `channel.set` within one socket generation, 1..4294967295; they never wrap.
A new synchronized generation starts fresh IDs and never replays old mutations.
A successful `WebSocket.send()` is only transport submission, never success.
The existing `accepted` status is internal admission, not a wire success ack.
The frontend never derives a channel value from an ack. Successful ack revision
must already be covered by authoritative received state; otherwise synchronization
is invalidated and a new snapshot required. Ordered gaps in global seq are legal.

Examples of rejection:

```json
{"v":1,"type":"command.error","id":7,"revision":"0","seq":"0","status":"invalid","domain":0,"reason":"invalid"}
{"v":1,"type":"command.error","id":8,"revision":"0","seq":"0","status":"busy","domain":0,"reason":"backpressure"}
```

Pre-owner errors have seq/revision zero; owner errors report current revision and
sequence. Validation rejects duplicate/unknown/missing fields, nested/array data,
non-ASCII/escapes in grammar strings, unsupported types/version, negative or zero
IDs, overflow, channel outside 1..512, value outside 0..255, decimals/exponents,
string-encoded values, booleans and null. Only exact integer fields are accepted.
Malformed frames never enter Dispatcher or change the domain/output. Existing
fragment/opcode/masking/length gates, origins and client limits are unchanged.
Backend parsing uses bounded stack/fixed storage rather than heap JSON parsing.
Read-only v1 observers and `state.request` response-as-complete-snapshot behavior
remain compatible.

`command.error` is displayed and clears interaction intents; no mutation retry.
A transport_error can follow a successful domain mutation but failed transport
publication: do not roll back authoritative state or claim that nothing changed.
The UI explicitly says state may have changed. An ack establishes domain outcome,
not that a particular RS-485 frame reached a fixture. No electrical claim follows.

## Drag scheduling, previews and concurrency

The existing server closes sessions after >20 inbound messages in its one-second
window. Therefore the suggested 20-30 Hz client target would violate the tested
server contract. No queue/rate capacities were increased. Chosen normal cadence:
60 ms (about 16.7 updates/s aggregate), with a HARD rolling maximum of 18 channel
sends per 1000 ms, leaving allowance for the existing infrequent health/resync
snapshot requests. The application uses one control service per native socket.
Global pacing here is justified by the existing per-client aggregate server cap;
there is no independent per-channel fixed delay or browser ownership lock.

At most eight channel-intent entries retain {channel, latest value, active,
dirty, sent}; one write may be in flight, one pacing timer and at most 18 recent
send timestamps. Each new input overwrites that channel's obsolete pending value.
Service rotates entries for fairness; released finals have priority. If eight
intents are already retained, a ninth new channel is rejected locally with a
busy message. It is not silently accepted. Normal bank changes resolve old
active entries first, retaining exact immutable channel identity.

Release/change/pointerup/blur marks the latest value final, bypassing the normal
60 ms delay when credit is available. If a write is in flight, the final remains
in its channel entry and is dispatched after its result. If the rolling cap is
exhausted, it waits only for the existing bounded credit window. It is never
replaced by an older pending input. Duplicate change/pointerup events after the
first release do not duplicate the final. Pointer cancellation drops unsent
intent; already submitted commands cannot be retracted. No blocking delay.

Native input range controls preserve mouse/touch/keyboard behavior, integer
step 1, 0..255 bounds and the existing vertical geometry. Programmatic value
rendering does not dispatch input events. Bank arrows, touch-action, layout and safe
areas are unchanged; pinch zoom remains available. No broad preventDefault or
custom gesture tracking was added. Actual Safari native range tracking is still
part of hardware acceptance, not established by mocked input events.

Preview is limited to interaction entries, not a second authoritative universe.
While active or awaiting final confirmation, remote values do not fight the
pointer. Output accessibility labels mark unconfirmed previews; the existing
footer shows BOTH preview and current Controller value so conflicts are visible.
After final ack the overlay is removed and the latest authoritative value wins.
With several sources the last successfully APPLIED owner command wins. Arrival,
local preview or successful socket send confers no ownership. A later physical
edit/other browser command/preset load may supersede an earlier acknowledged
value. All clients ultimately converge through authoritative publications.

## Blackout, presets and receiver separation

No web blackout or preset encoder/handlers are enabled. Buttons remain disabled.
The authoritative Controller's existing semantics remain exact:

1. Blackout ON saves its entry working snapshot and publishes zero output.
2. Channel edits while ON change working values and revision, but output stays
   zero and the entry snapshot is not overwritten.
3. Blackout OFF restores the entry snapshot, discarding those blackout edits.

The owner application time determines which side of the ordered blackout barrier
a channel command falls on. A queued command applied AFTER OFF is a new normal
edit, not an edit retroactively belonging to blackout. No browser special case,
coalescing of blackout events or attempt to change restoration semantics exists.
Physical preset operations still publish complete 512-value state and occupancy;
storage format and metadata are unchanged. Native receiver-mode state disables
web faders; Dispatcher still independently rejects unsupported_mode.

## Loss, stale state and recovery

One independent five-second CHANNEL APPLICATION ACK deadline is added, separate
from the accepted Stage 6B.2 15-second idle probe / 10-second matching snapshot
deadline and bounded delayed-timer grace. Channel timeout retires the generation,
marks state stale, closes the old socket, then uses existing bounded reconnect
and mandatory hello/full snapshot. It never retries the mutation. Pending native
CLOSING ownership and manual Retry remain unchanged. Errors and malformed
messages use existing recovery; unmatched/pre-owner IDs cannot finish a write.

Loss/resync/background/receiver transition invalidates disposable previews and
unsent intents immediately. Visible controls disable whenever state is stale,
unsynchronized, paused or not Sender. A newly synchronized snapshot enables
fresh user actions only. Already admitted domain commands may still apply after
client disappearance; fresh snapshots reveal actual outcome. No false rollback
or automatic final-value replay is performed. iOS visibility/pagehide/pageshow
recovery is unchanged. Health probes may coexist with one mutation (two requests
maximum, within the existing four credits); each resolves only its own ID and
contract. Normal traffic still suppresses idle probes; routine probes stay silent
in the LIVE indicator.

## Security and memory boundaries

Firmware/server tasks, inbox 16, outbox 8 per client, four WS clients, four
outstanding requests per client and existing six HTTP sessions are unchanged.
No new firmware diagnostic output, dynamic allocation, task, driver buffer or
persistent NVS data is added. Browser-only bounded state adds eight disposable
intent records, 18 timestamps, one write descriptor, one pacing timer and one
ack deadline timer; snapshot tracking/health timer remain existing. A single
result listener and existing store observer are installed once; no per-drag
listeners, unbounded histories or mutation queue/replay storage. JS creates small
short-lived JSON/result objects in the browser, not ESP32 heap allocations.

This is not a new authorization system: the existing v1 server already accepts
other command types from a validated session, although this UI exposes only
channels. Existing origin/WPA2/SoftAP boundaries and predictable factory password
tradeoffs remain. WPA2 association alone is not strong per-command user identity.
No speculative access-control or server-protocol redesign is included here.

## Automated evidence (2026-10-26)

- Frontend navigation tests: synchronous tabs, scroll ownership/reset, keyboard
  focus, safe areas and persistent navigation PASS.
- Live frontend: 57 integration groups PASS, including 26 new Stage 6B.3 groups;
  factory/dependency isolation, all 512 bank mappings and bootstrap PASS.
- 1000 rapid inputs coalesce; sliding windows stay <=18; latest released 255 is
  delivered. Separate rapid keyboard-final saturation case also preserves final.
- Tests execute shipped encoder/transport/control/fader/editor modules, mocked
  native input/change/pointerup for mouse/touch/keyboard at 4/6/8 columns, CH512,
  preview conflicts, no_change/rejection/transport_error, missing ack/publication,
  resync, background/loss mid-drag, no replay, and shared health IDs.
- Actual C++ decoder/Bridge/Dispatcher/Controller extended with malformed/typed/
  missing/duplicate/range rejection without mutation, boundaries, applied/no-op
  revision/ack order, read-only observers and independent/same-channel commands.
- All 21 freshly compiled host variants PASS: WS/core/adapter, Controller,
  Dispatcher, transport/ISR fault injection, HTTP/assets, SoftAP/station 1/4/8,
  production/diagnostic trace variants and PSRAM canvas guards.
- Eleven deterministic pipeline/CSS/markup/cap tests PASS; six infrastructure/
  packaging corruption/staleness/reproducibility tests PASS.
- All integration/source-scope checks PASS: protected DMX/driver/domain/UI/power/
  USB/presets/partitions; SoftAP/DHCP; HTTP/WS task boundaries; startup/canvas.
- Driver patch-state fresh/idempotent/mixed/version/template rejection PASS.
- Generated assets verified twice deterministically; all receipts match final
  source/asset identities. Git diff --check PASS (final audit recorded separately).

The host tests cannot prove real ESP32 task timing, slow-client latency,
concurrent iPhone native touch behavior, physical output, DMX baud/break/MAB/
frame coherence, real heap high-water marks or experimental Base signal quality.
No firmware upload or device access occurred. Hardware acceptance remains open.

## Current builds and resources vs committed Stage 6B.2

| Environment | Result | Static RAM bytes | Flash bytes | RAM delta | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87,508 | 1,059,997 | +0 | +3,088 |
| cores3se_base | PASS | 87,508 | 1,059,965 | +0 | +3,088 |
| cores3se_unit_diagnostics | PASS | 90,220 | 1,064,529 | +0 | +3,088 |
| cores3se_base_diagnostics | PASS | 90,220 | 1,064,501 | +0 | +3,088 |

| Asset | Raw bytes | Gzip bytes | Raw delta | Gzip delta |
| --- | ---: | ---: | ---: | ---: |
| index.html | 9,942 | 2,811 | -257 | -92 |
| style.css | 13,757 | 3,858 | +1,301 | +318 |
| app.js | 45,705 | 12,104 | +12,572 | +2,858 |

Each gzip remains below16384 bytes. Total compressed payload: 18773 bytes.
These are current final-Editor results; earlier follow-up tables below are dated
historical snapshots. RAM is link-time static usage, not runtime free heap/PSRAM.
No release/migration bundle was created or flashed. Current build source SHA256:
`019130b5688ab75adb8b1eacddef2d6782443f4412b035e0b38661163f4210ea`. Current asset bundle SHA256:
`dc534e2bc3420beed4f10891d4411d60328702b6d3719c95bb82676535d37297`.

## Real-device acceptance procedure (not executed)

1. Review this uncommitted increment. Only when ready for physical testing,
   manually upload Unit with the existing command (COM8 if still the board):
   `pio run -e cores3se_unit -t upload --upload-port COM8`.
   Do not erase flash/NVS, migrate partitions or alter stored credentials/presets.
2. Attach the known Unit DMX/Battery Bottom setup and a known, deliberately chosen
   fixture/function channel. No automated sweep, macros, multi-channel scenes or
   blind changes to unknown fixture functions. Observe ordinary continuous output
   and the unchanged physical UI before web testing.
3. Join existing SoftAP from iPhone Safari/Home Screen, Windows Chrome and MacBook.
   Load/reload current assets. Faders must stay disabled until hello/full snapshot
   establishes Sender synchronization; preset/blackout buttons always disabled.
4. On one deliberately selected known channel, drag slowly then rapidly on iPhone
   (touch), desktop (mouse) and keyboard arrows/Home/End where supported. Check
   local responsiveness, stable pointer, final value, physical Controller and
   real fixture response. Check bounds using a known safe fixture channel/value.
5. Test 4/6/8-column banks, arrows, CH512 selection and bank change during a drag
   with an unsent value. Only the original captured channel may receive its final;
   a newly displayed channel must not inherit it. Repeat tab change/responsive
   orientation change and rapid arrow tapping; verify previous iOS zoom fix,
   pinch zoom, safe areas, stable bottom nav, tab scroll reset and focus.
6. iPhone CH1 / desktop CH2 simultaneously (only if these are known safe fixture
   channels); verify both and observers. Then same-channel physical/browser/two-
   browser conflicts: last applied command wins, all converge after release. The
   preview/Controller discrepancy must be visible and no pointer oscillation.
7. Enter blackout PHYSICALLY. Edit a safe channel from browser: working value may
   change, actual output must stay zero. Exit physically: pre-entry snapshot must
   return. Verify pending commands applied after OFF are new edits. Physically
   load existing presets: complete universe and occupancy synchronize, no NVS
   format/selection/corruption-state regression. Do not enable web preset actions.
8. Break Wi-Fi/close a session during dragging, before final acknowledgment. UI
   must become stale/disabled, then reconnect with complete current snapshot.
   No old value/final may be replayed; issue a new deliberate input afterward.
   Repeat Home Screen background/resume, quiet >60 s, idle health probe and
   manual Retry. Verify no LIVE probe flicker, duplicate socket or final replay.
9. Repeat three-client load and observe existing /status diagnostics: queue high
   watermarks, failed/slow sends, overflow, AP stations and DMX TX health. No new
   production periodic AP_STATIONS trace. Capture durations/errors if stalled.
   Confirm original preset/speaker/battery/sleep/navigation behavior remains.
10. Base remains experimental. Build success is not Base electrical acceptance;
    test on its established external-PSU setup separately. External RS-485 capture
    is required to claim frame coherence, break/MAB/length/gaps under web load.

Known limitations: synchronous ESP-IDF sends can still stall; no bridge rewrite
or speculative socket fix was made. Final release waits for outstanding ack or
rate credit if congested. ACK proves application, not electrical delivery. A
lost-session command can have uncertain outcome; snapshot, not retry, resolves
it. No web presets/blackout/scenes, browser locks, new authentication or offline
mutations. Physical acceptance is required before a future closure commit.

## Exact changed-file inventory

```text
docs/STAGE6B3_ACTIVE_WEB_CONTROL.md
scripts/web_ui_bundle.py
tests/network/test_websocket.cpp
tests/web/README.md
tests/web/check_pipeline.py
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
web/src/js/control.js
web/src/js/editor.js
web/src/js/fader.js
web/src/js/transport.js
web/src/style.css
```

Ignored validation logs/executables/receipts live in `dist/stage6b3-validation/`
and follow-up evidence in `dist/stage6b3-polish/` and `dist/stage6b3-editor/`.
No changes are staged or committed. Do not push/merge/tag/release or proceed to
another feature before review and physical acceptance.


## iOS selection polish and physical-slider latency review (2026-10-26)

### Scope and CSS selection policy

User hardware evidence: active iPhone web faders and real Controller/DMX operation
work. Occasional iOS text selection and perceived physical long-slider lag are
follow-up observations, not evidence that network traffic is their common cause.

The DOM has a non-editable application shell/navigation, native range faders,
a number input for Go-to-CH, a checkbox preference and read-only Status values.
The prior stylesheet disabled selection only on `.fader input`; surrounding
channel labels, numbers and navigation text remained selectable. This supports
the narrow selection correction:

```css
body { /* existing dimensions unchanged */
  -webkit-user-select: none;
  user-select: none;
}
input, textarea, [contenteditable]:not([contenteditable="false"]), .status-panel dd, #connection-error {
  -webkit-user-select: text;
  user-select: text;
}
```

The existing, more-specific `.fader input` rule retains both `none` declarations
for non-text native range controls. Ordinary input editing, textareas and editable
hosts (including empty/plaintext-only contenteditable attributes) retain text
selection. Status `dd` values and connection-error text explicitly support copying.
No element or layout geometry changed. No touch-callout restriction is needed
on the available evidence, so none was added. The scoped bank-arrow
`touch-action: manipulation`, native range events, pinch zoom, selection in edit
fields, pointer/keyboard behavior, safe areas and svh shell remain intact. No
preventDefault, gesture handler, viewport scaling restriction or runtime JS change.
Actual iOS selection/copy behavior still requires the physical checklist below;
a served-CSS contract test is not a native Safari selection test.

### Physical touch/update execution path (exact inspected sources)

- `src/src.cpp:845-856`: `M5.update()` samples touch FIRST; transport/ownerTick,
  Bridge ownerPoll, Dispatcher process and serviceControllerChanges run before
  Sender touch handling. Reading getDetail afterward reads that sampled state,
  not a fresh hardware read. Work between sampling and painting increases age.
- Installed M5Unified 0.2.21 `utility/Touch_Class.cpp:8-24` and
  `Touch_Class.hpp:41`: raw getTouchRaw is skipped while elapsed <=4ms, otherwise
  queried; detail state can still update between raw samples. This is a minimum
  raw-poll interval, not a fixed responsiveness guarantee or a 16.7Hz throttle.
- `src/view_sender.h:348-363`: latched main slider maps y to
  `255 - ((tp.y - 42) << 1)`, clamps 0..255 and dispatches only a changed value.
  There is NO browser pacing or per-movement delay in this physical branch.
- `view_sender.h:512-524`: applyMainCommand -> executeLocal, synchronize selected
  value, incremental slider paint, then updateDisplay for changed revision.
  `drawUIValueSet:4828-4860` redraws its CH header when value changes and paints
  the changed bar extent. It does not recreate the shared full-screen canvas.
- `src/command_dispatcher.cpp:155-198`: executeLocal preserves ordering by
  processing older queued commands (bounded at capacity 32), then applying the
  local command. Ordinary Bridge ownerPoll admits at most one in-flight web
  command and the loop processes before touch; 32 is a bound, not an observed
  per-touch backlog. Safety admission/backpressure can return busy without a
  blocking wait. Do not bypass barriers to chase responsiveness.
- `view_sender.h:506-509`: local execution suppresses controller_redraw_pending;
  external application sets it and updates the selected-channel scalar.
  `serviceControllerChanges:78-90` then calls updateDisplay(true) and forced
  slider redraw on Main, or a full Precision page when that page is active.
  Thus remote changes to ANY channel can force expensive visible-grid painting
  before the next physical touch is handled. Flags coalesce within a turn, but
  sustained remote changes can trigger this repeatedly. This is a concrete
  candidate cost; it is NOT evidence of measured lag or a production defect.
- `updateDisplay:4895-4985` scans the universe, draws visible affected cells
  (full redraw draws all visible cells, approximately 25-30 depending on scroll)
  with numbers/two arcs, clears/reuses one of two small sprites and pushes them
  to the display. `fillBar:4871-4893` also emits multiple color runs. These are
  synchronous application-task operations with real display work to measure.
  Installed M5GFX 0.2.28 `LGFX_Sprite.cpp:58-87` only reallocates if missing or
  size changes; same-size createSprite clears/reuses it. Do not diagnose a
  per-cell allocation leak simply because createSprite appears inside the loop.
- `view_sender.h:1092`: publisher copies 513 output bytes into the existing
  transport staging path. `patches/esp_dmx_v202/tx_api.inc:71-80` retains the
  existing configured break/MAB busy wait (about188us when emitted), outside
  the local mutation's socket path. No transport timing/policy changed.

Network work triggered by each changed physical value:

- `src/websocket_bridge.cpp:130-149`: completion creates a bounded Event and
  broadcasts by copying it into each ready client's fixed queue under the short
  Bridge lock. Event includes fixed 512-value/preset storage even for a channel
  event; zeroing and copies are bounded CPU work, not an allocation or wire wait.
  Changed channels do not inspect preset NVS metadata. A web-origin command also
  queues an ack to its origin; a local touch does not need a web command ack.
- `src/websocket_service.cpp:8-12`: a cross-core spin critical section protects
  those copies. It never contains a socket send. Contention and copy cost are
  candidates to measure, not grounds for a speculative rewrite.
- `src/network_service.cpp:329-392,634`: JSON encoding/socket sends occur on HTTP
  core0, priority3. Arduino SDK config has CONFIG_ARDUINO_RUNNING_CORE=1. The
  network worker/core0 schedules one bounded HTTP work item; application owner
  does not call httpd_queue_work or send. A slow synchronous send can stall the
  HTTP task and unrelated clients, but cannot be equated to an application-task
  290ms blocking socket call. Shared resources, scheduler/interrupt activity and
  short locks could still have indirect effects; no timing measurements isolate
  these here. Battery/I2C sampling, display/SPI work and existing transport work
  also remain alternative causes to compare.

Conclusion: browser pacing explains the roughly60ms cadence of REMOTE web-driven
physical display changes. It does not throttle DIRECT physical slider input.
Concurrent remote changes can increase physical redraw cost. Local touch-only
streaming still incurs bounded broadcast-copy work, but no socket call on its
path. No unnecessary drawing was removed and no optimization is claimed.

### Measurement plan and why no timing instrumentation was added

This increment changes no C++ code, diagnostic initialization or runtime
instrumentation. We have not measured touch-to-domain, touch-to-paint, sample age,
phase durations or lock time. Existing `/status` has `max_owner_gap_us`
(`ownerTick:740-747`), heap/stack samples and TX health; `/ws-diagnostics` retains
send/work delay/max durations, queue/event counts and session generations.
The owner gap is a lifetime maximum between service calls, not a touch latency,
per-page draw duration or per-incident timestamped correlation. Startup/page work
can dominate it; reading it alone cannot confirm the suspected lag.

First compare identical safe physical slider gestures under:

1. No WS clients (SoftAP still enabled), same screen/selected channel.
2. One/three idle read-only observers receiving physical updates.
3. One/three clients with remote commands on a DIFFERENT safe channel.
4. Same-channel remote control separately, to distinguish legitimate serialized
   contention/last-applied-wins value changes from latency.

Keep fixture, battery/PSU, page, scroll position and movement speed constant.
Use fresh equivalent boots if comparing lifetime max_owner_gap_us; collect
/status and /ws-diagnostics only sparsely outside movements to avoid making a
poller the load. Record physical screen and finger together (high-frame-rate
video if available), along with whether visual paint or actual fixture response
lags. No automatic channel sweep. If lag is reproducible, a separately reviewed
DIAGNOSTICS-only addition can aggregate fixed count/sum/max/bucket timings for
M5.update, dispatcher/owner publication, external redraw, local command and paint,
plus max input sample age; no per-movement Serial logging or heap allocation.
Correlate aggregate windows with existing HTTP send/work metrics, not a claim
that different-task elapsed times identify causality. Dump once after the gesture
through the existing bounded USB path, without HWCDC changes. External DMX
capture is needed to separate paint latency from frame/fixture response.

### Latest roughly32-minute diagnostic incident

User-reported totals: web applied775, Controller events2713, DMX errors0,
outbox high8/8, queue_full2, send_failures1, slow_closes2, Wi-Fi disconnect1.
The bridge applied counter includes applied AND no_change command results;
Controller events count revision-changing commands from multiple sources.
Do not subtract these totals to derive physical movements or packet rate.
Zero TX errors is the driver's software result, not an electrical integrity test.

| Reported uptime | Evidence | What it establishes |
| --- | --- | --- |
| 1869022ms | Wi-Fi station-disconnect event | AP-wide callback count/timestamp; not a WS socket identity |
| 1871666ms | First Bridge queue-full | A generation's ninth pending event found its 8-event outbox full and retired it |
| 1871881ms | Send failure, 290421us, errno116 | Failed synchronous send/timeout path; target newlib ETIMEDOUT is116 |

Intervals: disconnect -> queue-full2644ms; queue-full -> send failure215ms;
disconnect -> send failure2859ms. If the reported failure timestamp is the
`wsSend` completion/closure timestamp, the measured duration implies a send
start around1871591ms, about75ms BEFORE the queue-full and about2569ms after
Wi-Fi disconnect. Millisecond timestamps/unspecified diagnostic fields make
this an approximate reconstruction, not proof that all records refer to one
session. The provided evidence does not include matching token/slot/generation,
fd and association correlation for every event.

Source-level interpretation:

- `network_service.cpp:90-127` counts AP_STADISCONNECTED and records last_wifi_ms.
  Associations and HTTP sessions are separate; no MAC-to-socket mapping or
  immediate socket close occurs in that callback. A station disappearance can
  leave TCP/HTTP reporting a session until transport/cleanup detects failure.
- `wsSend:329-345` calls IDF4.4.7 httpd_ws_send_frame_async synchronously. The
  pinned cached `httpd_ws.c:375-430` directly invokes send_fn for header/payload;
  its name does not make the socket operation nonblocking. The configured frame
  deadline is100000us; native send may return late. The existing sub-ms guard
  prevents a <1ms timeval becoming unlimited, but cannot preempt an in-progress
  blocking lwIP call. 290421us is consistent with the known send-stall limit.
  io_op1 means exhausted wrapper budget; io_op3 means underlying send failed.
  This incident did not supply io_op, so do not assert which one produced errno116.
- While HTTP is blocked, owner publications continue and bounded queues can
  fill. Origin receives update+ack per applied web command (two events), other
  clients one update. Local controls are not browser-rate-limited. A 290ms
  pause can plausibly exhaust an 8-event queue, depending on existing backlog
  and input rate; aggregate totals cannot prove that exact fill timeline.
- `websocket_bridge.cpp:24-27` increments queue_full/slow_closes and retires the
  bridge generation immediately. `sendFailed:64` still counts a subsequent
  failed in-progress send even if that generation is already inactive; it does
  not drop/close the bridge twice. The added host regression exercises precisely
  this order and preserves the original first-failure record and other clients.
- `network_service.cpp:345,348-392` records genuine send errors, requests session
  closure and closes inactive bridge sessions on the HTTP task. The IDF close
  request is itself queued work; a stalled HTTP task cannot service it early.
  The final in-progress send need not finish successfully merely because its
  bridge queue has already retired.

The sequence is CONSISTENT with a departed/slow client being detected/closed.
It does not prove the Wi-Fi event belonged to that WS client, that both overflow
closures were that same client, or that the second queue-full was unrelated.
A shared HTTP stall can delay otherwise healthy sessions too. Two queue-full
counts may concern different slots or later generations, not duplicate counting
of an already inactive generation. Capture first/last and per-session token,
queue-start/end, io_op, attempted/last-sent revision and send/work durations on
recurrence. A diagnostics build can provide redacted AP identifiers, but still
needs association/session correlation to attribute a cause. Keep all failures
visible. No queue enlargement, timeout change, error suppression, reassignment
of blame to iOS, or network architecture change is justified by this evidence.

### Follow-up validation and resources

- 47 frontend live groups and navigation/foundation PASS.
- Ten pipeline tests PASS, including served-byte selection/copy exceptions,
  preserved range specificity, viewport/zoom/arrow rules and no new handlers.
- All21 freshly compiled host variants and integration/regression guards PASS.
  Added Bridge case proves late failure after queue retirement is counted once
  as a send failure without a second drop; immutable first/other client preserved.
- Six infrastructure cases, deterministic gzip/manifest/cap checks and all driver
  patch-state/transport/ISR regressions PASS. No hardware accessed.
- All four builds PASS. Firmware source/configuration, authored/composed JS and
  JS gzip are byte-identical to the preceding hardware-tested Stage6B.3 build.
  Only CSS and dependent embedded assets alter the firmware images.

Delta vs the immediately preceding Stage6B.3 build:

| Environment | Result | Static RAM bytes | Flash bytes | RAM delta | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87,508 | 1,059,045 | +0 | +160 |
| cores3se_base | PASS | 87,508 | 1,059,013 | +0 | +160 |
| cores3se_unit_diagnostics | PASS | 90,220 | 1,063,593 | +0 | +176 |
| cores3se_base_diagnostics | PASS | 90,220 | 1,063,565 | +0 | +176 |

| Asset | Raw bytes | Gzip bytes | Raw delta vs prior6B.3 | Gzip delta |
| --- | ---: | ---: | ---: | ---: |
| index.html | 10,183 | 2,896 | +0 | +1 |
| style.css | 12,818 | 3,707 | +362 | +167 |
| app.js | 41,587 | 11,242 | +0 | +0 |

No new allocation, task, queue, diagnostic buffer or serial output. Linker/static
RAM is unchanged; runtime responsiveness and heap under new hardware load have
not been measured here. Source/asset fingerprints and preservation/whitespace
checks are recorded in `dist/stage6b3-polish/final-audit.json` after review.

### Physical acceptance still required

Retest rapid range dragging, repeated arrow/tab taps, label/fader long press in
iPhone Safari AND Home Screen mode: non-editable UI must not select text. Verify
Go-to-CH editing/selection, software keyboard, copy/paste/select-all, Status
revision/error copying, pinch zoom, scrolling, range pointer capture and keyboard
focus; approved layout/nav must stay stable. Reconfirm real known-channel output,
blackout restore and no replay after background/reconnect. Compare physical
slider in the four load conditions above and retain diagnostics if lag recurs.
Do not treat this CSS fix as a physical-slider latency correction. Keep the
entire Stage6B.3 increment uncommitted until the user accepts the result.


## Final Editor refinement (2026-10-26, supersedes earlier UI descriptions)

User confirms active web faders work on real CoreS3SE/iPhone hardware. This pass
adds only the requested channel UI refinements. Physical display/long-slider
handling, DMX architecture, v1 protocol, queue capacities, recovery, Wi-Fi/NVS,
blackout and preset operations are unchanged. The latency review above remains
separate; these UI changes are not a physical-slider performance correction.

### Banner removal

The entire LIVE CONTROL/preview aside and its monitor-tag/monitor-note nodes
were removed from `web/src/index.html`; `app.js` no longer populates them. Their
unused presentation rules were removed. The banner was not moved or re-created
in the footer. The existing fader feedback footer remains for pending/confirmed/
rejected/unavailable outcomes. The header Connected / Live indicator, Status
health details and disabled/stale control gates are unchanged. The disabled
Blackout button's description now references the existing blackout summary,
without a dangling aria-describedby reference. Preset/blackout remain read-only.

### Fine adjustments and pending-value arithmetic

Each fader now has a 44x44 CSS-pixel `+` button above and a matching minus button
below the native slider. Click is the only adjustment event; there is no custom
long-press handler or repeat timer. DOM/tab order matches the visual + / range / minus order. Both bounds clamp
without wraparound, and buttons disable at the applicable preview/authoritative limit and while stale.

`control.step(channel, delta)` accepts only exactly+1/-1. Its base is the latest
pending intent for that channel if present, otherwise the current authoritative
store selector. Thus starting100, two taps before acknowledgment create101 then
102, not101 twice. Every tap adjusts intended value by one; intermediate network
commands may coalesce under the established rate cap. Acknowledged final state
is102. Opposite taps and boundaries behave equivalently. No second universe or
independent browser ownership/lock is introduced.

`intent(channel,value,active)` is the previous edit implementation extracted
locally. Range dragging calls it with active=true; new `setFinal` and fine steps
use active=false atomically. This avoids manufacturing a second release event
for a button or numeric Apply. Existing range release behavior, one in-flight
write, eight intent entries, final priority, rolling18/s bound and60ms normal
cadence remain. One tap/Apply does not create an automatic duplicate final send.
No WebSocket encoder, server, acknowledgment or reconnect changes were needed.

### Inline numeric draft and cancellation

The displayed number is now a native button, preserving its typography/color,
with a minimum44px target, aria-controls/expanded and an accessible CH/value label.
On activation, that card's rail/steps are temporarily replaced by a compact form
occupying the same height. No large modal and no navigation replacement. At most
one card's numeric draft is open; opening another cancels the former silently.

The form has an explicit Draft label, native text input with inputmode=numeric,
18px font (above the usual iOS focus-zoom threshold), maxlength3, Apply and Cancel.
It retains normal text selection/copy/paste and native focus/keyboard behavior;
there is no viewport or software-keyboard workaround. Field focus uses native
scroll-into-view behavior; returning focus to the value button uses preventScroll.
Native Enter submits Apply; Escape and Cancel discard without sending anything.
Only form submit and Escape use preventDefault, never touch/gesture events.

Raw trimmed input must match1..3 decimal digits and numeric value<=255. Empty,
negative, plus-signed, fractional, exponent/hex, malformed, and>255 values show
an inline validation error and no network command. Leading zeroes are allowed:
000 confirms0. No keystroke/input/change handler transmits draft values.

Incoming authoritative publications update the displayed Controller number but
never overwrite the draft field. Draft text is not a Controller value or pending
mutation. Apply validates and submits one captured-channel `setFinal` through the
same channel.set/Dispatcher/Controller path, closes the draft and shows the local
unconfirmed preview until the established result/publication reconciliation.
Rejection restores authoritative rendering and existing footer error feedback;
no rollback is invented for transport_error and nothing retries automatically.

Bank/responsive replacement first disposes old cards and cancels/clears drafts.
Captured pending button intents remain tied to their original channel and may
complete normally. Detached card handlers are inert, so neither a stale submit
nor a later pointer event can affect a different bank. Leaving Editor cancels
open drafts. Connection loss/resync/background or inactive Sender cancels drafts
and disables mutations; a new snapshot only enables new deliberate interaction.
Confirmed-but-unacknowledged commands are never replayed on reconnect. Closed
fields clear their text; there is no persistent draft/session storage.

### Geometry, gestures and browser memory

Original native slider travel is preserved via --fader-travel: base
clamp(160px,100svh-440px,300px), desktop clamp(200px,100svh-420px,330px), short
landscape150px. Rail height adds104px (two44px buttons plus two8px gaps). Inline
form uses that same rail height, preventing an extra vertical jump when opened.
Validation messages wrap within the card even at the280px minimum width.
Rail margin is12px above/10px below. The removed banner frees vertical space;
remaining content scrolls through the existing primary scroller naturally.

The 4/6/8 columns, dark palette, native range direction/travel, svh shell, safe
areas, persistent bottom nav and tab scroll-reset behavior remain. Numeric Apply/
Cancel have44px minimum heights and width up to76px, fitting narrow cards. CSS
budget tests cover280/320/600/768px widths and verify targets fit without column
overflow; these are source/layout contracts, not a real-browser screenshot test.

Selection exceptions still allow numeric input/diagnostic copying. The old
`.fader input` nonselection/vertical rule is narrowed to input[type=range], so
editable numeric text is not accidentally made vertical or unselectable. The
accepted bank-arrow touch-action rule is unchanged; a separate scoped
`.fader-step, .fader-value { touch-action: manipulation; }` prevents rapid-control
double-tap zoom while preserving pinch zoom. No global touch restriction or
new touch-callout rule.

Browser-only DOM adds two step buttons and one small form per visible card,
with a single active draft. At most eight cards exist, no DOM growth per tap or
per publication. Card replacement drops/disposes old handlers; no new global
listeners, timers, unbounded arrays, firmware task, queue, diagnostic buffer or
NVS allocation. The visible draft is at most3 typed characters per field and is
cleared on close; programmatic invalid values are still rejected before sending.

### Automated validation and current resource delta

- 57 live integration groups PASS (ten new refinement cases) plus navigation,
  bootstrap, all512 bank mappings at4/6/8 and factory isolation.
- New cases:101/102 accumulation, +/-bounds,100 rapid taps and final value,
  numeric Apply/Cancel/Escape, invalid/empty drafts, untouched remote-updated
  drafts, server rejection, loss during drafting and after Apply, no replay,
  disposal/bank identity, single-open draft and CH512 numeric/boundary behavior.
- Eleven pipeline cases PASS: served banner removal/no dangling ARIA targets,
  44px targets, mobile18px font, equal form/rail height, column width budgets,
  selective user-select and scoped touch-action; deterministic composition/gzip
  and16KiB per-asset gates unchanged.
- All21 freshly compiled Controller/Dispatcher/transport/ISR/network/HTTP/asset/
  SoftAP/diagnostic host variants PASS. Integration/source guards, six packaging/
  infrastructure cases and driver patch-state regressions PASS.
- All four builds PASS, receipts match final source/assets. Git diff --check and
  preservation checks recorded in dist/stage6b3-editor/final-audit.json.

Resource delta vs the immediately preceding selection-polish Stage6B.3 build:

| Environment | Result | Static RAM bytes | Flash bytes | RAM delta | Flash delta |
| --- | --- | ---: | ---: | ---: | ---: |
| cores3se_unit | PASS | 87,508 | 1,059,997 | +0 | +952 |
| cores3se_base | PASS | 87,508 | 1,059,965 | +0 | +952 |
| cores3se_unit_diagnostics | PASS | 90,220 | 1,064,529 | +0 | +936 |
| cores3se_base_diagnostics | PASS | 90,220 | 1,064,501 | +0 | +936 |

| Asset | Raw bytes | Gzip bytes | Raw delta | Gzip delta |
| --- | ---: | ---: | ---: | ---: |
| index.html | 9,942 | 2,811 | -241 | -85 |
| style.css | 13,757 | 3,858 | +939 | +151 |
| app.js | 45,705 | 12,104 | +4,118 | +862 |

The largest compressed asset remains below16384 bytes. Static RAM is unchanged;
actual runtime heap/native iPhone touch, software keyboard and zoom behavior
have not been measured by this automated pass. No firmware upload occurred.

### Physical iPhone acceptance (pending)

1. Load the new hashed assets in Safari and Home Screen. Confirm the banner is
   absent everywhere, header status still works, safe areas/nav/scroll are stable
   and pinch zoom remains available.
2. Use a deliberately known safe fixture channel. Tap+ twice rapidly from100:
   intended/final value102 on iPhone, physical Controller, real fixture and other
   browser. Repeat minus/mixed taps, bounds0/255, and a burst under congestion.
   Hold a button: no application auto-repeat. No automatic channel sweep.
3. Tap the number; verify numeric keyboard, readable18px field, no automatic focus
   zoom, selection/copy/paste, Apply/Enter and Cancel/Escape. Try empty,256,-1,1.5.
   Only confirmed valid values may change DMX. Observe rejection feedback if
   induced through existing test tooling, without changing backend policy.
4. Change the channel physically/from another browser while editing: draft text
   stays, displayed Controller number can change, Cancel sends nothing. Apply
   acts as a new serialized absolute command; all peers ultimately converge.
5. Change banks or orientation with a draft and pending fine tap; draft cancels,
   pending value affects only its captured original channel. Test4/6/8 layouts,
   CH512, short landscape, keyboard visibility and bottom-navigation access.
6. Disconnect/background mid-draft and after Apply; new snapshot must not replay
   either draft or old final intent. Recheck existing health-probe silence, manual
   Retry, range dragging and physical preset/blackout restore. The physical long-
   slider latency investigation remains separate and unchanged.

No commit, push, merge, tag/release, flash or NVS erase. Stop for user hardware
acceptance of this Editor refinement before any Stage6B.3 closure commit.


## Final hardware acceptance and Stage 6B.3 local closure (2026-10-26)

The user explicitly confirmed physical acceptance and approved Stage 6B.3 closure.
Accepted functionality on the real CoreS3SE includes:

- Active web DMX channel control and real Controller/DMX operation.
- iPhone touch faders and multi-client authoritative synchronization.
- Channel +/- fine adjustments and direct numeric value entry.
- iOS text-selection prevention, bank navigation and touch behavior.
- Connection recovery with no mutation replay.
- Updated Editor layout, including the removed redundant banner.

These are user-reported real-device results, distinct from the automated tests.
Do not infer newly measured wire timing, runtime heap, four-client capacity or
experimental Base electrical stability from this acceptance. Earlier pending
checklists remain a history/reference, not an outstanding Stage 6B.3 gate.

### Deferred backlog: PHYS-UI-SLIDER-PRECISION

Classification: deferred physical firmware/UI usability improvement; NOT a
Stage 6B.3 blocker. No implementation is included in this closure.

The user reports that the CoreS3SE physical display slider is relatively short.
During slow dragging, DMX values typically jump by approximately 10, with
variations of 1-3. Precise single-value adjustment is not reliably possible,
even with a stylus. This behavior is independent of Wi-Fi/WebSocket client
activity and may have existed before Stage 6B.3. Keep it separate from web
command pacing and the earlier perceived-latency investigation; do not label it
a confirmed Stage 6B.3 network regression or an established root cause.

A future, separately scoped investigation should examine touch sampling,
physical slider geometry, coordinate-to-value mapping and the possible addition
of physical +/- controls. Establish a measured baseline and preserve physical
Controller/Dispatcher/DMX safety when evaluating alternatives. No sampling,
geometry, mapping, physical control, timing or redraw change was made now.

### Closure validation and retained limitations

The complete increment is reviewed against the exact parent above: 20 intended
files (inventory in this report), including the authored sources, reproducible
composed bundle/gzip manifest, tests and this report. The installed v1 channel.set
path is reused; no web preset/blackout mutation, new phase, protocol/queue limit,
partition, Wi-Fi credential, NVS or DMX driver change is included. The generated
bundle/assets must be derived exactly from the reviewed authored inputs.

Final closure reruns PASSED: 57 frontend live groups plus navigation/foundation and
512-channel bank coverage; 11 pipeline cases; all 21 freshly compiled host
variants; six infrastructure/packaging cases; integration/source guards and
reproducible driver patch-state checks; all four normal/diagnostics Unit/Base
PlatformIO builds; deterministic regeneration and Git whitespace checks.
Actual outcomes and final build/source/asset identities are recorded in ignored
`dist/stage6b3-closure/`, with the resource tables above describing the accepted
source. A focused staged inventory and staged whitespace check are required
before the local commit.

The historical synchronous ESP-IDF/lwIP WebSocket send-stall limitation remains:
a blocking send can exceed its nominal 100ms frame budget, delay other HTTP
sessions and contribute to bounded outbox saturation/closure. Stage 6B.3 does
not establish a permanent fix or attribute the prior incident to a particular
station from timing alone. Genuine failure counters and recovery remain intact.
Final values may wait for rate credit or an outstanding ack; an uncertain
already-admitted command is reconciled by a new snapshot, never automatically
replayed. Application acknowledgment is not proof of electrical DMX delivery.
Base remains experimental; external RS-485 frame/timing/coherence measurement
and current-runtime resource measurements remain separate acceptance evidence.
Web preset/blackout controls remain read-only; no new per-command authorization
system is provided beyond the existing documented network/security boundaries.

The user authorized one local commit with message:

```text
Stage 6B.3: Add active web DMX channel control
```

Preserve the stash and unrelated historical files. No push, merge, tag/release,
firmware flashing/erase, NVS operation or Stage 6B.4 implementation is part of
closure. Stop after the local commit and final report.
