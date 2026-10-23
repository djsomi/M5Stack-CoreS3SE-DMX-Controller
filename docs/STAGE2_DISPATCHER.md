# Stage 2: Unified Command Dispatcher

## Baseline and Scope

The user confirmed Stage 1 hardware tests passed for channel editing, Precision
and keypad, presets, blackout restoration, Settings/navigation/canvas, battery
and speaker, with stable diagnostic memory and successful preset operations.
The existing Controller host tests and normal Unit/Base builds were rerun before
committing the exact Stage 0/1 project-file set.

Stage 1 commit: `0e43640e57519e47177b08fae9c0b86ff8b0e9d9` on
`codex/stage1-controller` (`Extract authoritative DMX controller and add hardware diagnostics`).
Stage 2 branch: `codex/stage2-command-dispatcher`, created from that commit.
Main and fetched origin/main remained at `32c0571049fa57c584167f5a4f6f07e67668430d`.
Logs, historical development files, existing stash, and ignored build/tool files
were not committed. No main merge or remote push was performed.

Stage 2 Unit hardware validation passed and the user approved closing this stage.
No Base hardware revalidation is claimed. No networking,
HTTP/WebSocket service, radio producer, LittleFS, OTA, partitions, program engine,
new task, driver changes, or automatic upload is introduced.

## Commands and API

`include/controller_types.h` defines owned, protocol-independent value types:

- SetChannel: channel 1..512, absolute value 0..255.
- AdjustChannel: channel 1..512, signed delta evaluated against committed state;
  result clamps to 0..255 using the Controller's overflow-safe arithmetic.
- SetChannels: 1..8 channel/value pairs, unique indices, validated completely
  before application. One Controller change/revision after the entire patch.
- SetBlackout: explicit ON/OFF, not a domain toggle.
- LoadPreset, SavePreset, CopyPreset, ClearPreset: slots 0..7 map to P1..P8.
- ResetSender: lifecycle initialization, restricted to system-source commands.

The command carries type, source (`touch`, reserved `external`, or `system`),
caller request ID, and a fixed payload union. There are no producer pointers,
Arduino Strings, JSON objects, or dynamic payload allocation. Source is trusted
application metadata, not authentication or a permission/security boundary.

`CommandDispatcher` public API:

| Method | Contract |
| --- | --- |
| `submit(command)` | Validate, copy into FIFO, return accepted/ticket or explicit invalid/busy/unsupported_mode. Never wait. |
| `process(budget=4)` | Application task executes up to budget commands, capped at 32 per call. Returns count. |
| `executeLocal(command)` | Validate local intent; drain at most 32 prior accepted commands, enqueue and execute this command through the same application path; return its completed result. |
| `setSenderActive(bool)` | Application-owned mode gate, refreshed before servicing and at mode transitions. |
| `setPublisher(callback, context)` | Existing packet-publication hook; not a new transport or scheduler. |
| `setCompletion(callback, context)` | Optional synchronous result sink for every accepted queued command. |
| `pending()`, `executingLocal()`, `queueStorageBytes()` | Bounded queue/read-only execution context information. |

Admission tickets are nonzero 32-bit sequence IDs (wrapping, not durable/global
identifiers). Completion receives the original copied command/request/source and
result: ticket, status, exact domain result, before/after revision, and separate
transport return code. Borrowed callback arguments must be copied if retained.
Rejected admission has ticket zero and does not generate a later completion.

Statuses distinguish accepted (not yet executed), applied, no_change, invalid,
busy, unsupported_mode, rejected domain policy (empty/occupied/same slot),
storage_error and transport_error. Accepted commands removed after a mode change
complete explicitly as unsupported_mode rather than being silently discarded.

## Bounds, Ordering and Ownership

The queue contains 32 entries, each holding a 76-byte command plus a 4-byte
ticket: 2,560 bytes of fixed queue storage. The host ABI's complete Dispatcher
size is 2,624 bytes. The compiled ESP32-S3 ELF reports 2,596 bytes for the complete
Dispatcher instance (queue plus metadata/callback pointers).
There is no dynamic queue growth, heap queue allocation, coalescing or command
replacement. Full admission returns busy; accepted entries always retain FIFO
order, including preset/blackout barriers and final slider samples.

Normal application-loop servicing processes up to four queued commands before
any page, startup, receiver, sleep or consumed-wake return. Local execution uses
the same FIFO/application function and may process up to 33 commands including
its own command. It does not wait for another task or future loop iteration.
This is bounded work by command count, not a hard wall-clock latency guarantee:
existing synchronous NVS calls still take time. A future remote ingress needs
measured persistence/rate limits before allowing command bursts.

All APIs, including submit, are application-task-only. The reentrancy busy guard
is NOT thread synchronization. Callbacks must not reenter or mutate the
Dispatcher/Controller. No concurrent producer is implemented or tested.
Before an HTTP task is introduced, ingress admission/removal, counters and mode
handling need a tested bounded synchronization/mailbox design. That future
change must preserve this validation, FIFO ordering and sole owner-task apply
path; it does not require moving domain state or rewriting Controller operations.

## UI and Publication

Every touchscreen domain mutation, including sender reset, now uses the
Dispatcher. No application-side direct Controller mutations remain. Read-only
state queries and observer registration are not command writes. Lazy preset
metadata queries retain their existing application-task cache behavior.

The old end-of-loop comparison that wrote target_value back into Controller was
removed. Absolute values now come only from explicit slider/keypad input;
physical +/- and Precision +/- submit relative commands. After completion, UI
caches read Controller values. Thus an old cached value cannot become an
unsolicited write, and consecutive relative commands use the latest committed
value. Selected channel, STEP session, draft keypad digits, scroll, gestures and
rendering state remain UI-local.

The existing Controller observer refreshes target_value synchronously. Local
handlers retain their same-turn rendering. Non-local queued changes set a redraw
flag serviced before early returns, respecting the current page and speaker
deferral; keypad drafts are not replaced. Settings needs no DMX-value redraw and
return-to-Main already redraws from authoritative values. Full-screen ownership
checks and the single canvas are unchanged.

After a changed channel/patch/blackout operation, Dispatcher calls the existing
sender publication wrapper once. Successful LOAD still publishes even if values
are identical, preserving the tested load behavior. SAVE/COPY/CLEAR do not
publish. No-op edits/blackout commands do not publish. A transport error reports
failure separately without rolling back committed state, as before.

The initial setup write, driver mode installation, send-ready checks/send calls,
hardware pins, compatibility patch, wake-touch service, and shutdown zero/wait
sequence are untouched. Publication remains distinct from wire transmission.
Nothing here establishes atomic frames on the physical DMX line; Stage 3 remains
the transport-coherence gate.

## Revisions, Blackout and Presets

Revisions remain monotonic in RAM, not persisted. Unchanged channel writes,
clamped no-op adjustments, identical patches, repeated blackout state, identical
LOAD, repeated zero reset, and an already-persisted identical SAVE do not advance
revision. SAVE checks the existing valid blob before returning no_change; it is
already persisted, not an unfinished write being acknowledged. Other persistence
success is reported only after the synchronous storage calls succeed. Read/
write/metadata errors remain explicit. The additional comparison read does not
alter blob format or namespaces.

Blackout ON snapshots entry values; all output remains zero during subsequent
edits/loads; OFF restores the exact entry snapshot and discards those interim
changes. UI double-tap protection and sounds are unchanged. Start code stays zero.

Preset namespaces, key names, 513-byte blobs, cleared mask, copy occupancy policy,
hold gestures, and missing-key query behavior are unchanged. No erase/migration
is performed. Blob and cleared-mask writes remain separate NVS operations:
failure after writing a cleared slot's blob still reports storage_error and
leaves it logically cleared; this is not a new atomic NVS transaction scheme.

## Verification and Hardware Checklist

### Completed Unit Hardware Validation

The user confirmed that all tested controls worked normally on CoreS3SE + Unit
DMX. Preset save/load succeeded. No sustained heap decline, resets, NVS log spam,
or dropped diagnostic records were observed.

| Measurement after shared canvas allocation | Observed value |
| --- | ---: |
| Internal free heap | 159372 bytes |
| Largest DMA-capable block | 131060 bytes |
| Free PSRAM | 8385947 bytes |
| Shared canvas pointer, reused across full-screen pages | 0x3fca3910 |

These are user-supplied physical Unit results. They do not establish DMX
electrical timing/frame coherence or explain the separate experimental Base
glitches. The diagnostic banner still identifies the old Stage 0 baseline;
accurate build provenance is a future change, not part of this closure.

Stage 2 verification completed before hardware testing:

| Environment | Result | Static RAM bytes | Flash bytes |
| --- | --- | ---: | ---: |
| cores3se_unit | PASS | 32932 | 604117 |
| cores3se_base | PASS | 32932 | 604081 |
| cores3se_unit_diagnostics | PASS | 35548 | 606361 |
| cores3se_base_diagnostics | PASS | 35548 | 606329 |

Both host suites passed (six Controller groups and six Dispatcher groups), along
with both source-integration guard groups and `git diff --check`. Static RAM is
2,600 bytes above Stage 1 in each variant: the Dispatcher plus the UI redraw flag/
alignment. These numbers do not replace real heap, DMA-block or stack measurements.
PlatformIO emitted a Windows locked-build-directory cleanup advisory; all four
builds nevertheless completed successfully. No user process was terminated and
no manual build-directory removal was performed.

Run Controller tests per `tests/controller/README.md`, Dispatcher tests and source
guards per `tests/dispatcher/README.md`, all four existing PlatformIO environments,
and `git diff --check`. Host tests are not display, real-NVS, electrical timing,
concurrency, or hard-latency tests.

For the requested Unit diagnostics trial, from the repository root:

```sh
pio run -e cores3se_unit_diagnostics -t upload --upload-port COM8
pio device monitor -p COM8 -b 115200
```

Do not erase flash/NVS. The diagnostic logger retains Stage 0's `[S0]` banner/
format; identify captures as Stage 2 and keep the printed build date/time.
The normal and diagnostic builds intentionally share the same command path.

Hardware regression checklist (retained for reproducibility):

- Main scrolling, channel highlight and editing at CH1/CH21/CH512; slider endpoints and final value after rapid release/re-touch.
- Precision slider, +/- taps and holds; STEP1/5 accelerate, STEP10/25 constant repeat; clamping and session STEP retention.
- Keypad accept/cancel/clear, 0 and 255, selected-value synchronization on return.
- Existing P1-P8 survive upload; save/load/copy/clear, occupied refusal, clear/reactivate and reboot persistence; no repeated empty-key errors.
- Blackout double tap, edit/load/save while ON, exact original entry restoration on OFF, repeated cycles and correct zero output.
- Settings/Presets/Precision/Keypad/Main transitions, single fullscreen canvas, no flicker, gesture leakage or stale value overwrite.
- Backlight sleep and consumed wake touch, continuous output, no accidental edit/preset activation on wake.
- Speaker feedback and deferred rendering; battery display/runtime/flashing, skipped 20%/10% warnings with UI MUTE and later volume restoration.
- Unit + Battery Bottom operation and bare USB cold start; Base + external PSU regression, retaining experimental status.
- Capture after-init/page/preset diagnostics; compare internal free/minimum heap, largest DMA block, PSRAM and canvas reuse against Stage 1. Fixed queue RAM is new; no persistent per-transition growth is expected.
- Restart/shutdown zero sequence and receiver-mode behavior where used.
- External RS-485-safe measurement of cadence, break/MAB/slots and packet coherence remains required separately. Do not infer wire-frame integrity from these tests or logs.

The user flashed and tested Stage 2. Its closure commit remains local; no push,
main merge, tag, or release is part of closure. Preserve Stage 1 and user files; do not
reset/stash/discard this work to switch firmware. Use the tested Stage 1 firmware
artifact or a separate checkout if a hardware comparison is needed.
