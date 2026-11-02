# M5Stack CoreS3SE DMX Controller v0.3.0

Release notes; hosted publication is separately authorized. See the [complete changelog](../CHANGELOG.md)
for all changes since public v0.2.0 and [installation](V0_3_0_INSTALLATION.md) for
commands. Binary manifests identify the final committed source. The obsolete local tag
requires a separately authorized correction.

## Highlights

- Complete offline web UI: responsive Editor, Presets, Settings and Status,
  including tested iPhone Safari and Home Screen operation.
- Live 512-channel faders, +/- and numeric entry, synchronized with touchscreen
  controls and other clients. Reconnect resynchronizes without replaying commands.
- P1–P8 web Load/Save/Delete, confirmations, double-tap loading, Loaded/Modified
  metadata and synchronized blackout using the original snapshot restoration.
- **Backup** JSON Import/Export of the entire saved collection without changing
  live output. Custom confirmation, responsive progress, error feedback and
  timestamped `DMX-Presets-YY-MM-DD-HH-mm.json` downloads.
- Shared brightness/timeout/volume, browser-confirmed Restart and original Power
  Off, five-second physical CANCEL, physically approved Wi-Fi configuration,
  local persisted Wi-Fi ON/OFF and separate RESET WIFI.
- Physical fader capture dead-zone fix, reliable fullscreen navigation during
  scrolling, aligned Settings and visible build information.
- Safer DMX packet ownership/full-frame qualification and corrected sender
  RS485_CLASH behavior; sustainable transactional NVS Save/Delete after Import.
- PSRAM framebuffer memory recovery, bounded HTTP/WS handling and deterministic
  embedded Core/UI assets. No filesystem image or separate no-Wi-Fi build.

The existing physical Precision Editor, keypad, presets/copy, blackout, battery
warnings, sound and sleep behavior predate this release and remain available.

## Installation and images

- **Recommended:** `CoreS3SE-DMX-Unit-v0.3.0.bin`, Unit DMX U183 + Battery Bottom.
- **Experimental:** `CoreS3SE-DMX-Base-v0.3.0.bin`, DMX Base + external PSU.

Both are complete factory images written at **0x0**. From **v0.2.0, erase flash
first**; presets/settings are lost. Factory images cover NVS and may destroy its
data even if a separate erase is omitted.

For the tested **current-format v0.3.0**, an application-only update at 0x10000
without erase preserves presets/settings. Extract the matching app as documented;
do not use the complete factory image for this path. Older RC/pre-cleanup labels
alone do not establish compatibility. No new migration system is introduced.

## Acceptance and limits

The user physically accepted the cleaned firmware: complete erase/installation,
normal boot, Save/Load/Delete/Export/Import, reboot persistence and a no-erase
current-v0.3.0 app update retaining presets. Automated current-format storage tests
also cover real ESP-IDF NVS, capacity/error/interruption cases, Controller,
Dispatcher, WebSocket, transport and frontend regressions. These do not establish
physical power-cut recovery or electrical DMX timing.

- Unit is recommended; Base remains experimental with known random fixture
  glitches. Unit acceptance does not certify Base hardware.
- Keep Import/Export **open and active** until completion; backgrounding cancels
  the connection/transfer. Inspect current data before retrying an uncertain Import.
- **Change the predictable factory Wi-Fi password before untrusted use.**
  Associated clients share control; no separate per-user HTTP/WS authorization/TLS.
- Windows/Intel AX200 driver 24.20.2.1 compatibility: verified workaround is
  **MIMO Power Save Mode → No SMPS**; not a confirmed firmware defect.
- Slow physical coordinate stalls, fixed-scale accessibility limits and historical
  synchronous TCP send stalls remain. Recovery is not a permanent nonblocking-send
  fix. External frame/timing capture, shutdown-zero guarantees and interrupted-
  installation recovery remain unverified. OTA/LittleFS/STA are not implemented.

Read [known issues](../KNOWN_ISSUES.md) before installation or live use.

## Source and notices

Source fingerprint:
`688fe1feb3969ebf8dec8eef0ddd4f623bbc1c36dcf3bd08f9f4bbd40aca9543`.
The production images are rebuilt from committed source and retain its clean
build identity. `BUILD_INFO.json` gives the exact commit and checksums. The old
local-only v0.3.0 tag is unchanged in this source-finalization task; recheck remote
tags before the separately approved correction. Never force-update a public tag.

Original M5Stack attribution, project LICENSE and third-party notices accompany
binary delivery. Matching source/build/relink materials must be refreshed for the
final cleaned commit; older frozen-source archives are not silently relabeled.
GitHub source/repository contents and downloadable firmware attachments are
separate; local logs, old RCs and investigation artifacts are not release assets.
