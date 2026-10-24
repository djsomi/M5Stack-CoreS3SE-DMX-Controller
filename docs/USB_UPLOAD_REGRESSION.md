# USB Upload Regression Investigation

Investigation date: 2026-10-24. Status: user-performed hardware A/B follow-up
completed; diagnostic-build stub failure isolated, root cause still unproven.
No USB initialization or upload-speed change is proposed here.

## Hardware A/B Follow-up (Supersedes the Initial Broad Regression Report)

The user supplied these completed CoreS3SE results after the initial investigation.
These are user-performed hardware tests, not device operations run by the agent:

| Firmware | Test | Result |
| --- | --- | --- |
| Old stable v0.2.0 | Automatic reset + RAM stub | PASS |
| Stage 2 diagnostics | Automatic reset + RAM stub at 115200 | FAIL |
| Stage 2 diagnostics | Automatic reset + `--no-stub` | PASS |
| Stage 2 normal | Automatic reset + RAM stub at 115200 | PASS |
| Stage 2 normal | Automatic reset + RAM stub at 1500000 | PASS |

The demonstrated failure is associated with the **diagnostic build**, not the
normal Stage 2 build. In particular, the diagnostic failure at 115200 cannot be
explained merely as failure to switch to 1500000: that test skips the baud-change
command. Successful ROM/no-stub communication and failing stub communication
narrow the diagnostic handoff investigation, but do not identify the responsible
register or code change. HWCDC initialization remains a hypothesis, not a proven
root cause. No original full trace, artifact hashes or USB-event capture for these
new trials was supplied with the result table; do not invent those details.

These identification/`flash_id` results do **not** prove flash programming works.
Actual repeated automatic firmware uploads (at least three), monitor reconnection
and reboot validation remain pending. No complete three-rate matrix is claimed;
460800 was not included in the supplied results.

Keep the existing USB initialization, upload_speed=1500000, monitor_speed=115200
and diagnostic environments unchanged. Use **normal cores3se_unit** as the primary
Stage 3 hardware build. Revisit diagnostic USB behavior before later Wi-Fi resource
testing, which will depend on reliable diagnostics. The original investigation
and command templates below are preserved as historical evidence, not a request
to repeat Experiment 1 or flash anything now.

## 1. Scope and Repository Safety

- Branch: `codex/stage3-dmx-transport`.
- Source baseline: `4c973007d74a1f24319e0e375a56107bc261b784` (Stage 2).
- The tracked working tree was clean before this investigation. The historical
  `_kezdeti kezi fejlesztes` folder (whose actual name contains accented letters),
  `logs/`, ignored binaries, existing development snapshots and build outputs
  were preserved.
- Stash `996a2f9378208540b0fd3b544e7d9908affe8e84` was not restored or changed.
- Only this report was added. No firmware, PlatformIO configuration, library,
  transport, Controller, UI, preset, power, partition or packaging fix was made.
- No device connection through esptool, reset, upload, erase, Git checkout,
  history change, commit, merge, push or release was performed. Serial-port
  enumeration did not open COM8. Public release assets were compared in memory.

**Updated conclusion:** the diagnostic-build association is demonstrated by the
follow-up table above. Failure of normal Stage 2 firmware is not demonstrated.
The exact first broken diagnostic revision and root cause remain unproven.
Changing upload speed or USB initialization now would be speculative.

## 2. Reported Reproduction and Exact Failure Phase

User observations on CoreS3SE / ESP32-S3 rev v0.2, Windows 11, COM8:

- Old application-only binary written at `0x10000`: works, and subsequent normal
  automatic uploads work without manual download mode.
- Initial broad report about new Stage 0/1/2 firmware: application works; manual
  download permits uploading; automatic upload fails with the excerpt below.
  The follow-up above narrows the demonstrated problem to diagnostic firmware:

```text
Connecting...
Chip is ESP32-S3 (revision v0.2)
Features: WiFi, BLE
Crystal is 40MHz
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 1500000
A fatal error occurred: No serial data received.
*** [upload] Error 2
```

The supplied excerpt has no `Changed.` line. A complete trace is still needed.

| Phase | Evidence / interpretation |
| --- | --- |
| A: enter download mode | Successful enough to communicate with ROM; not a simple failure to enter download mode. The exact reset history is not recorded. |
| B: ROM synchronization | Successful chip/crystal identification and RAM upload. |
| C: stub upload/start | Successful receipt of the `OHAI` greeting. Does not prove that the stub can subsequently receive commands. |
| D: baud-change command | Host sends opcode `0x0f` at the initial connection setting and waits for its reply. The supplied log stops here. |
| E: host port baud update | Not reached if `Changed.` is genuinely absent. USB disappearance/re-enumeration at that instant has not been captured. |
| F: flash writing | Not reached in this failure. No evidence of a flash write, erase or verification failure in the supplied excerpt. |

Installed esptool 4.5.1 `esptool/loader.py:1078-1087` orders the operations as:
print `Changing...`, send/wait for `ESP_CHANGE_BAUDRATE`, print `Changed.`, set
the pySerial port baud, sleep 50 ms, flush input. Therefore this log does **not**
demonstrate corruption after a successful 1.5 Mbaud host switch. It demonstrates
no received reply to the request preceding that switch.

The installed bundled stub source is equally important:
`flasher_stub/stub_flasher.c:439-466` sends `OHAI` **before** `stub_io_init()`.
`esptool/loader.py:947-981` prints `Stub running...` upon receiving that greeting.
The first command may expose broken stub RX initialization even though its TX
greeting succeeded. At `stub_flasher.c:346-356`, the baud response is sent before
the stub actually changes its UART divider.

The old application is no longer executing normally after successful ROM entry.
Possible effects of the preceding firmware must involve retained peripheral or
power state, reset history, host USB state, or a different test condition, not an
ongoing call to the Dispatcher during flashing.

## 3. Exact Known-Good Binary Identification

Input, inspected without modification:

```text
LATEST BUILD/firmware_unit.bin
Length: 599632 bytes
SHA256: 9f5ab8b7d129410f009ea554a19900444678dcf4ff1d8bc2b7d7e18ec223c1b8
```

This is **exactly the application section**, from offset `0x10000` to EOF, of
the locally retained **and independently fetched published** release asset:

```text
M5Stack-CoreS3SE-DMX-v0.2.0-UNIT-FULL.bin
Length: 665168 bytes
SHA256: f89405c9875a46c4b2ce88e4e9713790788f6b07d8ce5d326dd0fbefee06bb39
```

The public `SHA256SUMS.txt` agrees. This establishes artifact identity with the
[published v0.2.0 release](https://github.com/djsomi/M5Stack-CoreS3SE-DMX-Controller/releases/tag/v0.2.0),
not just a filename or a matching size. That release identifies source commit
`32c0571049fa57c584167f5a4f6f07e67668430d`. The local annotated v0.2.0 tag object
is `4be591e59502fc8f15bdfd64f8544918300bb07b`, pointing to that commit.
The binary itself has no trustworthy application Git/version field; correspondence
to the source relies on the release provenance, not recovery of a Git SHA from
machine code.

### Image Headers and Metadata

Inspected using installed `esptool.py --chip esp32s3 image_info --version 2`.

| Field | Exact old Unit | Current Stage 2 Unit | Current Unit diagnostics |
| --- | --- | --- | --- |
| Image format / segments | v1 / 6 | v1 / 6 | v1 / 6 |
| Entry point | `0x40377be0` | `0x40377be0` | `0x40377c14` |
| Chip ID | 9 (ESP32-S3) | 9 | 9 |
| Header flash mode | DIO | DIO | DIO |
| Header flash frequency | 80 MHz | 80 MHz | 80 MHz |
| Header flash size | 8 MB | 8 MB | 8 MB |
| Min / max chip revision | v0.0 / v655.35 | same | same |
| WP pin | `0xee` | same | same |
| Image checksum / appended hash | valid / valid | valid / valid | valid / valid |
| Embedded ELF SHA256 | `0df007ee8a55f01e806d8f2d1c55851d63413947e3d8b6b2641b3926ffe186dd` | `372b9c2e38677f4a6a6b611f276ee77085a1968b8886515cd243c19e96cdf9d1` | `f4e453964cecc0f2c98b5dc9e173a09628a665ce37a4572948fc8591b0ddf7dc` |

All inspected app descriptors report `arduino-lib-builder`, version
`esp-idf: v4.4.7 38eeba213a`, IDF `v4.4.7-dirty`, secure version 0, and
`Mar 5 2024 12:12:53`. That is SDK/library-build metadata, **not** the actual
application build date, release version or proof that the firmware is from 2024.
The diagnostic banner `v0.2.0-base/32c0571+stage0` is also stale and must not be
used to identify a tested binary. No provenance change was made in this task.

The normal Unit image changes code/data segment sizes and addresses, as expected
after Controller/Dispatcher extraction; there is no incompatible chip or flash
header change. Both Base image headers also select DIO / 80 MHz / 8 MB.

### Current Build Artifact Identities

These are raw application binaries, not FULL images, built from Stage 2 during
this investigation. Hashes identify these exact local files, not every future
rebuild (timestamps, debug paths and dependency-directory names can affect bytes).

| Environment | Image bytes | SHA256 |
| --- | ---: | --- |
| cores3se_unit | 604528 | `bb82bfcf3c475bb5f92c4cf8c21dd259ccc45994852f0c159d44870823a36da8` |
| cores3se_base | 604480 | `1ae30ad0f988baf14d919fd8ac9f241043bc5ee19f63f4747019f36a0b37e627` |
| cores3se_unit_diagnostics | 606784 | `301598b8996c63c4a10b8c31364f73760f5ad0b8fcf9bb128b0d157e9cb55ca7` |
| cores3se_base_diagnostics | 606768 | `6899500064f0992b8aef5ba564191350080ac3f02d1dd0e8813dbbfb3410512d` |

## 4. Bootloader / Partition Confounder

PlatformIO does not perform the same operation as the user's app-only write.
Installed platform builder `builder/main.py:448-465` and Arduino
`tools/platformio-build.py:209-215` schedule:

```text
0x00000  bootloader.bin
0x08000  partitions.bin
0x0e000  boot_app0.bin (OTA selection initialization)
0x10000  firmware.bin
```

This investigation compared current local components against their corresponding
byte ranges in the verified old published Unit FULL image:

| Component | Bytes | SHA256 | Equal to old FULL range? |
| --- | ---: | --- | --- |
| bootloader | 15104 | `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343` | YES |
| partitions | 3072 | `1d9cca96de0fe07ad7fc0648b9878ddecd9ce565e38b589ad20fea698ed4c80c` | YES |
| boot_app0 | 8192 | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` | YES |

Thus no changed local bootloader/partition/boot_app0 **contents** were found.
That does not read or establish the current device contents or its OTA selection.
Rewriting `boot_app0` can replace live OTA metadata even when the packaged file
is unchanged. A failed attempt before flash writing did not write these images.

The configured table decodes to:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x330000
app1      0x340000  size 0x330000
spiffs    0x670000  size 0x180000
coredump  0x7f0000  size 0x010000
```

The real flash is 16 MB according to the prior hardware diagnostics; retaining
the tested 8 MB layout is intentional here. No filesystem or partition work.

`board_build.flash_mode = qio` is unchanged. PlatformIO 6.7.0 builder
`main.py:88-99` maps QIO/QOUT to a DIO image-header setting for this Arduino
path, while selecting the QIO bootloader/SDK configuration. This explains the
matching DIO image headers; it is not evidence of a newly changed flash mode.

**Safe comparison rule:** first read/verify the device's partition table and
active boot selection; then alternate only raw app binaries at the verified
active app offset. Do not use `pio ... -t upload`, FULL/merged binaries,
`--erase-all`, `erase_flash`, `erase_region`, partition writes or OTA metadata
writes for that comparison. Both supplied apps fit app0, and their app-only
sector erases cannot overlap NVS if app0 really is at `0x10000`.

## 5. Git Revision and USB/CDC Audit

| Revision | Relevant changes / evidence |
| --- | --- |
| `b33a9b8` baseline | Already specifies 1500000 upload, CDC_ON_BOOT=1, USB_MODE=1, QIO and platform 6.7.0. M5 libraries not yet version-pinned. |
| `c38384d` reproducibility | Pins M5Unified 0.2.21 / M5GFX 0.2.28 and introduces the three-expression esp_dmx S3 patch. Predates the verified good v0.2.0 image. |
| `06931d8` power startup | Profile-specific battery-guarded external power startup; predates the good image. |
| `32c0571` v0.2.0 | Exact known-good artifact identified above. Same platform, M5 pins, upload speed, USB flags and power startup as Stage 2. |
| Stage 0 working state | Adds guarded diagnostics, including HWCDC begin and bounded serial output. Not an independent commit on the current ancestry. An ignored historical `dist/stage1-stage0/` snapshot exists but is not treated as a proven commit/artifact identity or overwritten. |
| `0e43640` Stage 1 | First committed post-v0.2.0 state, containing both Stage 0 and Controller extraction. Normal env configuration unchanged; only diagnostic envs added. Earliest committed candidate, **not established first bad revision**. |
| `4c97300` Stage 2 | Dispatcher/Controller/UI routing changes. No changes to platformio.ini, diagnostic source/header or esp_dmx patch relative to Stage 1. No new USB/reset code. |

The initial Stage 0/1/2 association is retained as user evidence, but there is
no complete artifact-hash/environment/reset-history/upload trace for each stage.
Stage 0 versus Stage 1 cannot be bisected solely through these commits.
No speculative reconstructed Stage 0 firmware was created. If needed after the
first experiment, exact Stage 1 builds can be made in a separate managed worktree
at `0e43640` without switching or changing the current checkout. A fresh v0.2.0
rebuild is not a substitute for the exact already identified old binary.

### Installed USB Implementation

- `platformio.ini:1-64`: normal and diagnostic Unit/Base envs inherit
  `-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`, monitor 115200 and upload
  1500000. No new USB mode, upload protocol or reset override was introduced.
- Board JSON also defines USB_MODE=1 and VID/PID `303a:1001`. Its default upload
  speed is 460800, overridden by the project since the original baseline.
- Installed Arduino `cores/esp32/HWCDC.h:110-115` maps `Serial` to `HWCDC` for
  these flags: hardware USB Serial/JTAG, **not TinyUSB/USB-OTG USBCDC**.
- Arduino `main.cpp:55-68` starts `Serial`/`USB.begin()` automatically only for
  USB_MODE=0. Merely defining CDC_ON_BOOT does not execute HWCDC.begin here.
- M5Unified `M5Unified.hpp:91` defaults `serial_baudrate=0`.
  `M5Unified.cpp:2521-2524` calls Serial.begin only if that value is nonzero.
  Application setup does not set it. Its power startup at `src/src.cpp:727-737`
  is unchanged since the verified good release. Do not confuse this regression
  with the previously resolved external-output USB cold-start issue.
- `src/hardware_diagnostics.cpp:117-120` explicitly calls `Serial.begin(115200)`
  and sets the TX timeout to zero **only with DMX_HW_DIAGNOSTICS=1**.
  `HWCDC.cpp:260-309` makes that begin nontrivial: pull USB pins low, wait 10 ms,
  re-enable the PHY/pullup/pads, enable USB interrupt sources and install an ISR.
  The `baud` argument is not used to set a physical USB transfer rate there.
- Normal builds compile out the entire diagnostics implementation and its calls.
  ELF symbols confirm no `hardware_diagnostics::*` in the normal Unit image;
  the diagnostic Unit includes `hardware_diagnostics::begin()`. HWCDC.begin is
  linked in both (M5Unified contains a conditional call); linkage alone does not
  prove it is executed in the normal build.
- No application `USB.begin()`, `Serial.end()`, deep/light sleep, USB mode switch,
  DTR/RTS callback or 1200-baud reset handler was found. Display sleep is backlight
  control, not MCU sleep. Existing settings restart calls `esp_restart()`;
  shutdown calls `M5.Power.powerOff()`; both predate Stage 0.
- Normal UI Serial messages remain; diagnostics suppress those legacy messages
  and use the bounded Stage 0 queue. They do not parse USB input as commands.
  The normal Serial.write implementation does not implicitly call begin.
- The legacy initial GPIO19 variable in setup is replaced with Unit TX2/RX1 or
  Base TX7/RX10 before dmx_set_pin on detected CoreS3/CoreS3SE. There is no newly
  introduced direct use of the USB GPIO19/20 pins. Prior logs identify CoreS3SE.
  A board-detection failure would be a separate hypothesis, not established here.
- Arduino `USBCDC.cpp:256-257` has a 1200-baud reboot path for TinyUSB, but this
  project uses HWCDC. No 1200-baud touch should be added as an assumed fix.

The fixed-function interface and sleep/pin limitations are documented in the
[matching IDF 4.4.7 USB Serial/JTAG guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/usb-serial-jtag-console.html).
Its reset-persistence discussion is relevant, but does not by itself prove which
interrupt register bits survive this device's specific auto-reset sequence.

### esptool Reset and Stub Details

Paths below are in the installed `tool-esptoolpy` package, version 4.5.1:

- `esptool/loader.py:574-610`: default_reset automatically selects
  USBJTAGSerialReset when the detected PID is 0x1001; `--before usb_reset` forces
  that same strategy. This is not necessarily an electrically different reset.
- `esptool/reset.py:33-42,106-125`: Windows DTR/RTS propagation workaround and
  USB Serial/JTAG control-line sequence. Not a UART bridge GPIO-reset assumption.
- `esptool/targets/esp32s3.py:222-250`: console detection and RTC-watchdog disable
  on normal ROM connection; the bundled stub also disables the RTC watchdog.
  Do not assert a watchdog reset solely because a timeout occurred.
- `esptool/__init__.py:662-730`: initial connection capped at 115200; stub launch
  precedes optional baud change; at requested 115200 the baud-change step is
  skipped. Both 460800 and 1500000 still request opcode 0x0f first.
- `flasher_stub/stub_io.c:43-78`: USB stub RX handler clears only OUT_RECV_PKT;
  RX setup ORs that enable bit into the interrupt-enable register, rather than
  replacing all enables. Arduino HWCDC enables OUT_RECV_PKT, IN_EMPTY, BUS_RESET
  and SOF. **If** other enabled/pending interrupts survive auto-reset, the stub
  could receive repeated interrupts it does not clear and fail to process the
  first request. Survival and causation have not been measured. A full hardware
  reset may differ from the automatic reset.
- `flasher_stub/stub_io.c:289-302`: bundled source skips UART-divider changes
  for USB-OTG, but not USB Serial/JTAG; the latter can modify UART0 as an unrelated
  side effect. This does not make the USB cable carry 1.5 Mbaud UART signaling.
  This code runs after the response, so it cannot alone explain its absence.

These are the package's bundled stub sources. This investigation did not rebuild
or modify the distributed stub; source-level reasoning is not hardware proof.
Distributed `stub_flasher_32s3.json` SHA256:
`76863d24c088a217b6538cf2437241b07a9d6b2e4aae5ad4c2e3953099ac490a`.
Installed HWCDC.cpp SHA256:
`c5362242196b6fcebaf8c83588de8bba509d711e9c216d7e61fe745c9d080d89`.

### Passive Windows Observation

COM8 was present, status OK, without opening the port:

```text
USB Serial Device (COM8)
VID:PID = 303A:1001
USB serial = 1C:DB:D4:BA:72:9C
Location = 1-3.2:x.0
PnP instance = USB\VID_303A&PID_1001&MI_00\7&A7BF8FE&0&0000
```

COM3 belongs to a different device (`043e:9a39`), not the CoreS3SE.
There is no live disconnect/re-enumeration trace for the failed transfer yet.
Enumeration alone cannot show whether Windows retained a stale handle.

## 6. Ranked Hypotheses and Discriminating Evidence

1. **Firmware-associated USB/reset/stub RX handoff state.** Best matches old/new
   association and manual/automatic difference; explicit HWCDC initialization is
   a concrete diagnostic-only change. The retained-interrupt mechanism above is
   a candidate, not a finding of actual register state. It does not explain a
   rigorously reproduced failure in a normal non-diagnostic build on its own.
   Test first-stub-command behavior without a baud change; then compare stub
   versus ROM and normal versus diagnostic app-only A/B under identical resets.
2. **Windows USB endpoint/handle/re-enumeration timing after auto-reset.** Can
   coexist with 1. Device identity and before/after timestamps plus complete
   trace distinguish disconnects, stale ports and loss without re-enumeration.
   Keeping the same COM number does not prove there was no USB reset.
3. **Baud-command-specific esptool 4.5.1/stub interaction.** Possible if 115200
   works but both rates requesting 0x0f fail. The log is not evidence of a physical
   high-baud USB signal problem. 460800 succeeds versus 1500000 fails would narrow
   this further; capture whether `Changed.` appears in either case.
4. **Uncontrolled bootloader/OTA/power/monitor/test history.** Local boot components
   match the old release, reducing the likelihood of changed file contents. Live
   flash, current loaded app identity, power source, battery, monitor ownership
   and reset procedure remain uncontrolled. Hold them constant during A/B.
5. **Unrelated application memory corruption or board misdetection.** No evidence
   currently attributes this to Controller/Dispatcher/DMX logic. Normal runtime
   behavior and heap observations are useful but cannot exclude all corruption.
   Consider only if controlled exact-artifact bisection requires it.

Reducing upload_speed is currently an **experiment/workaround, not a verified
fix**. At 115200 it also removes a protocol command. If any first stub request
fails, changing speed just changes the displayed failure point. Do not change
the project setting until the result is understood.

## 7. Hardware Experiments: Run One at a Time

**None of the commands in this section was run against the device by the agent.**
esptool identification/read commands do not erase flash, but they reset the MCU,
load/execute RAM code unless `--no-stub` is used, and interrupt DMX. Make fixtures
safe first. Keep cable, host port, power source, Battery Bottom/Unit attachment,
firmware and monitor state identical. Close every Serial Monitor before testing.
Do not enter manual download mode for the first experiment.

### Experiment 1: First Stub Request Without Baud Change -- Run This Next Only

Use the currently running failing firmware. Record the exact last uploaded
environment/file/hash, whether Settings/other pages were opened, uptime, and
power configuration. If its identity is uncertain, say so; do not flash another
image just to execute this step. Record whether the board was reset/power-cycled.

From the repository root in PowerShell:

```powershell
$python = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
$esptool = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = "logs/usb-upload-115200-$stamp.txt"
if (Test-Path -LiteralPath $log) { throw 'Log already exists; use a new name.' }
Get-Date -Format o | Tee-Object -FilePath $log -Append
& $python -m serial.tools.list_ports -v 2>&1 | Tee-Object -FilePath $log -Append
& $python $esptool --chip esp32s3 --port COM8 --baud 115200 --before default_reset --after hard_reset --trace flash_id 2>&1 | Tee-Object -FilePath $log -Append
$result = $LASTEXITCODE
"esptool_exit=$result" | Tee-Object -FilePath $log -Append
Get-Date -Format o | Tee-Object -FilePath $log -Append
& $python -m serial.tools.list_ports -v 2>&1 | Tee-Object -FilePath $log -Append
```

This has no flash-write operation. Serial-port listing does not open COM8.
The trace may contain the device MAC/USB serial; redact those before sharing
publicly if desired, but preserve command/reply bytes and timing.

**Stop and share the full log and observed screen/USB behavior before the next
experiment.** If the operation fails and leaves the device in download mode, a
normal reset/power cycle may be needed to run the app again. Record that action;
do not immediately repeat against an already running stub or manual ROM state.

Interpretation:

- `OHAI` then no response to the first real request even at 115200: not a
  successful high-baud switch failure; inspect stub RX/reset state next.
- Complete flash identification at 115200: automatic reset, ROM, stub and at
  least some stub commands work in this trial. Baud-command-specific or timing
  explanations remain; this is not yet proof that an upload will succeed.
- Different earlier failure: report it rather than forcing the prior diagnosis.

### Subsequent Decision Tree -- Do Not Run as a Batch

After review, repeat the same read-only `flash_id` test with a fresh log and the
same **running app** at 1500000, then 460800 as individually requested. Restore
the same starting state between trials. The supplied failing 1500000 excerpt is
not a replacement for a trace. All three baud settings must eventually be tested.

If stub requests fail, a next ROM-only comparison, with its own log, is:

```powershell
& $python $esptool --chip esp32s3 --port COM8 --baud 115200 --before default_reset --after hard_reset --no-stub --trace flash_id
```

`--no-stub` is a global option supported by installed 4.5.1, and `flash_id` is
available in ROM mode. It may configure RAM/peripheral flash-attachment state,
but does not program flash. ROM success plus stub failure narrows the handoff;
it does not automatically prove which register is responsible.

If appropriate later, ROM-only `read_mem` can inspect USB INT_ENA / INT_RAW /
INT_ST (`0x60038010`, `0x60038008`, `0x6003800c`; installed S3 register headers).
Read only, with expert review of traces; do not write/clear registers or call
`Serial.end()` as an unproven fix. Whether the ROM connection itself normalizes
those registers must be considered. Compare auto-reset versus full reset before
interpreting them. Manual download is a diagnostic comparator, not the solution.

### Later Controlled App-Only A/B: Safety Gate and Command Templates

Only after reviewing read-only tests and explicitly choosing this experiment:

1. Preserve logs and a readback of the existing low-flash region/NVS. No chip
   erase. The readback is private because NVS can contain user settings/presets.
2. Read the live partition table and OTA metadata, compare with the verified
   layout, and confirm which app partition actually boots. Stop if mismatched.
   The report's `0x10000` template applies only after app0 is confirmed active.
3. Hold bootloader, partitions, OTA selection and power/reset conditions fixed.
   Use app-only writes for **both** old and new. Use an agreed reliable recovery
   path/rate to install images; manual mode is allowed solely for installing a
   test candidate when necessary. Reset into the application before measuring
   its subsequent **automatic** operation.
4. Test exact old image -> current Stage 2 Unit normal -> Stage 2 Unit diagnostics
   -> exact old image again. Run the same first-request/baud matrix for each,
   never batch automatic tests starting from the previous stub state.
5. If normal Stage 2 fails and old passes under that controlled protocol, build
   Stage 1 normal/diagnostics from its exact commit in a separate checkout and
   test next. If only diagnostics fails, isolate the HWCDC startup/printing
   difference before editing Controller or DMX code. A Stage-0-only reconstruction
   must have an explicitly recorded source diff and artifact hash.

Readback template at agreed reliable 115200, after selecting a safe reset path:

```powershell
# Use a NEW output filename; never overwrite an existing backup.
& $python $esptool --chip esp32s3 --port COM8 --baud 115200 --before default_reset --after hard_reset read_flash 0x0 0x10000 "dist/usb-low-flash-UNIQUE.bin"
```

That includes the bootloader, table, NVS and otadata without changing flash.
It does not prove the active application identity. If app1 is selected or the
device layout differs, stop; do not reset OTA selection to make the test fit.

App-only templates, **not instructions to run now**:

```powershell
& $python $esptool --chip esp32s3 --port COM8 --baud 115200 --before default_reset --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 "LATEST BUILD/firmware_unit.bin"
& $python $esptool --chip esp32s3 --port COM8 --baud 115200 --before default_reset --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 ".pio/build/cores3se_unit/firmware.bin"
```

Each command is a separate experiment, with trace/log capture and review between
them. These write only app sectors; do not append other image address pairs.
Re-read and compare NVS/boot/table/OTA ranges before booting if testing that flash
writes preserved them byte-for-byte: application boot itself can update NVS.
Use recovery/reset options proven by earlier tests rather than blindly retrying.

All CLI spellings here were checked against the installed 4.5.1 help/source.
Do not substitute newer hyphenated command names or unsupported watchdog-reset
options from current online documentation.

## 8. Fix Decision and Validation Gate

**No fix implemented.** Evidence is insufficient to choose a safe firmware fix.
Keep platform 6.7.0, Arduino 2.0.16, esptool 4.5.1, existing USB flags and 115200
serial diagnostics. No reason established for an upgrade, partition change,
external-output modification, driver patch or permanent manual download mode.

If a specific HWCDC/reset handoff issue is demonstrated, isolate that lifecycle
fix without disabling Stage 0 logging or changing the application domain logic.
If a tool issue is demonstrated, require an exact upstream fix/version comparison
before proposing a tool upgrade. A lower upload setting is not called a root-cause
fix unless the controlled evidence establishes the supported configuration limit.

After an evidence-backed candidate exists, acceptance requires:

- Exact old image still passes its controlled test.
- Fixed firmware supports at least three consecutive normal automatic uploads,
  with the application running between attempts, no manual download mode.
- Exercise the original 1500000 setting as well as the agreed rate comparison;
  record why any retained lower setting is needed.
- Serial monitor at 115200 reconnects after upload and reboot; normal and
  diagnostic builds retain their intended serial behavior.
- Touchscreen, channel/Precision editing, continuous DMX, blackout, presets,
  battery warnings, power startup and sleep/wake retain Stage 2 behavior.
- No NVS erase or partition migration; user settings/presets survive.
- Host suites, integration guards, all four builds and whitespace checks pass.
- Stage 3 transport implementation remains separate until this issue is resolved.

## 9. Build and Test Results

All four requested environments built successfully from unchanged Stage 2 source.
The final verification run also passed all four:

| Environment | Static RAM bytes | Reported flash use bytes | Result |
| --- | ---: | ---: | --- |
| cores3se_unit | 32932 | 604117 | PASS |
| cores3se_base | 32932 | 604065 | PASS |
| cores3se_unit_diagnostics | 35548 | 606361 | PASS |
| cores3se_base_diagnostics | 35548 | 606345 | PASS |

Reported flash use is not the raw `.bin` file length in section 3. Existing
dependency directories have different debug-path lengths (including an M5GFX
`@0.2.28` directory in Base diagnostics); resolved M5GFX is 0.2.28. Do not infer a
firmware regression from a small size difference without comparing actual code.

Resolved packages: espressif32 6.7.0; Arduino 2.0.16
(`framework-arduinoespressif32 3.20016.0`); tool-esptoolpy 1.40501.0 / esptool
4.5.1; Xtensa S3 GCC 8.4.0+2021r2-patch5; M5Unified 0.2.21; M5GFX 0.2.28;
esp_dmx metadata 2.0.2 from v2.02, with existing three-expression S3 patch.

No firmware source changes were made. Existing host Controller/Dispatcher and
integration checks were rerun successfully; `git diff --check` passed. This
documentation-only report remains uncommitted. Hardware acceptance is pending.

## 10. Remaining Unknowns After Hardware Follow-up

- Exact hashes and full logs of the newly tested normal/diagnostic artifacts.
- First broken diagnostic artifact; no normal Stage 2 failure is demonstrated.
- Complete failed command trace, including confirmation that `Changed.` is absent
  in the original 1500000 attempt. Diagnostic stub failure at 115200 is confirmed.
- Results at 460800 and additional diagnostic rate comparisons, if needed.
- Actual USB interrupt state across auto-reset; ROM/no-stub passes are confirmed
  for the tested diagnostic build, without proving HWCDC is the cause.
- USB disconnect/re-enumeration timing and Windows driver/handle behavior.
- Device-resident boot/table/OTA contents and active app partition.
- A verified minimal fix and three repeated automatic upload passes.

The previously proposed Experiment 1 has been completed as reflected above.
Do not repeat the historical plan automatically. Keep the diagnostic issue open
for focused investigation before Wi-Fi resource testing; repeated real firmware
uploads and post-upload serial operation remain the USB acceptance gate.
