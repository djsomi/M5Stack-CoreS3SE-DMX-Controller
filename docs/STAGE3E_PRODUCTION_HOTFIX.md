# Stage 3E production hotfix: plain-DMX RS485 clash policy

Prepared 2026-10-25 from committed Stage 3 baseline
`e41d8ee21add2bb1fd226600e937b2d348368a05`, on isolated branch
`codex/stage3e-rs485-clash-hotfix`. The separate Stage 3D/E investigation worktree
is retained unchanged. Stage 4B integration and migration are separate steps.

## Evidence and correction

The user captured two no-XLR CoreS3SE Unit cold boots with `first_code=6`,
`cause=6`, raw IRQ `0x20000`, enabled IRQ `0x20002`, progress `128/513`, safe
retirement and three-failure latch (`err=3 seq=3 stop=1`). Stage 3 had enabled
CLASH in FEEDING/DRAINING and promoted it to a fatal packet result. This confirms
the predicted policy regression, not an ownership defect.

The user now confirms functional hardware validation of the corrected Stage 3E
investigation firmware. That evidence supports the same interrupt-policy change
here. This exact production-only build and the future 16 MiB build have not been
flashed by the agent. External RS-485 waveform measurement remains pending.

Production changes are limited to:

1. `patches/esp_dmx_v202/tx_service.h`: enable only FIFO_EMPTY during FEEDING and
   TX_DONE during DRAINING; do not enable RS485_CLASH.
2. `scripts/esp_dmx_tx_patch.py`: acknowledge an unexpected delivered CLASH inside
   WRITE without changing TX state; update the exact service/ISR fingerprints.

CLASH remains in the all-TX cleanup mask. No RS485_CONF, half-duplex mode, RTS,
GPIO, baud, break/MAB, framing wait, scheduling, retry or application change.
READ handling is unchanged. Unit's TX-derived direction circuitry does not
provide continuously enabled bus echo for this comparator. Future RDM or a
validated multi-transmitter collision policy needs a separate design.

## Ownership and completion invariants

The canonical core, contract, mode/task APIs, FIFO HAL and S3 getter patch remain
identical to the Stage 3 baseline. Buffer 1 is copied only after safe idle and
PREPARING reservation, then immutable throughout TX/retirement. Initial and ISR
FIFO fills still read only that snapshot under the existing owner/lock contract.

COMPLETE still requires full requested progress (513 slots in this application),
qualified TX_DONE, DRAINING, FIFO/shifter idle and no abnormal result. Raw DONE
before/during an incomplete refill, including the final refill, remains an
underrun. Invalid/zero progress, timeout, cancellation and failed retirement
remain errors. Safe retirement is required before reuse; a stuck fault keeps
storage reserved. Three genuine consecutive failures still latch; successful
completion resets the consecutive count. Latest values and blackout barriers
retain the existing ordering and retry semantics.

This establishes a local UART/ownership contract, not a promise of electrical
bus delivery. Physical coherence, break/MAB and gaps need external capture.
The existing shutdown helper still cannot guarantee a complete zero frame before
power action. No in-place fault reset or OTA/network feature is introduced.

## Excluded investigation code

The production worktree starts at the committed baseline, not a merge/copy of
the investigation tree. It contains no event ring, failure hook, trace APIs,
automatic USB dump, USB command handler, trace define or extra diagnostic object.
Application sources, normal/Stage 0 configurations and USB initialization are
unchanged. The existing separately selected Stage 0 diagnostics remain; their
known USB/stub issue is not repaired or conflated with Stage 3D instrumentation.

Permanent regression tests cover first-fill/repeated CLASH, unexpected delivery,
normal 513-slot frames, no modeled receiver/ACK, latest/final values, ordered
blackout ON/OFF, Receiver isolation, 385 real incomplete tails, premature/final
refill DONE, timeout, failed retirement, three-error fail-stop and successful
retry reset. Previous fatal-CLASH installations are rejected by strict patch
checks rather than accepted with stale templates. Fresh installation and repeat
application remain deterministic; no hand-edited dependency is required.

## Software validation

Controller and Dispatcher suites (six groups each), actual installed ISR/API/
adapter/Transport suite (all original groups plus CLASH/incomplete/blackout
regressions), strict patch-state tests and source integration guards PASS.
All four original 8 MiB environments PASS, including fresh dependency/patch
installation. Normal src/include/config and the canonical core/API/mode contract
match the baseline; normal Unit map contains no Stage 3D symbols.

| Environment | Static RAM | Reported flash | Raw app BIN |
|---|---:|---:|---:|
| cores3se_unit | 35764 | 606733 | 607152 |
| cores3se_base | 35764 | 606717 | 607136 |
| cores3se_unit_diagnostics | 38380 | 608945 | 609360 |
| cores3se_base_diagnostics | 38380 | 608913 | 609328 |

Normal RAM is identical to committed Stage 3 (35,764 bytes), without the temporary
2,776-byte Stage 3D/E investigation overhead. The selected Stage 0 variants retain
only their original diagnostics. Flash comparisons across dependency folder names
can differ slightly; no trace code is present. All app headers are 8 MiB and
all partition binaries retain SHA256
`1d9cca96de0fe07ad7fc0648b9878ddecd9ce565e38b589ad20fea698ed4c80c`.

Both unstaged and exact staged whitespace checks are required before the focused
commit. Validation logs are retained in ignored
`dist/stage3e-finalization-20261025/`; no firmware upload was performed.

## Hardware gates and rollback

The user's corrected 8 MiB investigation hardware test passed. Exact production
and integrated 16 MiB candidates still require their own authorized validation.
Test no-receiver cold boots, connected cold boots and reconnects, full UI/preset/
blackout/sleep behavior, USB upload/reboot and a separate Receiver matrix. Capture
RS-485 length/coherence/timing externally; keep experimental Base issues separate.
No claim of electrical completion or diagnostic USB fix is made.

Preserve backups, logs, the investigation worktree and stash. Rollback uses a
matching saved source/artifact in a separate worktree; do not reset dirty Stage
4B or restore a flat FULL image over NVS. A future 16 MiB migration needs separate
explicit permission and matching bootloader/table/OTA/app components.
