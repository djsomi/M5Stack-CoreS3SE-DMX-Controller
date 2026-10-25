# Stage 6B.0: Web UI architecture and implementation plan

Planning baseline: `294b8b8ea21d139555a7222f16b5eeff00d16091`, branch
`codex/stage6a-websocket-controller`, 2026-10-25. This document is the only new
project file in this step. No frontend, firmware, generated assets, protocol,
partitions or NVS changes are implemented or committed.

## 1. Product scope and recommendation

Build an offline, responsive, modular vanilla-JavaScript interface over the
existing Stage 6A protocol. Keep exactly one device Controller; the browser holds
a confirmed read-only projection plus clearly separate, temporary input drafts.
Two simultaneous clients are the normal performance target. Retain the server's
maximum of four without locks, master browsers, accounts or roles.

Use a restrained dark visual language from the supplied Luminair-inspired brief:
near-black canvas, charcoal panels, subtle borders, rounded controls, blue
selection and a clearly red active Blackout. Use system fonts and small inline
SVG or CSS icons; no external font/image assets, console-style programming
features, fixtures, patching, effects, scenes, chases, timelines or cue stacks.

Keep the requested five implementation phases and names. Two scope corrections
are needed: 6B.1 includes asset-bundle preparation and narrowly updated asset
test guards; 6B.5 provides read-only device information and browser preferences,
not unsupported device-setting writes. Wi-Fi credential editing is deferred.

## 2. Inspected infrastructure and actual limits

Source of truth: `docs/STAGE6A_WEBSOCKET_IMPLEMENTATION.md`, actual
`src/websocket_protocol.cpp`, `src/websocket_bridge.cpp`, `src/network_service.cpp`,
`src/http_policy.h/.cpp`, `include/web_assets.h`, `scripts/web_asset_pipeline.py`,
`scripts/embed_web_assets.py`, `scripts/package_firmware.py`, `platformio.ini`
and `partitions/cores3se_16mb_ota.csv`. Earlier Stage 4 proposals are background;
Stage 6A's implemented schemas supersede their proposed APIs/field names.

| Existing capability | Consequence for the UI |
| --- | --- |
| Arduino ESP32 2.0.16 / IDF 4.4.7, espressif32 6.7.0 | No framework/toolchain migration |
| 16 MiB layout; two 6 MiB application slots; 8 MiB PSRAM | Keep layout; no filesystem needed for assets |
| Exactly three inputs: web/src/index.html, style.css, app.js | Keep three served assets; authored JS modules need build-time composition |
| Generated HTML substitutes STYLE_URL and SCRIPT_URL tokens | Preserve each token exactly once and existing hashed URLs |
| gzip assets + manifest, fixed Python 3.11.7 / zlib 1.2.13 regeneration | Commit regenerated assets when implementing; normal clones only verify |
| Generator limit 256 KiB/source and 256 KiB gzip bundle | This is not the practical HTTP per-file limit |
| HTTP compressed asset cap 16,384 bytes; TX budget 20,480 bytes/session; 3 s lifetime | Every served gzip asset must fit; allow header margin and test real load times |
| Six HTTP sessions, four WS clients, three extra reserved SDK sockets | Preserve socket budget; share one WS across all sections in each tab |
| Shared 12 HTTP/upgrade requests/s window | Avoid polling/reload storms; four simultaneous cold page loads can exceed it |
| WS text max 256 bytes; 20 text attempts/client/s, 24 frames including controls | Throttle all client JSON through one scheduler |
| Four unresolved incoming requests/client; one web mutation admitted globally | Prefer one client mutation in flight; do not equate queued with applied |
| Eight outbound events/client; bounded round-robin pump | Avoid command floods; no server queue/rate changes in this plan |
| Strict Host/Origin, CSP, flash-backed gzip, no CORS wildcard | Run on device origin; no inline handlers, eval, CDN or policy relaxation |

Stage 6A hardware baseline: CoreS3SE + Unit DMX, macOS Chrome, two clients,
both revision 296; 550 enqueued/taken/sent updates, outbox high-water 3,
zero queue-full/send/Wi-Fi/work failures and first failure null at uptime
139,798 ms. This is not a four-client, long-soak or electrical waveform result.
The previous Windows AX200 setup had 26-30% packet loss; do not change firmware
timeouts/queues speculatively to accommodate that unstable network.

## 3. Responsive layout and navigation

Tablet is the reference; desktop uses a centered container with maximum width
1,024 CSS px. The shell owns a persistent connection indicator and explicit
Blackout control. Editor, Presets, Settings and Status are hash-based sections
(`#editor`, etc.), not server paths: GET /editor is not an existing route.

| Viewport | Initial target |
| --- | --- |
| Phone below about 480 px | Four faders; 12-16 px gutters; bottom section navigation |
| Wide phone/small tablet about 480-679 px | Six faders when targets fit comfortably |
| Tablet from about 680 px | Eight faders; same four sections, more panel spacing |
| Desktop | Eight faders in the centered tablet-like shell; no ultrawide expansion |
| Short landscape phone | Width-based count, shorter tracks around 144-180 px; allow vertical scrolling |

Treat breakpoints as prototype defaults, not fixed product requirements.
Targets should be at least 44 CSS px, preferably 48; faders must have generous
hit areas. Use grid columns with minmax(0,1fr), constrained dialogs, safe-area
padding and wrapping text. No horizontal document overflow at 320 px or 200%
zoom. Permit normal page scrolling outside fader tracks. Respect reduced-motion
preferences; no animated DMX value interpolation that conceals actual state.

Tablet arrangement:

```text
CoreS3SE DMX       Connected / Synchronizing / Offline       BLACKOUT
CH 001-008        Previous    Go to CH [___]    Next
CH001 CH002 CH003 CH004 CH005 CH006 CH007 CH008
 127   0     0     255    0     0     0     0       confirmed values
 [|]   [|]   [|]   [|]   [|]   [|]   [|]   [|]      vertical faders
Editor             Presets            Settings           Status
```

Editor banks contain the visible 4/6/8 absolute channel numbers. Next/Previous
move by that count; Go to CH validates 1..512 and reveals its bank. Partial last
banks show only actual channels, never 513+. Recompute around the selected
channel on resize; bank changes never change DMX values. Selection is local to
this browser and does not change touchscreen selection.

Only visible faders exist in the DOM; keep all 512 confirmed values in memory.
Display `CH 021` and an integer 0..255. An optional browser-local percentage is
secondary, derived display only. Tapping a value opens an integer-entry dialog
with explicit Apply/Cancel. The device has no channel names to rename.

## 4. Modules and data ownership

Proposed authored files, created incrementally rather than all at once:

```text
web/src/index.html                 semantic shell, loading/error fallback
web/src/style.css                  tokens, layout, controls, responsive sections
web/src/js/protocol.js             exact v1 decoding/encoding, pure validation
web/src/js/transport.js            one socket, lifecycle, reconnect, epoch
web/src/js/state.js                private confirmed universe and metadata
web/src/js/commands.js             IDs, bounded scheduling, results, drafts
web/src/js/preferences.js          versioned browser-only preferences
web/src/js/fader.js                reusable pointer/keyboard widget
web/src/js/editor.js               banks, jump, values, editor interactions
web/src/js/presets.js              P1-P8 and explicit operations/dialogs
web/src/js/settings.js             browser preferences, read-only device facts
web/src/js/status.js               bounded HTTP reads and diagnostics display
web/src/js/shell.js                navigation, connection/blackout presentation
web/src/js/app.js                  dependency wiring and bootstrap only
web/src/app.js                     generated, checked-in single runtime bundle
scripts/web_ui_bundle.py           deterministic standard-library concatenation
tests/web/run_tests.mjs            actual JS logic tests with built-in Node tools
tests/web/browser_harness.html     test-only DOM/interaction harness
tests/web/fixtures.js              bounded real-protocol fixtures/fake transport
tests/web/check_pipeline.py        bundle freshness/limits and scope checks
tests/web/README.md                reproduction and real-device matrix
```

Use simple factory functions and explicit dependencies, not a generic event bus,
plugin registry or framework. Source files are ordinary JS factories composed
in a fixed reviewed order inside one strict IIFE. Only the bootstrap touches the
live application at startup. No runtime import graph or import/export rewriting
is required. Tests can load the same factory sources into a controlled context.

| Boundary | Owns | Must not do |
| --- | --- | --- |
| Transport | socket, reconnect timer, connection epoch, raw messages | Infer applied values or replay commands |
| Protocol | bounded DTO validation and exact request fields | DOM work, sockets, domain mutation |
| Confirmed state | private Uint8Array(512), blackout, sender, presets(8), revision/seq | Predict blackout restoration or persist a universe for later replay |
| Commands | IDs, pending results, one active draft, bounded unsent intents | Mutate confirmed state or treat send() as an ack |
| Views/widgets | DOM, focus, gestures, temporary target indicator | Send from render/subscription callbacks |
| Preferences | bank/selection and optional percentage/density choice | Store device presets, credentials, pending commands or authoritative values |

Render subscriptions are read-only. Only explicit input handlers create intents.
Keep the confirmed typed array private; expose selectors/copies, not writable
references. Validate a full snapshot into temporary storage before swapping it
atomically and notifying views. Batch DOM writes with requestAnimationFrame;
apply every ordered state event to the store, even when only the latest visible
render is painted. Disable or defer inactive-view rendering and remove listeners
on view teardown without creating another WebSocket.

## 5. Exact protocol contract

Use `type`, not the earlier proposed `op`. All requests include `v:1`, a nonzero
strictly increasing unsigned 32-bit `id`, and only these operation fields:

| Type | Additional fields |
| --- | --- |
| state.request | none |
| channel.set | channel 1..512, value integer 0..255 |
| blackout.set | active boolean |
| preset.load / preset.save / preset.clear | slot integer 1..8 |
| preset.copy | slot and target integers 1..8 |

The server converts preset slots to internal 0..7. There is no writable start
code, selection, UI brightness/volume, Wi-Fi settings, receiver control or
arbitrary method API. Validate integers before serialization; reject NaN,
Infinity, signs/floats/exponents in numeric-entry text and extra request fields.
Measure UTF-8 bytes before send; all supported commands fit the 256-byte limit.

Server messages always include v/type/id/revision/seq. Revision and seq are
decimal strings; parse bounded unsigned 64-bit values with BigInt. Request IDs
remain numbers. Never coerce revisions to Number or require contiguous seq.

- `hello`: v1, channels=512 and limits including max_outstanding=4.
- `state.snapshot`: exactly 512 working values, blackout/sender booleans and
  eight preset states. It is sent automatically after hello. A requested
  snapshot carries its request ID and has no separate command.ack.
- `state.update`: single channel/value, full values+presets, or mode-only;
  blackout and sender are included. Apply only fields present. Full updates can
  originate from preset metadata operations too; they do not identify an actor
  or expose a command/change-kind field.
- `command.ack`: applied or no_change, domain code and final revision. State
  changes precede the ack in that client's stream. This confirms Controller
  application/transport staging, not fixture receipt.
- `command.error`: status/domain and possibly reason. An error with revision/seq
  zero is not a state reset. A transport_error can follow a real state change;
  do not locally roll it back. Unknown/id=0 errors must not free another ID's
  pending request or alter confirmed values.

Preset state codes are 0 missing/logically cleared, 1 valid, 2 corrupt,
3 storage/metadata error. Preserve all four meanings. Domain result codes are
the existing 0..11 enum documented in Stage 6A; show an operation-specific error
and offer raw details in diagnostics instead of relabelling every failure busy.

No boot ID, hardware profile, firmware semver/commit, asset hash or credential
mode is supplied in hello/snapshots. Do not invent or depend on them. A browser
connection epoch is local bookkeeping, not an added wire field.

## 6. Connection, synchronization and uncertain results

Transport states: disconnected -> connecting -> synchronizing -> ready, with
distinct error/reconnecting states. Opening a socket alone never enables writes.
Wait for valid hello and full snapshot. Incoming state seq must increase and
revision must not decrease within the epoch; mode-only updates may reuse a
revision. Gaps in seq are normal. A malformed/mismatched initial schema disables
controls and shows a reload/unsupported-protocol error, not a retry storm.

On loss, stop producers, cancel gestures and unsent work, mark sent/unresolved
commands uncertain, retain at most one dimmed last-known projection and disable
all mutations. Reconnect the transport only, using one bounded backoff timer
(initial 0.5 s, doubling to a 10 s cap with jitter and a manual Retry option).
Reset IDs per new connection and wait for a new authoritative snapshot before
enabling controls. Old socket callbacks/timers/results are ignored by epoch.
Never resend old channel/final/blackout/preset intents after a disconnect.

Proposed browser watchdogs: 5 s to connect/synchronize and 5 s for a result.
These are UI uncertainty thresholds, not changes to server timing or proof that
a command failed. A timed-out sent command might still apply. Pause new writes;
keep its ID for a late result, and allow read-only status inspection or deliberate
reconnect. Do not treat a snapshot/value match as proof that an uncertain Save,
Copy, Clear or Load succeeded. A same-session snapshot is not cancellation of a
previously admitted command. Stage 6A may defer it behind the pending mutation.

On backgrounding, stop polling/render work and cancel active input; keep the
single WS where the browser permits. On return, require readiness and reconcile
with a bounded state.request or a fresh connection/snapshot. Do not catch up
expired command timers or replay drafts. Server control PING/PONG needs no JS
JSON heartbeat. Mobile suspension can legitimately require resynchronization.

## 7. Faders, final values, backpressure and blackout barriers

Recommended initial policy: one active fader gesture and one mutation in flight
per tab. Use a global per-connection minimum 100 ms spacing (10 JSON requests/s)
for slider samples and other requests together, below the server's 20/s bound.
Do not run independent per-fader timers. Reserve at most one extra read-only
state request; total pending IDs must remain within the advertised limit four.

Bound client memory to one replaceable unsent drag sample, one nonreplaceable
final sample, and at most four ordered discrete intents. Disable conflicting
actions if those slots are occupied; never grow a queue. Wait for socket OPEN,
ready synchronization, a result credit and a drained bufferedAmount before
transmitting. bufferedAmount describes queued bytes, not server application.

For an ordinary healthy gesture:

1. Capture absolute channel/pointer/connection epoch and context at pointerdown.
   A large hit area and pointer capture keep the gesture stable outside the rail.
2. Compute/clamp a temporary 0..255 target. Show a clearly distinct target thumb
   or pending label; the primary value remains the confirmed device value.
3. Replace only unsent samples from that same gesture/context. Already sent
   messages and discrete operations are never overwritten or reordered.
4. On pointerup, record the exact final clamped value as a nonreplaceable final
   intent. If throttled/busy, retain it and send at the next eligible turn; do
   not drop the last value merely because no later pointermove occurs.
5. An applied/no_change result resolves the operation. A definite busy rejection
   may retry that absolute final value with a new ID at most twice after delay,
   only in the same live epoch/context. Other errors are explicit. Timeout or
   disconnect is uncertain and must never auto-replay.

pointercancel, unexpected lost capture, orientation/context change or navigation
during editing ends the gesture without fabricating a pointerup. Normal bank/
view changes wait for an existing final intent or require explicit cancellation.
Show a visible cancelled/not-applied result if safety/context loss invalidates
an unsent final; do not claim an unconditional delivery guarantee on a broken
connection. Keyboard arrows, Home/End and numeric Apply use the same absolute
command path and bounds. Never change a fader solely in response to wheel/scroll.

Blackout is always explicit blackout.set(active), formed from confirmed state,
not a network toggle. A pending Blackout operation is visibly pending and not
toggleable again until resolved. Keep the control visible in every section.
An active banner explains: output is zero; working edits during blackout will
be discarded when blackout is released. Do not zero the browser's working array
or implement its own restore snapshot. OFF's authoritative full update restores
the displayed values. Save during blackout saves working values, not zero output.

Treat blackout/mode changes and a bulk universe replacement as input-context
barriers. Stop the active gesture and discard only unsent obsolete drafts, with
visible cancellation. Do not send a stale blackout-era target after release.
A full update with unchanged values/pure preset metadata is not automatically
a preset load. Sender=false disables mutations. Locally finalized work and
explicit operations retain FIFO order; no coalescing crosses a barrier or a
preset operation. Accepted operations cannot be cancelled by this UI. Another
client may change blackout while a command is already in flight; the existing
Controller's serialized order wins, and v1 has no compare-and-swap/epoch command
guard. State this limitation rather than promise cross-client transactional edits.

## 8. Presets, Settings and Status responsibilities

### Presets

Show P1-P8 with the actual occupancy code. Selecting a tile is local selection;
explicit Load applies it live. Save confirms replacing an occupied slot and
states it saves the current working universe; Clear confirms its selected slot.
Copy selects a valid source and a missing target. The current backend rejects
occupied targets: do not invent a copy-overwrite flag or automatically chain
Clear+Copy. Corrupt/error slots are not empty. A deliberate confirmed Save may
replace a corrupt slot where the existing Controller permits it.

On blackout, explain Load changes working values while output stays zero and
OFF restores the entry snapshot. Confirmed dialogs capture slot/operation but
do not assume another client cannot change occupancy before application. Save
captures the Controller's working values when applied, not a browser-frozen
universe from the moment its confirmation opened. Handle
empty/occupied/corrupt/storage failures from the actual result. Disable repeated
submits while an operation is pending. A snapshot refreshes the server's current
projection; it does not promise a new flash scan or repair of cached NVS errors.
No names, import/export, backups, new storage format or browser preset database.

### Settings

There is no remote device-setting write API. Show only read-only network facts
already exposed and clearly labelled browser-local preferences (bank/selection,
optional percentage/density choice). Storage failure/private mode must degrade
to in-memory defaults. Do not persist a universe or pending command.

Device brightness, display timeout, speaker volume, restart/shutdown and Wi-Fi
reset remain on the physical device. No fake toggles or disabled mock settings.
Wi-Fi credential editing is deferred by the current request. The older future
factory-password warning/Action-needed design also needs a credential-mode API;
it cannot truthfully be implemented now. A generic network-access advisory may
be shown without claiming the current password is factory/custom. Do not expose
passwords, infer credential mode or add a settings endpoint in Stage 6B.

### Status: exact existing data sources

| Display | Actual source / qualification |
| --- | --- |
| Browser connection and sync | Local transport state and confirmed-store readiness |
| AP phase/IP, Wi-Fi station count | /status.phase, ip, stations, max_stations |
| WS sessions / configured capacity | /status.websocket.clients, max_clients; distinct from stations |
| Sender/blackout state | WS sender and blackout; sender=false does not specify a particular other mode |
| DMX software faults | /status.tx_errors, tx_stopped; no wire-integrity/fixture-delivery claim |
| Owner gap and stacks | max_owner_gap_us, worker_stack_hwm, http_stack_hwm; lifetime/raw reported metrics, not measured DMX frame rate |
| Memory | samples event 92: internal, minimum, largest, dma (largest DMA block), psram; top-level dma_free |
| Sample time | sample.ms is device sample uptime, not browser wall time or socket lifetime |
| Advanced session/work/closure counters | /ws-diagnostics, user-requested inspection |
| Firmware version/profile/layout | Not exposed: omit or label not reported; /status.stage=5 is a legacy marker, not a firmware version |

The C++ asset Bundle contains profile/source/layout strings, but they are not
an HTTP/browser API. Do not hard-code Unit, fabricate a device commit, or infer
hardware from the common UI bundle. Base remains experimental in documentation.

Poll /status at most once every five seconds only while Status is visible, with
one fetch in flight and a bounded timeout; pause hidden-tab polls. Read advanced
diagnostics on demand. Never poll all 512 channels over HTTP or use a high-rate
status loop to drive faders. Failures produce stale/unavailable labels, not an
assumed healthy/zero state. Unknown native eviction/Wi-Fi reason fields remain
unknown. Two clients are normal; four-client initial load must be staggered or
explicitly tested against the existing global HTTP request/socket limits.

## 9. Asset build and packaging

Keep three generated served assets and the existing HTTP server. Add a small
standard-library `web_ui_bundle.py` helper with a fixed ordered list of the JS
factory files. Concatenate canonical UTF-8/LF inside a strict IIFE, with safe
separators and deterministic comments; no minifier, transpiler, eval or regex
module-language transformation. app.js is generated and checked in, not edited
by hand. CSS remains one sectioned conventional stylesheet.

Minimally extend web_asset_pipeline.py: explicit --generate composes app.js
before the existing gzip/manifest generation; verify recomputes the expected
bundle without writing and rejects missing modules or stale app.js. Checking
only the three old inputs would miss edits to unused/stale module sources, so
module freshness verification is mandatory. Normal PlatformIO builds must not
regenerate source files or depend on Node/npm/internet for UI assets.

Reuse existing HTML URL substitution, SHA-256 naming, deterministic gzip, MIME/
encoding metadata, flash C++ arrays, linker retention and artifact/source/layout
receipts. The recursive source digest already includes nested web sources.
The pre-hook already verifies assets before compiling; authored-module changes
must fail clearly unless the checked-in bundle/gzip/manifest are regenerated.

Initial gzip targets: JS <=12 KiB, CSS <=6 KiB, HTML <=2 KiB. Hard gate each asset
<=16 KiB and HTTP response budget with headers; preserve the existing broader
generator limits. Test cold cache over the actual SoftAP within the three-second
HTTP lifetime. If the real implementation exceeds the limit, pause and propose
a separately reviewed static chunk/table extension; do not silently enlarge
server caps or add dynamic module requests.

Browser navigation uses hash fragments. Preserve root no-store and immutable
hashed CSS/JS cache policy. No service worker/offline cache layer, dynamic
imports or API cache. The device's local AP supplies all assets without Internet.
Provide static loading/reload guidance in HTML if the script request fails
(e.g. cold-load HTTP 429); do not rely on a JS handler that never loaded.

Assets remain inside each application firmware image. Reuse the current audited
separate-image package/manifest/checksum process; package_firmware.py explicitly
does not create old merged FULL.bin files. No filesystem image/upload, SPIFFS/
LittleFS use, partition change or NVS reset is needed. Any future release merge
is a separate approved operation, not part of this UI plan.

## 10. Incremental phases and gates

Common gate after each implementation phase: scope/whitespace review, all
existing 16 host cases, nine integration/asset/patch/infrastructure checks plus
new frontend tests, deterministic bundle/gzip checks and all four established
PlatformIO builds. Preserve driver/domain/partitions/USB/PSRAM framebuffer.
Hardware work requires that phase's separate authorization; none happens now.
Use controlled networks first; log packet loss separately from firmware errors.

### 6B.1: UI Foundation

Implement theme, semantic shell, four hash sections, responsive bank/fader shell,
accessible focus and loading/error presentation. Controls stay disabled without
real synchronization; no fake production values. Add deterministic module
composition and size/freshness checks now, before UI growth.

Existing tests freeze all web files against Stage 5 and freeze pipeline scripts.
Narrowly update those asset-specific guards for approved Stage 6B files and add
new scope/bundle tests; retain exact Stage 6A firmware/driver/partition guards.
Do not disable ownership/transport tests just to permit UI work.

Automated gate: reproducible output, source-only module edit detected as stale,
missing/reordered module failures, actual HTTP asset/encoding/budget tests,
no external URLs/dependencies/inline handlers. Visual gate: 320/390/600/768/1024/
ultrawide widths, phone landscape, zoom, focus and no horizontal overflow.
Hardware gate: normal Unit page cold/warm loads fully offline under current CSP;
no DMX commands, new faults or unexpected heap decline. Check two concurrent
cold loads; stagger four while documenting request-limit behavior.

### 6B.2: Live WebSocket State

Implement one transport/epoch per tab, protocol validation, confirmed store,
initial-sync gating, result tracking, reconnect and read-only live rendering.
Only state.request is exposed to the page here; no live editing yet.

Automated gate: actual v1 fixtures, single/full/mode updates, BigInt ordering,
seq gaps, revision regression, invalid schemas, state.request completion,
error seq zero, no-change ack, late/old-epoch events, reconnect and uncertain
results. Applying any number of incoming updates must produce zero DMX commands.
Hardware gate: touch changes and physical preset/blackout updates reach two
browser projections; idle >60 s, background/resume and manual disconnect recover
only from a fresh snapshot, with no replay and no new queue/send/work faults.

### 6B.3: DMX Editor

Enable banks/jump, faders, numeric Apply, keyboard input and persistent Blackout
through the common scheduler. Implement final-release retention and explicit
pending/confirmed/cancelled displays. No settings/preset mutation features yet.

Automated gate: channels 1/512, last partial bank, value 0/255, invalid entries,
pointer capture/cancel, final release with no later move, throttling and bounds,
definitive busy vs uncertain timeout, simultaneous client conflicts, navigation/
resize, blackout/barrier transitions and no draft replay after reconnect.
Hardware gate: real phone/tablet/desktop touch/mouse/keyboard on two clients;
fast/slow gestures and exact release values reach fixtures; touch/web changes
stay synchronized. Test ON/edit/OFF restoration and an external client's OFF
during another drag. Record queues, send/work/transport faults, heap/HWM and
owner gap. Verify CSSOM geometry updates under the unchanged CSP; do not add
unsafe-inline to bypass a UI defect. Four clients are a stress gate, not a new
hardware capacity claim until measured.

### 6B.4: Preset Management

Enable P1-P8 selection and explicit Load/Save/Copy/Clear with occupancy-specific
states and confirmations. Use the existing protocol/NVS semantics only.

Automated gate: all four slot states, full 512-value loads, no-change results,
occupied-target copy errors, overwrite/clear confirmation cancellation, storage
errors, stale confirmation vs another client, uncertain outcomes and blackout
working-value saves/restoration. Never silently chain destructive operations.
Hardware gate: only user-approved disposable slots; browser and physical load
synchronize every channel across two clients and real fixtures. Test save,
missing-target copy, clear and load while blacked out. Confirm existing presets
remain readable and no format migration or unexpected key-log spam occurs.

### 6B.5: Status, Settings, Final QA

Finish bounded status/advanced diagnostics, truthful unavailable/stale fields,
local preferences and graceful localStorage failure. Keep credential changes,
credential-mode-specific warning flows and device-setting APIs deferred.

Automated gate: exact endpoint fixtures/field meanings, one visible-page poll,
no hidden-tab polling storm, timer/listener/socket cleanup, errors/unknown
values, preference schema corruption and no saved universe/commands. Final
bundle/reproducibility/size/scope and all normal/diagnostic build gates pass.
Hardware gate: two-client sustained editing plus Status, background/resume,
reconnect, touch Settings/sleep/battery/speaker/blackout/presets and HTTP reloads.
Collect comparable memory/stack/queue/owner-gap/transport evidence and a longer
soak; attempt three/four clients and document the actual outcome. Check macOS
Chrome and intended phone/tablet Safari/Chrome; test Windows only on a separately
verified stable Wi-Fi path. Real Base validation is separate and stays
experimental. External RS-485 capture remains necessary for electrical timing,
all slots, coherence, break/MAB and frame-gap claims under UI load.

## 11. Dependencies, risks and remaining decisions

Runtime dependencies: browser DOM/CSS/Pointer Events/WebSocket/BigInt only,
system fonts and local assets. Asset build: existing Python/zlib plus the small
standard-library composer. No frontend framework, npm install, Node server,
CDN, Google Fonts or Internet requirement. Node built-in tests may be a developer
test tool; they are not required by a fresh-clone PlatformIO firmware build.
Keep DOM testing in a separate offline harness, not a bundled fake device.

| Risk | Planned control |
| --- | --- |
| Browser mirror becomes another Controller | Read-only confirmed store; separate drafts; no local restore/preset engine |
| Final drag sample lost by throttling | Nonreplaceable final; bounded scheduling and explicit failure/cancellation |
| Old accepted command applies after loss | Uncertain state, no replay; fresh snapshot and subsequent authoritative updates |
| Blackout/preset changes interleave with stale drafts | Context barriers; cancel unsent stale intents; preserve accepted order |
| Slow clients / lossy Wi-Fi / main-thread stalls | Modest producer rate, one mutation in flight, bounded logs, minimal DOM, honest disconnect status |
| HTTP/socket limits under four cold loads | Three served assets, conservative polling, staged loads and explicit 429/reload handling |
| UI outgrows HTTP asset limit | Budget from phase 1; measured gzip gate; review chunking separately if necessary |
| Device heap/CPU regress despite flash-only assets | Compare real /status and /ws-diagnostics under two clients; no firmware tuning by assumption |
| Unavailable settings/provenance presented as real | Field mapping above; omit unsupported controls/identifiers; no API additions |
| Predictable Wi-Fi password permits remote DMX control | Preserve approved access model; explain tradeoff, no invented authentication layer |

Decisions for plan review, with recommended defaults:

1. Accept the 4/6/8 responsive bank targets and one active fader gesture/tab;
   refine dimensions on real devices before adding multitouch complexity.
2. Accept confirmed-value plus distinct target/pending indication, 10 requests/s
   and one in-flight mutation initially; tune only from measured device evidence.
3. Accept explicit Blackout action with pending lockout, no extra modal; retain
   confirmation for preset overwrite/Clear and missing-only copy targets.
4. Confirm first browser support targets (recommend current macOS Chrome,
   iPhone/iPad Safari and Android Chrome); legacy BigInt/Pointer Events support
   is not promised without testing or a separately approved fallback.
5. Accept browser-only preferences/read-only Settings and deferral of Wi-Fi
   editing/factory-mode warnings until a later API scope is approved.

No blocker requires changing Stage 6A to begin 6B.1. These are reviewable UI and
test-policy choices, not permission to change firmware or implement now. Keep
the Stage 6A commit as the rollback baseline; preserve each phase's working
files/evidence rather than reset/stash/discard. No branch change, commit,
push, merge, firmware build/upload or release is performed in Stage 6B.0.

Browser API references: [WebSocket bufferedAmount and send semantics](https://websockets.spec.whatwg.org/#dom-websocket-bufferedamount)
support treating buffered bytes separately from an application result;
[Pointer Events cancellation/capture](https://www.w3.org/TR/pointerevents3/#the-pointercancel-event)
supports distinguishing cancellation from a successful release. The project
protocol and safety semantics above come from the inspected Stage 6A sources.
