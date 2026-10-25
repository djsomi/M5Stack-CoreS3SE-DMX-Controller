# Stage 5: SoftAP and embedded read-only HTTP

## Final Stage 5 acceptance (2026-10-25)

This section is the final implementation summary and supersedes the initial
software-only report below. Stage 5/5A/5B/5C production work is finalized on
`codex/stage5-softap-http`, directly above Stage 4B commit
`f31411a38d56934c5d698d52371cf45235491bc4`. No Stage 6 implementation is included.

The user confirmed these physical CoreS3SE + Unit DMX results:

| Hardware acceptance | Result / scope |
| --- | --- |
| SoftAP and embedded HTTP page | PASS |
| iPhone cellular internet while associated | PASS in the tested setup |
| DHCP default-router suppression | Confirmed working in the tested setup |
| Simplified factory Wi-Fi credentials | PASS |
| Simultaneous Wi-Fi stations | **Three tested successfully** |
| Settings association counter | Correct updates confirmed |
| Existing DMX, display and Controller functions | Operational, user confirmed |
| Stage 5B fullscreen allocation | PSRAM verified; about 153.6 KB internal recovery |
| Wi-Fi / HTTP service state | READY confirmed |

**Four associated stations are configured; only three have been physically
validated.** No claim is made for a fourth station, every iOS version or every
lease/routing configuration. These are user-performed hardware results, not
new uploads or measurements by the agent. The earlier Stage 5B log records
175,936 internal bytes at READY; it is not a new final-build measurement.

### Final production configuration

- The installed Arduino ESP32 2.0.16 / IDF 4.4.7 Wi-Fi/netif and HTTP server
  remain on their own bounded worker/server tasks. No new dependency, STA,
  NAT, captive portal, filesystem, OTA or WebSocket control is added.
- Native `esp_netif_dhcps_option` disables router offering before AP start,
  with checked stop/SET/start/GET/readback. Local address remains 192.168.4.1/24.
- New factory SSID is `CoreS3SE-DMX-XXXX`, password `espdmx-xxxx`, using the
  actual SoftAP MAC's final four hex characters. Actual AP MAC is verified
  after driver initialization. Credentials persist in dedicated Wi-Fi NVS.
- Legacy version-1 credentials are retained unchanged because factory/custom
  origin cannot be proven. Version 2 carries origin metadata and supports
  future 8..63-character user passphrases. The local held Wi-Fi-only reset
  writes an intent marker; the existing Restart action applies it next boot.
  No preset/settings NVS format changes or automatic credential resets occur.
- Four station associations are the default, separate from three accepted HTTP
  sessions. Event-driven counts are reconciled at 1 Hz; callbacks never render,
  write presets or access the Controller. Settings redraw follows visible
  phase/count/reset revisions, with the existing speaker deferral.
- One existing 320 x 240 RGB565 fullscreen sprite is explicitly placed in PSRAM.
  M5GFX's native non-direct-DMA path is retained. Internal fallback is rejected;
  allocation failure cannot become a redraw allocation loop. The Receiver's
  existing console buffer remains internal and reserved before radio startup.
- The tested Stage 3E RS485_CLASH correction, immutable TX snapshots, qualified
  513-slot completion, blackout ordering, Controller/Dispatcher ownership,
  publication/scheduling, presets, battery/speaker and sleep/wake behavior remain
  unchanged apart from approved network Settings/allocation integration.
- All resource/stack/transport guards remain enforced. `/status` retains
  free/minimum/largest internal, largest DMA, PSRAM, task watermarks, owner-loop
  gap and transport counters, plus station limit and DHCP option status. It
  contains no password and does not expose a Controller mutation endpoint.

### Startup tracing and privacy

Final standard builds set **DMX_STAGE5A_TRACE=0**, including all four normal/
Stage 0 environments. The verbose Stage 5A event buffer, formatting code and
USB FIFO output are compiled out. The small handwritten guarded source and
its tests remain available for an explicit normal-Unit investigation build
with `DMX_STAGE5A_TRACE=1`; no HWCDC initialization is introduced. This is an
intentional opt-in tool, not an active production diagnostic stream.

`/status` is independent of that flag and remains enabled. Disabling startup
tracing is the only final firmware change after the user's functional hardware
acceptance; it is checked in software and rebuilt, with no additional upload.
The final trace-disabled binaries have not separately been flashed by the agent.

Only factory derivation logic and synthetic examples/test values belong in
source. Deployed credentials remain device-local, displayed in Settings.
Private logs, historical development, flash backups, migration bundles,
`.pio`, generated host headers/executables and Python caches are excluded from
the commit. Existing files remain on disk; ignore rules cover logs and caches.
The existing committed gzip assets remain unchanged and included through the
original Stage 4B pipeline; no regenerated release/FULL bundle is needed.

### Security and remaining gates

A MAC-derived factory password is predictable from the public SSID. WPA2 still
protects association/encryption, but **association alone is not sufficient
authorization for future remote DMX control**. Stage 6 requires stronger
provisioning/authentication, input validation, bounded queues and the existing
single-owner command path. Current HTTP has no TLS or per-user authentication;
any associated client can read its limited diagnostics. Local reset requires
physical interaction and deliberately replaces Wi-Fi credentials only.

Four-station hardware validation, extended stress/leak testing, exhaustive
lease renewals/OS combinations and direct DHCP packet evidence beyond the user's
confirmed setup remain separate checks. External RS-485 capture is still needed
for wire-level packet coherence, break/MAB/cadence and electrical conclusions;
software tests and fixture operation do not establish them. Base remains
experimental. Existing upper-flash, OTA rollback and shutdown-zero-frame
limitations are not resolved by Stage 5.

### Final software validation

Finalization validation PASS: 12 freshly compiled host cases covering
Controller, Dispatcher, actual ISR/transport, credential/station/DHCP-contract,
canvas/guard, startup history and trace defaults; separately compiled HTTP and
asset-API tests; all integration/source guards, fresh/repeated/invalid patch
states, gzip asset verification and six infrastructure/layout tests. Working
and staged whitespace checks are required before committing. ELF/symbol and
image inspection confirm **no startup trace code/output strings in any of the
four standard builds**. Private logs/caches remain local and excluded.

| Final environment | Result | Static RAM bytes | Flash bytes | App BIN bytes |
| --- | --- | ---: | ---: | ---: |
| cores3se_unit | PASS | 59,844 | 1,024,501 | 1,024,912 |
| cores3se_base | PASS | 59,844 | 1,024,473 | 1,024,896 |
| cores3se_unit_diagnostics | PASS | 62,460 | 1,026,697 | 1,027,120 |
| cores3se_base_diagnostics | PASS | 62,460 | 1,026,665 | 1,027,088 |

Normal Unit static RAM is 3,880 bytes lower than the trace-enabled accepted
Stage 5C build. This is a build-time delta, not a new runtime heap measurement.
All receipts match source SHA-256
`e32dd60743553f3e9ccb5eb760c41927d2b567427b7a79aa6fe9762818c23daa`.
Normal Unit application BIN SHA-256:
`380150c8d561330b70cef461de3861dd68865e098cce731422185f7f7e447c19`.
The 16 MiB partition image and baseline gzip bundle hashes remain unchanged.
Final local logs/receipts are under ignored
`dist/stage5-finalization-20261025/`; they are not committed release assets.

Detailed implementation history is retained in STAGE5A_SOFTAP_MEMORY_DIAGNOSIS,
STAGE5B_PSRAM_FRAMEBUFFER and STAGE5C_SOFTAP_USABILITY. Their original pending/
uncommitted statements describe earlier checkpoints and are superseded here.

## Historical initial Stage 5 report (superseded above)

## Validation status and scope

Software validation completed on 2026-10-25. Changes remain **uncommitted** on
`codex/stage5-softap-http`, created directly from the hardware-tested Stage 4B
commit `f31411a38d56934c5d698d52371cf45235491bc4`. No merge or cherry-pick of
the separate Stage 3E sibling was performed. The production Stage 3E correction
already present in Stage 4B remains unchanged.

All four 16 MiB builds and the host checks pass. **This firmware has not been
uploaded. AP connectivity, browser rendering, runtime Wi-Fi memory consumption,
credential persistence on the device and DMX operation under network load remain
hardware acceptance items.** Host tests do not establish electrical correctness.

Only Wi-Fi SoftAP, static HTTP and read-only network/resource status are added.
There is no WebSocket, remote DMX mutation, OTA handler, filesystem, captive
portal, STA connection, cloud service, preset backup/restore or program engine.
The existing Stage 4B placeholder web assets are served unchanged.

## Exact files

Modified:

- `src/src.cpp`: credential preparation before M5/ADC initialization; UI buffer
  reservation; asynchronous network launch; owner-loop health telemetry.
- `src/view_sender.h`: reserve/retain existing drawing buffers before Wi-Fi;
  extend Settings scrolling for local network information.
- `src/view_receiver.h`: reserve/retain its existing console sprite before Wi-Fi.
- `tests/dispatcher/check_integration.py`: allow only the marked Settings
  information addition in existing protected-section comparisons.

Added:

- `include/network_service.h`: owner-facing service lifecycle and copied status.
- `src/network_service.cpp`: credentials, AP lifecycle, HTTP service and metrics.
- `src/http_policy.h`, `src/http_policy.cpp`: host-testable HTTP and budget policy.
- `tests/network/test_http.cpp`: actual policy/asset/budget host tests.
- `tests/network/check_integration.py`: exact baseline reconstruction and
  network ownership/startup/session source guards.
- `tests/network/probe_device.py`: explicitly invoked, read-only device probe.
- `tests/network/README.md`: host and device probe instructions.
- `docs/STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md`: this report.

`platformio.ini`, partition CSV, asset sources/generated bundle, packaging,
Controller, Dispatcher, preset storage, diagnostics implementation, transport
and reproducible driver patch are unchanged. Historical work, old logs, backups,
migration bundles and stashes are preserved. New local validation artifacts are
under the ignored `dist/stage5-validation-20261025/` directory.

## Architecture, ownership and bounded work

Arduino ESP32 2.0.16 / ESP-IDF 4.4.7 already supply `esp_wifi`, `esp_netif`,
`esp_http_server`, lwIP and Preferences. No new third-party dependency is needed.
A synchronous Arduino `WebServer::handleClient()` in the owner loop would couple
socket service to DMX/UI progress. An external async HTTP library adds an
unnecessary dependency here. The bundled IDF HTTP server provides a separate
task and socket I/O overrides appropriate for this small read-only service.

The application task remains the sole Controller/Dispatcher/transport owner:

1. `network_service::prepare()` performs credential NVS I/O once, before M5,
   ADC, radio and DMX initialization. No runtime network handler writes NVS.
2. Existing Sender fullscreen/cell sprites and the existing Receiver console
   sprite are reserved before radio allocation. No additional fullscreen canvas
   is created and no active fullscreen page is selected by reservation.
3. `launch(canvas_reserved)` captures pre-radio resources and creates a core-0,
   priority-1 worker with an 8192-byte stack. The owner does not wait for AP start.
4. The worker alone initializes/stops the radio, netif and HTTP server. DHCP,
   station polling and teardown never run in the owner task.
5. HTTP runs on core 0 at priority 3 with an 8192-byte stack. It reads immutable
   flash assets and short copied network/resource status only. No Controller,
   Dispatcher, transport or M5 display access exists in handlers.
6. `ownerTick()` copies owner-loop gap and existing transport health counters
   under a short portMUX. It performs no allocation, socket work, NVS I/O or wait.

The existing driver publication, UART interrupts, immutable snapshots, ordered
blackout barriers, completion qualification and sender/receiver separation are
unchanged. Network task scheduling and radio interrupts can still affect CPU
availability; preservation of source alone does not prove unchanged wire timing.

### Request/session limits

| Resource | Implemented limit |
| --- | --- |
| Associated stations | 2 |
| Accepted HTTP sessions | 3; listen backlog 2 |
| SDK request header / URI limits | 1024 / 512 bytes in the installed SDK |
| Application Host / Origin values | 64 bytes each |
| Accept-Encoding value | 256 bytes |
| Socket receive / send budget | 2048 / 20480 bytes per accepted session |
| Each native socket I/O operation | at most 256 bytes |
| Receive / send wait | at most 1 second, capped by remaining session lifetime |
| Session lifetime | 3 seconds from acceptance; incoming progress never renews it |
| Requests | one application request per connection; Connection: close |
| Rate | 12 handled requests/second globally after Host/body validation |
| Individual flash asset | 16 KiB maximum; larger assets fail with 503 |

The worker queues at most one pending idle sweep, normally once per second.
The HTTP task expires accepted sockets even if they have sent no data. IDF
4.4.7's queued close skips a session whose LRU counter is zero; the sweep updates
the live session's LRU counter in the HTTP task before requesting its close.
Successful handlers use supported queued session close rather than returning a
false handler failure. SDK parser errors and unsupported methods use native
error responses under the same socket budgets.

These limits bound storage, bytes and intentional socket waits. They are not a
hard real-time wall-clock guarantee: HTTP task scheduling, the control queue,
listen backlog and OS/radio work add latency. One slow client can delay another
web client. It cannot cause the owner task to wait on its socket. Hardware stress
testing must verify effective idle expiry and owner/DMX service under load.

Relevant exact upstream implementation references:
[IDF 4.4.7 session/close implementation](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/esp_http_server/src/httpd_sess.c),
[URI handler dispatch](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/esp_http_server/src/httpd_uri.c),
[default Wi-Fi netif lifecycle](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/esp_wifi/src/wifi_default.c).
Installed SDK headers were used for API compatibility, socket/task configuration
and stack-watermark units. No newer-IDF behavior is assumed.

## Provisioning, discovery and recovery

- SSID: `CoreS3SE-DMX-` followed by all six AP MAC bytes in uppercase hex.
- Password: 24 independently generated base-32 characters, alphabet
  `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`; no shared production key.
- First boot uses the installed SDK bootloader entropy source around
  `esp_fill_random()` before ADC/radio initialization, then disables that source.
- One versioned credentials blob is stored in Preferences namespace `dmxwifi`,
  key `ap1`. Preset/settings namespaces and formats are not touched.
- Existing blobs are checked for size, version, terminators, expected device
  SSID and password alphabet. A malformed blob or failed read/write disables
  networking; it is not silently erased or regenerated.
- The worker uses RAM-only Wi-Fi configuration with SDK Wi-Fi NVS disabled.
  Complete WPA2-Personal/CCMP configuration is installed **before radio start**;
  there is no initial open AP interval. The installed AP config lacks the newer
  PMF configuration field, so no unsupported PMF claim is made.
- Channel 6, AP-only, maximum two stations. There is no router or internet setup.
- The configured IP is explicitly set to `192.168.4.1/24`, gateway itself.
  After AP-up, DHCP server status, actual IP and WPA2 auth mode are read back.
  Settings receives that actual IP only after successful verification.

In Sender Settings, scroll down to see phase/error, SSID, key, actual HTTP
address and current internal/DMA memory. Existing right-side controls stay in
place. Reopen/scroll Settings to refresh its readout; there is no new periodic
fullscreen redraw. Forgotten-key recovery means physically reading the same
persistent key here. There is no reset/rekey action in this stage. Corrupt-NVS
recovery requires a separately reviewed targeted procedure; **do not erase NVS
or presets to recover a forgotten password**.

Credentials are never intentionally printed to runtime logs, included in HTTP
responses or checked into the repository. The Settings key is deliberately
visible to someone with physical access to the screen.

## HTTP routes and asset behavior

- `GET /`: unchanged embedded HTML, `text/html; charset=utf-8`, `no-store`.
- Hashed CSS/JS: exact paths/MIME from the existing generated manifest;
  `public, max-age=31536000, immutable`.
- All three asset bodies: original flash-resident gzip bytes,
  `Content-Encoding: gzip`, `Vary: Accept-Encoding`, exact compressed
  `Content-Length`. No inflation, filesystem or second packaging pipeline.
- Encoding parser handles case, q-values, wildcard and explicit gzip exclusion.
  Unsupported, missing or empty Accept-Encoding yields **406**, intentionally:
  this implementation offers gzip only, not an identity representation.
- Unknown paths yield 404; oversized encoding headers 431; bodies are rejected
  with 413; rate excess 429; unavailable/oversized assets 503.
- `GET /status`: bounded plain JSON, no-store. No channel/preset data or key.

Requests require Host `192.168.4.1` or `192.168.4.1:80`. If Origin is supplied,
it must match `http://192.168.4.1` or the explicit `:80` form. No CORS is enabled.
Application responses include nosniff, no-referrer and a same-origin CSP with
frame embedding disabled. The index is never reused through long-lived cache.

Security limitations: HTTP has no TLS or separate user/session authentication.
Any device admitted by the WPA2 key can read assets/resource metadata. AP-only
mode supplies no configured uplink, but this is not a claim of resistance to an
authorized station, RF interference, resource exhaustion or physical extraction.
NVS credentials are not encrypted by this change. Rate/session limits reduce
resource use; they do not constitute a complete network DoS defense.

## UI allocation and runtime instrumentation

Existing fullscreen canvas and two Sender cell sprites are allocated before
Wi-Fi, and retained across mode changes. The existing Receiver console sprite
is also reserved (320 x 88 pixels at 8-bit depth, approximately 28 KiB plus
allocator overhead) so Receiver entry does not depend on post-radio heap.
The normal sprite allocator and dimensions are preserved. Same-size
`createSprite()` in installed M5GFX reuses the allocation. The single shared
fullscreen page ownership and draw/push routines are unchanged.

`/status` reports:

- internal 8-bit free heap and lifetime minimum;
- largest internal 8-bit block and largest internal DMA-capable block;
- free PSRAM, timestamp and station/request counts;
- worker and HTTP task stack high-water marks **in bytes** on this IDF;
- maximum interval between owner-loop health samples;
- existing transport error counter and fail-stopped state.

Fixed samples 90/91/92 represent before worker/radio, after AP/DHCP (before
HTTP), and latest sample. A separate eight-entry ring records before-start,
after-AP, HTTP-ready, station connect/disconnect, request activity and failure.
Current values update about once per second without continuous logging. The
pre/AP samples remain available when the event ring wraps. Request/station
events can coalesce within a sampling interval. Brief largest-block/PSRAM peaks
can be missed; minimum internal heap is a lifetime low-water measurement.

Provisional, **unmeasured** admission guardrails:

- Before worker creation: internal free >= 80 KiB, largest internal 8-bit block
  >= 48 KiB, largest DMA block >= 32 KiB; all UI reservations must succeed.
- During startup/running: internal free >= 32 KiB and largest DMA >= 16 KiB.
- Stack high-water marks must remain >= 1024 bytes once measured.
- A transport error or fail-stop disables networking conservatively; it does
  not prove Wi-Fi caused that error and never resets the driver.

Failure stops HTTP/radio from the worker and exposes FAILED/error in Settings.
There is one initialization attempt per boot, no automatic retry loop. SDK
global event/TCP-IP infrastructure can remain allocated after failure; shutdown
does not promise complete recovery to pre-radio heap. If these guards reject
the actual hardware or stress exposes DMX instability, record the blocker and
stop. Do not lower the guards to conceal it.

No Serial/HWCDC initialization or logging path was added. Existing diagnostics
and their legacy build identifiers are untouched. Their old after-init canvas
assumptions should not substitute for Stage 5's explicit pre-radio capture;
canvas identity can still be checked on first fullscreen page entry.

## Build and host results

| Environment | Result | Static RAM | Flash use | Application BIN bytes |
| --- | --- | ---: | ---: | ---: |
| cores3se_unit | PASS | 59,724 | 1,021,401 | 1,021,824 |
| cores3se_base | PASS | 59,724 | 1,021,369 | 1,021,792 |
| cores3se_unit_diagnostics | PASS | 62,348 | 1,023,641 | 1,024,064 |
| cores3se_base_diagnostics | PASS | 62,348 | 1,023,605 | 1,024,016 |

Compared with Stage 4B: static RAM increases 23,960 bytes for normal builds and
23,968 for diagnostics. Flash increases 413,420 bytes for both normal builds,
413,416 for Unit diagnostics and 413,396 for Base diagnostics. These compiler
figures exclude dynamically allocated task stacks, reserved UI buffers, Wi-Fi,
lwIP, HTTP parser/session storage and runtime fragmentation. **No post-Wi-Fi
runtime heap figures have been measured yet.**

All four receipts have source SHA-256
`739d16d7116378e455ff076add79a5727c115e973c5aa889d24408489a844a23`.
The image header remains 16 MiB / DIO / 80 MHz (size/frequency byte `0x4f`).
The original two 6 MiB slots and all partition offsets remain unchanged.
Partition image SHA-256 remains
`6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`.
The embedded gzip bundle SHA-256 remains
`5ae4ba7f7ea51bcd054f92bbd3e549efccddf41008a28e8840a8c6235a44c7e9`.
HTML/CSS/JS compressed sizes remain 338/196/140 bytes (674 total).

Validation completed:

- Controller and Dispatcher host suites and existing integration guards: PASS.
- Stage 3 actual ISR/API/adapter/transport tests and fault injection: PASS,
  including 385 underrun tails, incomplete/premature completion, timeout,
  retirement, Stage 3E clash/no-receiver, receiver separation and blackout order.
- Strict reproducible patch fresh/already/mixed/unknown-source state checks: PASS.
- Six infrastructure checks and all four build artifact/layout checks: PASS.
- New production HTTP-policy/real-generated-asset tests: PASS. Covers metadata,
  cache/encoding/Host/Origin, fragmentation, byte/overflow limits, drip-feed
  absolute deadlines, idle deadlines and bounded parallel session simulations.
- New source guards reconstruct the entire Stage 4B application/Sender/Receiver
  after removing only approved additions; protected driver/domain/USB/layout/
  asset sources remain equal to the baseline: PASS.
- Final `git diff --check`: PASS.

Host tests do not execute the real Wi-Fi/NVS/socket/HTTP parser/scheduler. Their
deadline simulation is not a hardware socket-expiry result. The manual probe
has been syntax/help checked only and has not contacted a device.

Logs, build receipts, resource JSON and host executables are retained under
`dist/stage5-validation-20261025/`; the latest builds are in
`verified-builds.log`. Earlier failed SDK-compatibility compile logs are retained
as history, not presented as successful validation. No release FULL binary or
migration bundle was produced or replaced.

## Hardware upload and acceptance

These are instructions only; no upload was performed. The device must already
have the verified Stage 4B 16 MiB migration. Use the **normal Unit** environment
first; no separate Serial/HWCDC initialization or diagnostic workaround:

```powershell
Set-Location -LiteralPath 'C:\Users\somog\Downloads\MyCodes\esp dmx controller'
pio run -e cores3se_unit -t upload --upload-port COM8
```

This uses the existing PlatformIO uploader/layout, not a new FULL image or an
erase-flash operation. Do not flash an old 8 MiB image over this validation.

1. Boot Unit + Battery Bottom using the known working power arrangement.
   Verify UI, live DMX, Precision Editor, keypad, presets, blackout, speaker,
   warnings and sleep/wake before browser traffic. Check no-cable boot and
   subsequent XLR attachment preserve Stage 3E behavior.
2. Open Sender Settings and scroll down. Record phase/error and memory.
   Require READY. Read the unique SSID/key locally. If FAILED, preserve the
   error and resource values and stop rather than repeatedly rebooting or
   erasing NVS. Reopen Settings if its initial readout was STARTING.
3. Join the SSID from a phone or Windows computer. Verify an incorrect key is
   rejected. Accept the phone's no-internet warning and remain on this AP.
   Use the **actual address shown in Settings** (configured/read back as
   `http://192.168.4.1/`). No captive-portal redirection is expected.
4. Open the address in a browser. Confirm the unchanged placeholder page loads,
   all three resources return 200 with correct MIME, gzip and compressed lengths,
   index no-store, hashed CSS/JS immutable. Inspect DevTools/Network and confirm
   no missing resource, console error, external fetch or DMX command.
5. Inspect `/status`. Save the full JSON for each condition below. Fixed sample
   90 is pre-Wi-Fi; 91 is post-AP/DHCP; 92 is current. Require transport errors=0
   and tx_stopped=false, adequate stack reserves and no progressive heap loss.
6. Run the read-only probe with other refreshing browsers temporarily idle:

   ```powershell
   python -B tests/network/probe_device.py --host 192.168.4.1 --output dist/NEW-stage5-http-probe.jsonl
   ```

   Use a new output filename; the probe refuses overwrite. It checks all real
   gzip hashes/headers, 404, identity-406, untrusted-Host-403, status and idle
   socket expiry. It does not upload, reset or log the key.
7. Repeat refreshes for at least 60 seconds with one client, then two clients;
   inspect actual owner gaps, heap/DMA/PSRAM and task watermarks. Exceed 12 handled
   requests/second to verify bounded 429 behavior and recovery when traffic stops.
   Verify a third Wi-Fi station cannot associate. Reconnect after all clients leave.
8. Exercise malformed/oversized headers/URI, GET with body, unsupported methods,
   missing/identity/gzip;q=0 encoding, unknown paths, a slow reader, partial
   drip-fed request, idle accepted connection and mid-response disconnect. Check
   sessions retire and ordinary HTTP resumes. A 3-second accepted deadline plus
   sweep/scheduling delay is expected; not an exact close at 3.000 seconds.
9. While refreshing, exercise preset save/load/copy/clear, blackout double taps,
   rapid slider/Precision changes, Settings/Keypad/Presets page navigation,
   Sender/Receiver/Sender switching and sleep/wake. Check canvas pointer reuse
   with the existing diagnostic build only after normal-build acceptance.
10. Disconnect/reconnect XLR while serving traffic; verify no transport latch or
    reset. Cold boot again and confirm the same SSID/key, DHCP, HTTP and touch/DMX
    behavior. Repeat connected-receiver and no-receiver startup.

Runtime results to fill on hardware (none measured yet):

| Condition | Internal free/min | Largest internal/DMA | Free PSRAM | Stack HWM / owner gap / TX errors |
| --- | --- | --- | --- | --- |
| Before radio (sample 90) | pending | pending | pending | pending |
| After AP (91), then HTTP ready | pending | pending | pending | pending |
| One browser | pending | pending | pending | pending |
| Two clients | pending | pending | pending | pending |
| Sustained refresh / slow clients | pending | pending | pending | pending |
| All clients disconnected | pending | pending | pending | pending |
| Reconnect, mode switches, sleep/wake | pending | pending | pending | pending |

An external RS-485 capture/logic analyzer is still required to compare 513-slot
packet coherence/ordering, break/MAB, baud, gaps/cadence and signal quality under
network load. Owner-gap/TX health logs do not establish wire-frame integrity.
Base remains experimental; its unrelated electrical/output glitch investigation
is not resolved by these builds.

## Remaining gates, Stage 6 and rollback

Hardware must establish real Wi-Fi resource headroom, fragmentation recovery,
idle/socket bounds, cold-boot credentials, browser rendering and uninterrupted
touch/DMX/preset/blackout operation. Wi-Fi and lwIP create SDK tasks/buffers beyond
the two explicitly configured task stacks. Software assertions do not bound all
SDK CPU execution or demonstrate that provisional reserve thresholds are enough.
The Settings extension and retained Receiver buffer also require visual/mode-
switch acceptance. Stage 4B limitations (upper-flash writes, OTA rollback,
shutdown-zero frames and external electrical measurements) remain unproven.

Stage 6 must define authentication/Origin/session policy for commands; bounded
WebSocket RX/TX queues, sizes, client counts and slow-client backpressure;
initial snapshots, revisions, event sequencing, validation and reconnect rules.
Only the application owner may consume commands through CommandDispatcher.
HTTP/WS tasks must receive copied immutable status, not read/mutate live
Controller storage. Existing blackout ON/OFF barriers cannot be silently
coalesced with latest-state channel updates. Budget the additional RAM/CPU and
repeat the external coherence gate. Offline JSON Preset Backup & Restore stays
planned for a separate stage with schema/validation and no preset NVS migration.

No Stage 6 work begins here. If hardware acceptance fails, retain these changes
and logs, and use a separate clean worktree at the tested Stage 4B commit to
build/upload its normal Unit firmware using the same 16 MiB layout. Do not
reset/stash/discard this work, erase presets or overwrite the tested migration
bundle. No commit, push, merge, tag, release or automatic upload was performed.
