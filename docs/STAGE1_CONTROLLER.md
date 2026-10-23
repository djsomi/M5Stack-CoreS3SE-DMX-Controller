# Stage 1: Authoritative DMX Controller

## Scope and Baseline

This stage extracts state/domain operations only. There is no Wi-Fi, WebSocket,
filesystem, partition change, new task, command queue, or transport redesign.

Development branch: `codex/stage1-controller`, based on main/origin/main
`32c0571049fa57c584167f5a4f6f07e67668430d`, carrying the tested, uncommitted Stage 0
work. The existing stash, historical development folder, and hardware logs are
untouched. Local ignored `dist/stage1-stage0/` holds pre-extraction copies of the
Stage 0 files for comparison; it is not a committed backup or release artifact.

Stage 0 measured physical flash 16,777,216 bytes, configured flash 8 MB,
initialized PSRAM approximately 8 MB, internal free heap 161,484 bytes after
shared canvas allocation, and largest DMA block 131,060 bytes. Shared canvas
pointer was `0x3fca30d0`; tested transitions showed no persistent extra RAM use.
Preset save/load passed. These are historical measurements, not measurements of
the refactored firmware. External DMX waveform/coherence validation is pending.

## Ownership and API

`src/dmx_controller.h` and `.cpp` contain the portable `DmxController` and injected
`DmxPresetStorage` interface. `src/src.cpp` constructs one storage adapter, one
Controller, and the sender view referencing that Controller, in that order.

Controller owns:

- One 513-byte working packet: start code zero, channels 1..512.
- Blackout state and exact blackout-entry restore snapshot.
- Validated channel writes and clamped relative adjustments.
- Preset load/save/copy/clear and the existing logical cleared-slot mask.
- Monotonic 64-bit revision and a synchronous typed change observer.

Public API:

| Interface | Contract |
| --- | --- |
| `channel(int)` | Read channel; invalid channel returns zero. |
| `setChannel(int, int)` | Reject indices outside 1..512 and values outside 0..255. |
| `adjustChannel(int, int)` | Validate channel; clamp resulting value to 0..255. |
| `workingPacket()` | Borrowed, read-only current working packet. |
| `outputPacket()` | Borrowed, read-only zero packet during blackout; working packet otherwise. |
| `blackoutActive()`, `setBlackout(bool)` | Snapshot on entry, restore on exit; repeated same state is a no-op. |
| `presetState(uint8_t)`, `presetExists(uint8_t)` | Slots 0..7 map to P1..P8; apply logical occupancy mask. |
| `loadPreset`, `savePreset`, `copyPreset`, `clearPreset` | Explicit result codes; no driver calls or UI effects. |
| `revision()`, `setObserver(callback, context)` | Synchronous application-task notification after successful state changes. |
| `reset()` | Zero working/restore state, exit blackout; preserve persistent preset metadata. |

Packet pointers are borrowed views, not immutable snapshots across later writes.
Controller is noncopyable. The application task exclusively owns all access,
including storage queries and observer execution; no concurrency is introduced.
Observers must not recursively mutate the Controller. Notifications are not yet
a network protocol, event history, or queued command processor. Future network
callbacks must submit commands to the application task, not call this API.

Channel no-ops and repeated blackout state do not advance revision. A load advances
revision only when working values change. Successful save/copy/clear advances
revision for preset state. The observer refreshes the sender's selected-value
cache; it neither draws nor publishes to the DMX driver.

UI-local state remains in `view_sender.h`: selected channel/index, selected-value
display cache and same-loop edit draft, scroll/visibility, last-painted grid values,
STEP session state, hold repeat, keypad input, touch consumption, fullscreen page
ownership/canvas, navigation, preset selection/prompts, battery/audio/sleep UI,
and blackout double-tap gesture state. The last-painted array is only a render
shadow, never an alternative working universe.

## Publication and Behavior

Existing sender handlers still explicitly call the existing `dmx_write_packet()`
wrapper after a main edit, Precision/keypad edit, preset load, and blackout toggle.
The wrapper reads `Controller.outputPacket()`. Initial sender setup still writes
the initial packet. No write is automatically dispatched from the observer.

The send-ready checks, send calls, sleep/wake transmission path, DMX mode/pins,
driver code, scheduling, and restart/shutdown zero-before-power sequence remain
unchanged. The transient shutdown zero packet is a safety publication, not a
mutation of the authoritative working state. Receiver buffers remain RX-local.
These are the intentional remaining direct driver operations outside Controller.
There are no other application writes to the authoritative working packet,
blackout snapshot/state, or preset metadata. NVS bytes are written only by the
injected storage adapter when called by the Controller.

During blackout, edits and preset loads change working values while output stays
zero. Save captures working values, not zero output. Exiting blackout restores
the exact entry snapshot, discarding intermediate edits/loads as before. Preset
copy does not alter working/output state. Touch gestures, sounds, rendering
geometry, battery thresholds/volume restoration, STEP behavior, and storage schema
are not intentionally changed.

## NVS Missing-Key Logging

The old preset occupancy path called `Preferences::getBytesLength()` every time
the screen queried a slot. Arduino 2.0.16 logs `ESP_ERR_NVS_NOT_FOUND` at error
level for absent blobs, so repeated queries of empty `p4`/`p6` produced repeated
logs. That is not evidence of corruption by itself.

The adapter now uses read-only `nvs_open` and `nvs_get_blob(..., nullptr, &size)`.
Only missing namespace/key is treated quietly as normal absence. Type mismatch,
wrong blob length, namespace failures, and other NVS errors remain explicit logged
errors. No global log-level change or occupancy cache was added, so no cache
invalidation/stale-occupancy problem is introduced. The metadata mask is still
loaded lazily once, as in the old UI.

Schema is unchanged: `dmxpreset/p1` through `p8` are 513-byte blobs;
`dmxmeta/cleared` is a byte mask. CLEAR retains the blob and sets its mask bit.
Save/copy reactivate a cleared slot. Settings namespaces are untouched; no erase
or migration is performed.

Exceptional error handling is deliberately explicit: corrupt slots are not
reported as ordinary empty slots during operations, COPY refuses a corrupt
target rather than silently overwriting it, and failed metadata writes cannot
report success or change the RAM mask. Valid preset behavior is unchanged.
Blob write and cleared-mask update remain separate NVS operations, not an atomic
transaction. If reactivation metadata fails after a blob write, the operation
reports failure and the slot remains logically cleared; retry is possible.
Future atomic persistence would need a separate, explicitly approved design.

## Verification and Hardware Gate

Host test commands and coverage: `tests/controller/README.md`. These tests execute
the real Controller with fake storage; they do not prove NVS hardware behavior,
display rendering, electrical DMX timing, or packet coherence.

Build all four unchanged PlatformIO environments:

```sh
pio run -e cores3se_unit
pio run -e cores3se_base
pio run -e cores3se_unit_diagnostics
pio run -e cores3se_base_diagnostics
```

Stage 0 diagnostic capture/service and instrumentation remain intact, including
their `[S0]` format and baseline build label. Identify Stage 1 trials separately
in capture filenames and retain the printed build date/time. Compare new heap,
DMA block, canvas pointer/reuse, and page-transition readings with Stage 0 rather
than assuming the historical measurements still apply.

Before committing, run on physical hardware:

- Unit + Battery Bottom powers Unit DMX; bare USB cold-start and Base external-PSU behavior stay unchanged.
- Grid scrolling/selection at channels 1, 21, and 512; Main slider and selected highlight.
- Precision taps/holds at STEP 1/5/10/25, clamping, session STEP retention, and slider.
- Keypad accept/cancel/clear, 0 and 255, selected-channel/value agreement on return.
- Existing P1-P8 load unchanged after normal upload, save/reboot/load, copy to empty, occupied refusal, clear/reboot, save/copy reactivation.
- Empty P4/P6 stay empty without repeated NOT_FOUND logs; do not damage user NVS to test corrupt cases.
- Blackout double tap: zero output, edit/load while blacked out, save working values, release restores original entry universe, repeat cycle.
- Settings/Presets/Precision/Keypad navigation, single shared canvas, no flicker/ownership regression.
- Sleep/wake, continuous output, battery display/runtime/flashing, 20%/10% skipped-threshold warnings with UI MUTE, then UI volume restoration.
- Restart/shutdown zero sequence, normal speaker/UI tones, receiver mode if used.
- Repeat diagnostics transitions/preset operations, check memory stability and retained NVS settings, both profiles.
- External isolated DMX analyzer/oscilloscope: cadence, break/MAB/slot timing, complete start-code-zero 512-channel packets, and packet coherence under edits, preset loads, blackout, page changes, and sleep/wake. Software logs cannot establish wire-frame integrity.

The experimental Base's previously observed random glitches and transport
coherence remain separate investigation gates. No claim of fixing them is made.

## Rollback

Do not reset, stash, overwrite historical work, or alter tags. Before hardware
validation, the current branch is intentionally uncommitted. For a known stable
comparison use the existing v0.2.0 release firmware or a separate checkout of
the stable commit. The ignored Stage 0 copies support local source comparison;
do not overwrite Stage 1 files without an explicit rollback request. No release
artifacts or history are changed by this stage.
