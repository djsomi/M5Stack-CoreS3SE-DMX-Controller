# Stage 0 Hardware Baseline Diagnostics

Temporary diagnostics only, based on v0.2.0 / `32c0571`. No Wi-Fi, controller
extraction, partition changes, or DMX scheduling changes are included.

## Enable and Build

The normal environments `cores3se_unit` and `cores3se_base` leave
`DMX_HW_DIAGNOSTICS` at its default value of zero. The two additional environments
inherit their respective hardware profile and enable `-DDMX_HW_DIAGNOSTICS=1`:

```powershell
pio run -e cores3se_unit_diagnostics
pio run -e cores3se_base_diagnostics
```

The ordinary release build commands remain:

```powershell
pio run -e cores3se_unit
pio run -e cores3se_base
```

## Flash Unit and Collect the Log

Use the tested CoreS3SE + Battery Bottom + Unit DMX configuration. Close any
existing Serial Monitor before flashing. Use `pio device list` to check the
current port; the examples use the previously known COM8.

```powershell
pio run -e cores3se_unit_diagnostics -t upload --upload-port COM8
pio device monitor -e cores3se_unit_diagnostics --port COM8 --baud 115200 --filter log2file
```

If `pio` is not on PATH, replace it in each command with:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
```

The monitor's `log2file` filter writes `logs/device-monitor-YYMMDD-HHMMSS.log`
relative to the current directory and prints its full path. This is a local
capture file, not a release asset; keep it out of commits.
After the monitor is open, reset the device with the reset button. USB may
re-enumerate; reconnect the monitor on the actual port if necessary. The firmware
does not wait for Serial. Boot snapshots remain in the fixed RAM queue until the
host is connected, so a short reconnection delay does not discard them. No
firmware reset or reconnect is needed solely to initiate printing.

Perform this sequence slowly, allowing about one second between transitions:

1. Boot and let the startup screen enter Sender automatically.
2. Open Settings, then BACK to Main.
3. Select a channel, open EDIT, optionally open the numeric keypad, then return
   to Main.
4. Open Presets. Use a deliberately chosen spare slot: hold SAVE, change a
   channel, then LOAD that slot. Saving overwrites that slot's previous contents;
   diagnostics do not create or alter presets automatically.
5. Return to Main. Repeat Settings/EDIT navigation approximately ten times to
   compare retained-canvas memory and check navigation. Wait for pending output.
6. Exit the monitor using Ctrl+C. Share the `[S0]` lines together with profile,
   power source, hardware combination, and any fixture or navigation symptoms.

Repeat after a fresh boot with Precision opened before Settings, to compare
allocation order. Repeat with Base diagnostic firmware and external PSU when
available; keep its known experimental glitch status distinct from the Unit
baseline. A bare USB-only Core can establish its memory/boot baseline but will
not power Unit DMX under the existing battery-presence power policy.

To return to normal firmware, close the monitor and upload:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

Do not flash the published merged FULL image or erase flash just to switch
diagnostics on/off; these normal application upload commands preserve the
existing storage layout.

## Log Fields and Events

The first `[S0]` line identifies the source baseline, diagnostic build date/time,
M5-detected board name/numeric type, Unit/Base profile, IDF version, baud, flash
sizes and JEDEC ID. Sizes are bytes. `cfg_rc`, `physical_rc`, and `id_rc` equal
zero on success; a size of zero with a failed API result is unknown, not a
measurement of zero-capacity hardware.

The installed IDF supports:

- `esp_flash_get_size()`: configured/runtime-visible flash size, described by
  this SDK as the size from the binary image header. It is not the generic
  SDK's hard-coded default or a physical-capacity measurement.
- `esp_flash_get_physical_size()`: physical capacity detected from the flash ID.
  This is chip-ID-based detection, not an exhaustive destructive capacity test.
- `esp_flash_read_id()`: the JEDEC ID, allowing comparison with esptool flash_id.

Each subsequent line reports a captured transition, not the state at the later
moment its deferred text reaches the monitor:

| Field | Meaning |
|---|---|
| `n`, `t_ms`, `event` | Log sequence, snapshot uptime, transition name. |
| `screen`, `owner` | Logical current page and `active_fullscreen_page` owner. `main_value` is the main slider/EDIT view; owner `none` means Main/startup rather than a full-screen page. |
| `canvas`, `ptr` | Shared canvas ready state and buffer pointer. The retained buffer is expected to remain allocated on Main; this is not a leak. |
| `psram_total`, `psram_free` | Initialized PSRAM exposed by Arduino. Zero total means PSRAM was not initialized/found by the runtime. |
| `internal_free` | `heap_caps_get_free_size(INTERNAL | 8BIT)`. |
| `minimum_free` | Arduino `ESP.getMinFreeHeap()`, which uses INTERNAL capability in the installed core. This is a since-boot low-water mark, not a sum including PSRAM. |
| `internal_min` | Since-boot low-water mark requiring both INTERNAL and 8BIT; compare with current internal free space. |
| `largest_internal8` | Largest currently allocatable block requiring both INTERNAL and 8BIT. |
| `largest_dma` | Largest currently allocatable DMA-capable block. Capability classes overlap; do not add these sizes together. |
| `preset`, `rc`, `blackout` | User-facing P1-P8 slot for preset events, operation result (zero succeeds), and current blackout flag. |
| `dropped` | Cumulative snapshots not queued because the fixed 32-entry queue was full. Repeat a slower capture if nonzero. |

Events: `after_init`, `sender_enter`, `settings_first`, `precision_first`,
`keypad_first`, `return_main_settings`, `return_main_precision`,
`return_main_presets`, `preset_save`, `preset_load`, and `canvas_alloc_failed`.
First-screen records are taken after a successful canvas push, including when
the existing audio protection defers drawing. Allocation failure is reported
once and does not mark the page as successfully rendered. Preset failure paths
are also recorded, so a failed load/save cannot be mistaken for success.

Illustrative structure only; no hardware values have been collected yet:

```text
[S0] build=... board=CoreS3SE(...) profile=unit ... configured_flash=<bytes> cfg_rc=0 physical_flash=<bytes> physical_rc=0 ...
[S0] n=1 t_ms=... event=after_init screen=startup owner=none canvas=none ...
[S0] n=2 t_ms=... event=sender_enter screen=main owner=none canvas=none ...
[S0] n=3 t_ms=... event=settings_first screen=settings owner=settings canvas=ready ptr=<address> ...
```

## Bounded Logging and Measurement Limits

Transitions copy scalar measurements into a fixed static queue. There is no
Serial formatting/write inside the transition hooks, no allocation probe, no
diagnostic task, no waiting for a host, no `flush()`, no delays, and no changes to
the UART driver or packet scheduling. The main loop drains at most 40 bytes per
service call, spaced at least 4 ms apart, only if USB is connected and TX space
is available. The installed HWCDC transmit timeout is set to zero. No output is
generated by the logger when it has no pending banner/records. Existing canvas
push/preset Serial chatter is suppressed only in diagnostics-enabled builds.

Diagnostics have finite CPU and RAM overhead: heap queries take time and the
queue/line buffer reserve several KiB. The banner's `diag_buffers_bytes` reports
the snapshot array plus line buffer, excluding the small remaining counters and
flags. Compare enabled/disabled build sizes as well. These logs are not a
zero-overhead timing benchmark. Existing third-party SDK boot/debug messages may
appear separately; identify shareable records by `[S0]`.

Firmware can report memory, allocation success, logical page state, and flash
metadata. These snapshots do not measure physical DMX break, MAB, baud accuracy,
inter-frame gaps, channel data on the wire, or fixture receipt. No packet
coherence or timing guarantee follows from a successful software operation or
an allocation log.

External baseline measurements still needed:

- Use an oscilloscope or sufficiently fast logic analyzer through a suitable
  RS-485 receiver/differential probe to inspect DMX output safely. Do not connect
  a 3.3 V analyzer directly to differential DMX A/B lines. A UART-side capture
  alone does not establish RS-485 output electrical behavior.
- Decode baud, framing, break, MAB, start code, 512 channel slots, frame interval
  distribution and maximum gaps. Record during idle, slider/hold edits, keypad
  entry, scrolling, screen/canvas transitions, NVS preset save/load, sound,
  blackout transitions and sleep/wake. Compare diagnostics-disabled firmware
  for actual timing baselines.
- Check stable known preset patterns over complete captured frames and at
  transitions. A mid-frame mix of old/new channel values requires decoded wire
  evidence. Absence of an observed mix is limited by the capture duration and
  tested operations; it is not a proof of atomicity.
- Test both Unit/Battery Bottom and Base/external PSU with representative load,
  termination/cabling and fixture behavior. Capture blackout's exact zero and
  restore-entry behavior separately from memory logs.

Hardware data awaited before Stage 1: actual board/flash/PSRAM identification;
free/minimum/largest-block measurements at all requested transitions; shared
canvas pointer reuse and navigation stability; preset success and retained
values; disabled-build DMX timing/coherence captures; and power/speaker/sleep
regression observations. Wi-Fi memory overhead is intentionally not measured in
Stage 0 because the radio remains unimplemented/off.
