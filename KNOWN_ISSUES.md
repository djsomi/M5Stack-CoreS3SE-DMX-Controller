# Known issues and operating limits — v0.3.0

Applies to frozen firmware tag `v0.3.0`, commit
`59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`. These are limitations, not claims
of fixes made during publication preparation. The [evidence matrix](docs/V0_3_0_DOCUMENTATION_COVERAGE.md)
distinguishes source checks, automated tests and user-reported physical acceptance.

## Intel Wi-Fi 6 AX200 on Windows

**Client-side compatibility issue; not a confirmed firmware defect.** In the
reported Windows environment with **Intel AX200 driver 24.20.2.1**, significant
packet loss and WebSocket instability occurred when connected to the SoftAP.
Earlier development records measured 26–30% ICMP loss; clean macOS tests succeeded
when the unstable Windows client was disconnected. These observations do not
prove every Windows/AX200 combination is affected.

The user verified this workaround in the affected environment:

**Device Manager > Intel Wi-Fi 6 AX200 > Advanced > MIMO Power Save Mode > No SMPS**

This changes a client adapter setting, not the controller firmware. Availability
and wording may differ between driver versions. The exact driver/workaround is
user-supplied physical evidence recorded during this documentation audit; it was
not independently retested by the documentation reviewer. See the
[Stage 6A acceptance record](docs/STAGE6A_WEBSOCKET_IMPLEMENTATION.md) and coverage K03.

## Keep Backup operations in the foreground

**Keep this browser tab open and active until the operation completes.** This
warning applies to Import confirmation and both Import/Export progress dialogs.
Do not switch tabs, background the iOS Home Screen app, navigate away or close it
while transferring presets. Wait for the backend result, then verify the result
or downloaded file.

The frontend deliberately suspends its WebSocket and cancels transfers on page
visibility/lifecycle changes. This is demonstrable without advancing polling
timers; browser timer throttling may add delays but is not the sole explanation.
Returning to the foreground reconnects and resynchronizes; it does **not** resume
or replay mutations. If connection loss occurs after the atomic import selection,
the import may already have committed. Inspect/export the current collection
before deciding to retry. Do not assume a missing success message means no write.
See [Backup](docs/PRESET_EXPORT_IMPORT.md) and [Export analysis](docs/PRESET_EXPORT_RESPONSIVENESS.md).

## Wi-Fi security and control access

- **Change the predictable factory Wi-Fi password before use in untrusted
  environments.** The default `espdmx-xxxx` uses the AP MAC suffix; it is not a
  randomly generated secret. Configure a strong, unique password through web
  Settings and physical approval. Do not publish the device's actual credentials.
- WPA2 protects association; it is not separate application authorization.
  Associated clients share DMX, preset, settings and maintenance access. HTTP and
  WebSocket use plaintext at the application layer; there is no HTTPS/WSS,
  per-user login, role separation or separately authenticated administrative API.
- Physical approval protects Wi-Fi credential changes. Browser confirmation and
  the maintenance CANCEL countdown express intent/cancellation, not identity.
  Do not expose this interface through port forwarding, a LAN bridge or the internet.
- This release is SoftAP-only: no STA/AP+STA, captive portal, NAT/router service
  or OTA updater. DHCP default-router suppression helped iPhone cellular routing
  in the tested setup; internet routing is client-dependent, not guaranteed.

## Hardware acceptance: Unit recommended, Base experimental

CoreS3SE + **Unit DMX U183 + Battery Bottom** is the recommended hardware-tested
setup. Bare USB can power the CoreS3SE with external bus output disabled; it does
not supply the Unit in the accepted guarded startup configuration. Base requires
its external PSU. There is no automatic accessory detection or wrong-profile
flashing interlock. Verify the ZIP manifest and `UNIT DMX` / `DMX BASE` boot label.

The tested **DMX Base remains experimental**: random fixture glitches were
observed in controlled Unit/Base comparisons, including static/simplified tests.
The sender RS485_CLASH correction is not evidence that those Base glitches are
fixed. Do not generalize the observed Base behavior to every Base unit. Unit
acceptance does not certify Base migration or final Base electrical performance.
See [hardware and installation](docs/V0_3_0_INSTALLATION.md).

## Physical touchscreen slow movement

Slow continuous touchscreen fader movement may exhibit intermittent coordinate stalls originating at or before the touch-controller acquisition layer. Historical upstream code contains the same relevant acquisition mechanisms. No application-level cause was demonstrated. The behavior is not proven to have occurred on the original firmware.

The captured raw-coordinate fader correction is retained. It removes the proven
processed-coordinate capture dead zone without smoothing or changing geometry;
it does not promise to resolve upstream coordinate stalls. THDIFF was observed
changing from `0xA0` to `0xC8` with touch activity; cause and relation to stutter
remain unknown. THDIFF write experiments are suspended. Production and standard
diagnostics exclude investigation implementations; no automatic cleanup write is
performed. See [stabilization isolation](docs/V0_3_0_STABILIZATION_PHASE1.md).

## Accessibility and mobile browser behavior

The accepted UI uses a **fixed-scale viewport** (`maximum-scale=1`,
`user-scalable=no`), plus restricted touch actions on application/fader surfaces.
Browser pinch zoom is therefore limited; the release must not be described as
preserving unrestricted zoom accessibility. Non-editable UI text selection is
suppressed; inputs and designated selectable diagnostic content retain their
exceptions. Keyboard focus and accessible state labels have regression coverage,
but that is not a complete screen-reader, low-vision or WCAG conformance audit.
The stable `100svh`/internal-scroller layout was accepted on the tested iPhone and
Home Screen app; not every iOS/device/browser combination has been tested.

## Network backpressure and response time

A native synchronous WebSocket send can block the shared HTTP task. A historical
send took about **518 ms** before ETIMEDOUT despite the nominal frame deadline;
a slow/lost client can delay others, fill its bounded outbox and be disconnected.
Bounded queue draining, diagnostics, stale-socket probes and reconnect improve
handling; they do not turn the socket framework into nonblocking delivery or
prove this limitation permanently fixed. Manual Retry remains available.

Remote fader requests are paced at about 16.7 commands/s, with final-value
acknowledgment; this is distinct from physical touch sampling. Preset flash writes
still execute synchronously on the application owner. Optimized Export captures
one coherent collection and uses larger bounded download chunks; host timing is
not proof of physical input latency or continuous electrical DMX timing.

Four Wi-Fi stations are configured; four associated stations and three active WS
sessions were reported in an accepted snapshot. Station count is not browser-tab,
HTTP-socket or WS-session count. An unexpected station was identified as an Apple
Watch; no station-counter firmware defect was demonstrated. Relatively frequent
idle HTTP work scheduling remains a possible later optimization.

## DMX, shutdown and reset verification limits

Immutable active packets, qualified 513-byte completion, safe retirement,
blackout ordering and fault injection are software-tested. Three genuine
consecutive transmission failures can deliberately fail-stop the sender rather
than reuse ambiguously owned memory. Counters/logs do not establish RS-485 wire
integrity, electrical levels, break/MAB duration, complete frame length or frame
gaps; external capture is still required, including during NVS activity.

Power Off retains the original staging/wait/M5Unified shutdown path, including
accepted USB-connected use. It does **not** establish that three completed zero
frames reached fixtures. Restart has separate qualified-frame preparation.
An apparent five-second countdown near firmware-update-triggered reboot remains
a deferred observation of unknown cause, not proof an automatic reset can be
cancelled. No reboot redesign is included.

## Persistence, migration and recovery limits

- Legacy v0.2 presets/settings remain readable. After generation adoption and
  verified legacy-key reclamation, v0.2 cannot read generation-backed edits or
  rely on old `p1`–`p8` blobs. Export JSON before any downgrade; a downgrade is
  not a data-preserving rollback promise.
- Two fixed generations are sustainable in the tested constrained 20 KiB NVS
  workload. Arbitrary unrelated data can still exhaust storage. `storage_full`
  is actionable failure, not permission to erase NVS. CRC/selector corruption
  fails closed; no silent fallback to stale legacy presets is promised.
- Host tests use the actual ESP-IDF NVS algorithm and injected flash faults.
  They are not physical power-cut qualification. SDK initialization may format
  corrupt/incompatible NVS, so sparse flash exclusion alone cannot guarantee all
  damaged storage survives boot.
- The simplified **Unit v0.2.0 → rc.1** functional migration passed. Separate Base
  migration, preboot byte-for-byte NVS retention, interrupted-upgrade recovery and
  upper-flash write/alias checks remain unverified. The final stable distribution
  has not itself been used in that historical rc.1 test.
- Two 6 MiB app slots do not provide an OTA updater or validated application-health
  rollback. The reserved storage partition is not an implemented LittleFS service.
  App-only flashing cannot migrate the old table. Old padded FULL images overwrite
  NVS; use the documented individually addressed images and verify checksums.

See [migration acceptance](docs/V0_3_0_MIGRATION_ACCEPTANCE.md) and
[installation](docs/V0_3_0_INSTALLATION.md) before writing flash. Flash/NVS backups
can contain secrets; keep them private.

## Publication and source provenance

The frozen tag is unchanged. Later notices, source-delivery tools and this audit
are publication work, not firmware behavior in that tag. Unit/Base modified-Arduino
relinking and source/notice inventory are locally validated; these tests do not
alone constitute a legal opinion or prove bit-for-bit regeneration of timestamped
images. Final publication must name a separate reviewed packaging commit, preserve
accepted binary hashes, and deliver the corresponding source/build/notice material
beside the binaries. Local validation archives and old RCs are not final releases.
See [source inventory](docs/V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md) and
[LGPL delivery](docs/V0_3_0_LGPL_DELIVERY.md).
