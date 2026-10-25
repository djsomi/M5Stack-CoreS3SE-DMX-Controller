# Stage 5C: Offline SoftAP usability

Final hardware acceptance (2026-10-25): the user confirmed SoftAP, embedded
HTTP, iPhone cellular internet while associated, DHCP router suppression in the
tested setup, simplified credentials, correct Settings counts and three
simultaneous Wi-Fi clients. Existing DMX, display and controller functions remain
operational. **Four-client capacity is configured, but only three concurrent
clients have been physically validated.** Finalization disables verbose startup
tracing by default; /status diagnostics and all safety guards remain. The
implementation/checklist below is the earlier record, superseded where marked
pending by this acceptance and the final Stage 5 implementation summary.

## Scope and validation boundary

Work remains uncommitted on `codex/stage5-softap-http`, based on preserved
Stage 4B commit `f31411a38d56934c5d698d52371cf45235491bc4` plus existing Stage
5/5A/5B work. No upload, erase, commit, push, merge, tag or release was performed.

The user physically validated Stage 5B: one PSRAM fullscreen canvas, READY,
phone HTTP access, about 176 KB internal free, and functional DMX transport.
The retained `logs/device-monitor-261025-125423.log` records `caps=4` and
175,936 internal bytes at startup.READY. That evidence belongs to **Stage 5B**.
Stage 5C's lease behavior, iOS cellular use, reset procedure and live station UI
remain **pending physical acceptance**; build success does not establish them.

## 1. Routerless DHCP

The existing worker still owns the bundled ESP-IDF Wi-Fi/netif/HTTP services.
No Arduino WiFi wrapper, new dependency, STA mode, NAT, IP forwarding, DNS
interception, captive portal or filesystem is introduced.

Startup now performs these checked steps:

1. Configure WPA2-Personal/CCMP as before.
2. Stop DHCP; accept ESP_OK or the explicit already-stopped result, reject other
   errors. The previously ignored unexpected stop error is now handled.
3. While stopped, set one byte to zero with the installed native API:

   ```cpp
   uint8_t offer_router = 0;
   esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET,
       ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &offer_router, sizeof(offer_router));
   ```

4. Preserve IP/gateway `192.168.4.1`, mask `255.255.255.0`, register station
   events, then start the AP. The option is set before AP-start handlers can
   distribute leases.
5. Bring up/verify DHCP using the existing sequence. Only after it is started,
   GET the same option and require a zero readback. IDF 4.4.7 rejects GET while
   the server is stopped. Any failed call or nonzero readback prevents READY.

Despite its enum name/value, this setting controls **DHCP Option 3 (router)**
through the driver's offer flag; it does not advertise a zero-valued router or
modify the AP interface address. The matching upstream
[netif implementation](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/esp_netif/lwip/esp_netif_lwip.c)
handles SET/GET, and the
[DHCP server](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/lwip/apps/dhcpserver/dhcpserver.c)
includes Option 3 conditionally. The installed headers agree with those APIs.
The local subnet remains directly reachable by a leased client.

Return values appear in the bounded normal-Unit trace as
`esp_netif_dhcps_stop`, `dhcps.router.SET`, DHCP start/status checks and
`dhcps.router.GET`. `/status` adds `dhcp_router_option: false` only after the
API readback confirms it. This confirms configuration, not a captured packet.

An old lease/default route may remain until the client reconnects/renews.
iOS makes its own cellular-routing choices; removing an advertised default
router is not proof that a particular iPhone will use cellular internet.

## 2. Credentials and explicit migration

New factory provisioning uses the **last two bytes of the actual SoftAP MAC**:

| Identity | Example for AP MAC ending 72:9c |
| --- | --- |
| SSID | CoreS3SE-DMX-729C |
| Default password | espdmx-729c |

`esp_read_mac(..., ESP_MAC_WIFI_SOFTAP)` supplies the initial address. After
driver initialization and AP mode selection, `esp_wifi_get_mac(WIFI_IF_AP,...)`
must match it before configuration proceeds. No assumed STA MAC is used.
WPA2-Personal/CCMP remain enabled, configured before radio start.

The scheme is deterministic across boots. A four-hex suffix is not globally
unique and can collide across devices. The password is fully predictable from
the advertised SSID: these are **convenience credentials, not strong access
authentication**. No password is printed to logs or returned by `/status`.
Local Settings deliberately continues to display it.

### Existing credentials are not automatically overwritten

The original 64-byte version-1 blob has no factory/custom origin marker. A
24-character password matching its generator alphabet cannot prove factory
origin. Consequently:

- Valid version-1 SSID/password bytes are retained in NVS unchanged. They load
  as LEGACY in RAM, including plausible user-customized values. A normal upgrade
  does not replace them or start writing them on every boot.
- Fresh devices create a version-2, 108-byte blob under the same dedicated
  `dmxwifi/ap1` key. It has explicit factory/custom origin metadata and space for
  printable ASCII WPA2 passphrases of 8..63 characters. SSIDs are 1..32 printable
  ASCII bytes. The example default has 11 characters.
- Valid tagged custom records remain unchanged. This reserves the storage and
  validation path for a future stronger user-selected password; no remote
  password setter is added in this stage.
- Unknown/corrupt existing records fail closed and are preserved. They do not
  silently become defaults. A tagged factory record must match this device's
  deterministic identity. Unsupported future formats likewise require review.

### Local Wi-Fi-only reset

In Sender Settings, scroll down to **HOLD RESET WIFI 2s** (content rectangle
x=24..239, y=450..477). Hold still for two seconds. Moving outside or by the
existing gesture threshold cancels the action; its press/hold/release are
consumed so they do not become a scroll, slider or power-button command.

The action writes and verifies only a `dmxwifi/factory=1` intent marker, once
per pending request. It does not change active credentials or disconnect clients
immediately. Settings shows **RESET SAVED: RESTART TO APPLY**. Use the existing
fixed RESTART button and its existing double-tap confirmation; no new automatic
restart or DMX shutdown path is introduced.

On the next boot, preparation replaces only the Wi-Fi credential blob with the
new factory identity, verifies the write/readback, then removes the intent
marker. If writing/verifying fails, the marker is not acknowledged and networking
fails closed. Retrying after interrupted application produces the same default,
not another random password. The write/readback ordering is tested in source;
actual NVS/power-interruption behavior still requires hardware validation.

This explicitly requested operation also permits recovery from an invalid
Wi-Fi blob. It deliberately replaces a custom Wi-Fi key if the physical user
requests reset; it never silently resets one during an ordinary boot.
Preset/settings namespaces, their keys and their formats are not opened or
erased. There is no `clear()`, erase-all, Flash reset or deletion of `ap1` before
replacement. Reset intent is written only by the application owner, with a task
identity check. HTTP handlers and Wi-Fi callbacks cannot request or write it.

## 3. Stations and Settings updates

`include/softap_config.h` defaults `DMX_WIFI_MAX_STATIONS` to **4**. It can be
overridden as a build define within 1..8, matching this installed SDK's eight
DHCP lease slots (the radio alone supports more). A second compile-time check
uses `CONFIG_LWIP_DHCPS_MAX_STATION_NUM`. Existing dependencies and PlatformIO
configuration are unchanged. Firmware applies and reads back
`wifi_config_t.ap.max_connection`; a mismatch prevents READY.

Associated Wi-Fi stations are tracked separately from HTTP sessions:

- `WIFI_EVENT_AP_STACONNECTED` and `...STADISCONNECTED` update a fixed MAC set
  under the short status mutex. Duplicate events cannot inflate/underflow the
  count; the set cannot exceed the configured capacity. AP stop clears it.
- Callbacks update status/revision only: no M5GFX, Controller, NVS, socket work,
  driver-list queries, allocation or printing.
- The existing worker reconciles against `esp_wifi_ap_get_sta_list` at its
  original 1 Hz. A generation check prevents a query taken before a newer event
  from overwriting that newer state. Event delivery/scheduling can introduce
  temporary lag; periodic reconciliation repairs stale/missed state.
- Settings and `/status` use the same copied station count. `/status` also adds
  `max_stations`. HTTP still allows **three accepted sockets**, backlog two and
  the existing request/byte/deadline limits. Four stations does not mean four
  simultaneous HTTP requests are guaranteed service.

Visible Settings displays `WiFi READY - Clients 0/4`, `1/4`, etc. STARTING and
FAILED (with error) use their actual service state. The owner checks a small
revision counter during Settings servicing; a full frame is requested only when
phase/client/reset state changed and the network status row is visible. Memory
sampling does not trigger periodic redraws. Existing tone deferral and the
single PSRAM canvas draw/push path are retained. No network callback renders.

The Settings content height increases from 480 to 540 to fit the reset action
and its explanation. Existing fixed right-side buttons, sliders and page
layouts remain unchanged. The network panel shows the legacy-key notice when
appropriate and warns that the default key is predictable.

## Resource and test results

The Stage 5A trace expands from 48 to **64 bounded entries** so added DHCP/MAC/
event return checkpoints fit without losing startup completion. This costs 832
additional static bytes in normal Unit. It still prints only from the owner in
bounded USB FIFO chunks, uses no HWCDC initialization and logs no credentials.
Base/Stage 0 diagnostic variants still compile this temporary trace out.

All four PlatformIO builds succeeded:

| Environment | Static RAM bytes | Flash bytes | Application BIN bytes | Result |
| --- | ---: | ---: | ---: | --- |
| cores3se_unit | 63,724 | 1,029,185 | 1,029,600 | PASS |
| cores3se_base | 59,844 | 1,024,473 | 1,024,896 | PASS |
| cores3se_unit_diagnostics | 62,460 | 1,026,697 | 1,027,120 | PASS |
| cores3se_base_diagnostics | 62,460 | 1,026,665 | 1,027,088 | PASS |

Relative to Stage 5B, static RAM changes are +952/+120/+112/+112 bytes;
Flash changes are +3316/+2992/+2940/+2940. Most of the normal-Unit static
increase is the 832-byte trace expansion. Runtime memory consumption at four associations
and during handshakes/HTTP traffic remains unmeasured for Stage 5C. The existing
32 KiB free-internal / 16 KiB largest-DMA runtime guards, startup admission guards,
stack and transport checks are unchanged.

All four artifact receipts identify source SHA-256
`a113033a362802f6ab8a302125bc040ec050c1835d68e910f8f7fbe305c4d892`.
Normal Unit application BIN SHA-256:
`68a5228ca52cb646f151313dcb49a551b6c391a9c4c458c194a3b967cad6d413`.
The unchanged artifact hook validates the 16 MiB image header/partition table
and retained embedded gzip bytes. Logs/resources/host-test outputs are under
ignored `dist/stage5c-validation-20261025/`. No FULL or migration bundle changed.

Tests passed:

- Actual pure credential policy: MAC suffix case/leading zeros, deterministic
  defaults, 8..63 WPA2 validation, preserved ambiguous legacy/custom records,
  factory mismatch/corrupt/unknown-version rejection and explicit reset choice.
- Actual bounded station-set policy: duplicates, unknown disconnect, capacity,
  reuse and clear; compiled/executed with capacities 1, 4 and 8.
- Extracted production DHCP-call fragments against a simulated SDK contract:
  exact option id/value/size, stopped-before-SET, GET-after-start and error
  propagation. Source checks verify actual startup ordering and unchanged IP/mask.
  These tests **do not deliver real leases or test iOS**.
- Source ownership/reset checks, existing whole-view/application guards and
  actual fullscreen-allocation/heap-guard tests.
- Existing Controller, Dispatcher, adverse ISR/transport and Stage 3E clash
  regressions, HTTP policy/real assets, startup history, patch-state checks and
  all six infrastructure checks. Domain/driver sources are unchanged.

Reproduce the new host suite after a normal Unit build:

```powershell
python -B tests/network/check_softap_integration.py --dhcp-header dist/stage5c-host/dhcp_under_test.h
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude -Idist/stage5c-host src/softap_policy.cpp tests/network/test_softap_policy.cpp -o dist/stage5c-host/softap-tests.exe
.\dist\stage5c-host\softap-tests.exe
python -B tests/network/check_integration.py
python -B tests/dispatcher/check_integration.py
python -B tests/network/check_startup_trace.py
python -B tests/network/check_canvas_memory.py
```

## Files

Modified relative to the preserved Stage 5B working tree:

- `include/network_service.h`: station-limit/revision/router/reset status and
  local credential/reset discovery APIs.
- `src/network_service.cpp`: routerless DHCP, actual AP-MAC verification,
  persistent-credential decisions, reset marker and station event/reconciliation.
- `src/view_sender.h`: only the Settings network panel, revision refresh and
  local held reset action.
- `src/wifi_startup_trace.cpp`: bounded capacity 64.
- `tests/dispatcher/check_integration.py`, `tests/network/check_integration.py`:
  exact normalization of marked Settings additions before existing comparisons.
- `tests/network/check_startup_trace.py`: expected bounded trace capacity.
- `tests/network/probe_device.py`, `tests/network/README.md`: Stage 5C read-only
  status checks and host instructions.

Added:

- `include/softap_config.h`
- `src/softap_policy.h`, `src/softap_policy.cpp`
- `tests/network/softap_guard_support.py`
- `tests/network/test_softap_policy.cpp`
- `tests/network/check_softap_integration.py`
- `docs/STAGE5C_SOFTAP_USABILITY.md`

No changes to DMX transport/patch, Controller/Dispatcher, preset storage, PSRAM
framebuffer allocation, Receiver, partition table, embedded assets or their
packaging pipeline. Historical files, logs, backups, other worktrees and the
existing stash are preserved. No release bundle is regenerated.
Final preservation checks found all 326 preexisting files intact, with only the
nine intended existing files changed. All four receipts match the current
source and unchanged partition images. `git diff --check` and added-file
whitespace/EOF checks pass. HEAD, branch and stash remain unchanged; nothing
is staged.

## Physical acceptance checklist

The following are user instructions only. No device was opened or flashed.

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
pio device monitor -e cores3se_unit --port COM8 --baud 115200 --rts 0 --dtr 0 --filter time --filter log2file
```

Use the known Unit + Battery Bottom power arrangement and existing 16 MiB
layout. With the monitor open, reset and retain the whole new trace through
READY/FIRST_FAILURE. Do not overwrite prior logs or reset preset NVS.

1. Confirm READY, PSRAM canvas `caps=4`, zero transport errors and all P1-P8
   presets/settings retained. Compare internal free/minimum/largest-DMA/PSRAM
   against the tested Stage 5B baseline and keep all guards unchanged.
2. On an existing version-1 device, first confirm its **old credentials still
   work**. To adopt the new default, use HOLD RESET WIFI 2s, verify the saved
   notice, then the existing RESTART double tap. Confirm the new suffix matches
   the actual AP MAC's last bytes, and presets remain intact. Repeat a normal
   cold boot to verify credential persistence without another reset.
3. Verify a brief touch or moved/canceled hold does not request reset. Confirm
   reset affects Wi-Fi only, and ordinary active DMX stays unchanged while the
   intent marker is saved. No new restart behavior should occur until requested.
4. On iPhone, forget/rejoin the relevant SSID to obtain a **fresh lease**. Keep
   cellular data enabled. Confirm `http://192.168.4.1/status` loads and reports
   `dhcp_router_option: false`. Test an uncached external website over cellular
   while the phone remains associated; do not infer success from the Wi-Fi icon
   or API readback. VPN, OS policy and connection assistance can affect routing.
5. Renew/reconnect again to cover lease renewal. Where capture is available,
   inspect actual DHCP OFFER and ACK messages: a valid local address, /24 mask,
   DHCP server identity and lease duration must remain, while **Option 3 must
   be absent**. Neither a zero router address nor merely an unchanged cached
   lease is sufficient. No captive portal or DNS interception should appear.
6. Connect Windows as a second device: Settings should show Clients 2/4 and
   `/status` stations=2, max_stations=4. Confirm local HTTP access. Inspect
   `ipconfig /all` and `route print -4`: this Wi-Fi interface should have a
   connected 192.168.4.0/24 route and no DHCP-provided default gateway. Renew that
   interface's lease and recheck; other interfaces may retain their own defaults.
7. Disconnect each device and verify prompt count reductions. Repeat rapid
   reconnects and open/close HTTP tabs: tab/socket changes alone must not change
   the Wi-Fi count. Connect third/fourth stations if available; a fifth must not
   associate. Existing three-socket HTTP limits/429 behavior may require retries
   during simultaneous browser loads and are distinct from association capacity.
8. Under concurrent refresh traffic, exercise rapid sliders/Precision, Keypad,
   presets, blackout, Settings scrolling, speaker, sleep/wake and mode changes.
   Require no persistent heap/PSRAM decline, no transport fault/reset or display
   corruption. Inspect task watermarks, owner gaps and at least the original
   internal/DMA reserves; extra clients/handshakes have real runtime costs.
9. Run the updated read-only probe when other refreshing tabs are idle:

   ```powershell
   python -B tests/network/probe_device.py --host 192.168.4.1 --output dist/NEW-stage5c-http-probe.jsonl
   ```

   Confirm the address in Settings and use a new filename. The probe now checks
   the new router-option/maximum-stations fields; it does not provision Wi-Fi,
   change credentials, deliver leases, reset the device or log the password.
10. Repeat cold boots, lease renewals, client reconnects and XLR disconnect/
    reconnect. Preserve the separate external RS-485 capture gate for complete
    513-slot coherence, break/MAB, cadence/gaps and electrical quality under load.
    Software status and fixture response alone do not establish wire integrity.

Physical Stage 5C acceptance is still pending. In particular, **iPhone cellular
routing has not been verified**. Before any future remote DMX control, require
stronger provisioning/authentication and the planned bounded owner-command path;
the public MAC-derived convenience key must not be treated as authorization.
