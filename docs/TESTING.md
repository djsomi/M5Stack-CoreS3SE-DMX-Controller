# Firmware regression and hardware testing

Normal release profiles are `cores3se_unit` and `cores3se_base`. Build from the
matching committed source; no device is flashed by these commands:

```powershell
pio run -e cores3se_unit -e cores3se_base
python -B tests/preset_archive/run_tests.py --output dist/preset-tests
python -B tests/preset_archive/run_native_nvs.py --sdk-source <esp-idf-v4.4.7-source> --output dist/native-nvs
python -B tests/network/run_websocket_tests.py
python -B tests/wifi/run_tests.py
python -B tests/wifi_power/run_tests.py --output dist/wifi-power
python -B scripts/web_asset_pipeline.py
python -B tests/web/check_pipeline.py
node tests/web/run_tests.mjs
python -B tests/infrastructure/test_infrastructure.py
git diff --check
```

See the READMEs under tests/ for compiler setup, Controller/Dispatcher/transport,
frontend/browser, settings/maintenance, navigation and diagnostic-isolation suites.
The archive runners use the existing host Zig compiler under ignored
`dist/host-toolchain/ziglang/zig.exe`; the real-NVS runner verifies 35 exact SDK
files before compiling ESP-IDF 4.4.7 NVS against RAM-backed flash. It does not
open hardware or persistent device storage. Synthetic fixture credentials are
not real device secrets. Logs/builds belong under ignored dist/ or logs/.

Source scope guards and archival packaging tests need the preserved Git history;
a full source clone is preferable to a shallow/archive-only checkout for those
tests. Historical packaging document fixtures are reconstructed from their real
old commit, not retained in the current public documentation. Those old frozen-
source tools are not a replacement for the current factory build procedure.

## Diagnostics and isolation

`cores3se_unit_diagnostics` / `cores3se_base_diagnostics` enable existing bounded
hardware/memory logs. See [diagnostics](STAGE0_DIAGNOSTICS.md). Production keeps
hardware diagnostics off. Physical-fader/acquisition tracing, touch snapshots and
THDIFF experimental writes require separate explicit investigation opt-ins;
standard diagnostics cannot enable APPLY/RESTORE indirectly. THDIFF experiments
remain suspended. No touch-register writes or global threshold changes are part
of this release.

## Physical regression checklist

- Choose the correct Unit/Battery Bottom or Base/external-PSU profile; verify its
  boot label, board startup, power and actual version/build identity.
- Test channel-grid scrolling, main captured fader, Precision +/-/STEP, keypad,
  screen sleep/wake, sound and battery-warning priority/volume restoration.
- During positive and negative kinetic scrolling, enter Settings, Presets and
  Precision. Check complete drawing, canvas reuse, touch ownership and preserved
  Main position with stopped momentum on return.
- Save/Load/Copy/Delete all preset slots; JSON Export/Import, cancel/conflict/error
  handling, reboot retention, two-client occupancy and unchanged live output.
- Test blackout ON, edits/preset loads while zero, then exact entry restoration.
- Test local Wi-Fi OFF/ON, Settings preview/save and peer sync, credential physical
  approval/cancel/recovery, Restart and original Power Off/CANCEL including USB.
- Web control, rapid faders, final values, invalid commands, multi-client state,
  idle health checks, disconnect/reconnect and no mutation replay.
- Check iPhone Safari/Home Screen, desktop/tablet layout, tab scroll reset, bank
  taps, preset double taps, confirmation/pending state and foreground Backup.
- A clean factory install is destructive. A tested current-format app-only update
  must retain presets/settings. Never use a full factory image for retention.

User acceptance covers Unit normal operation, clean erase/install, Backup and
Save/Delete/reboot, plus a no-erase current-format v0.3 app update. Base remains
experimental; build success is not Base hardware acceptance.

## External measurements and known limits

Software logs/tests cannot establish wire integrity, electrical levels, baud,
break/MAB length, full packet length, frame coherence/gaps or zero-frame delivery
on shutdown. Measure with suitable RS-485 capture/probes and representative
fixtures/termination, including NVS activity, blackout and sleep/wake. Never
connect a 3.3 V logic-analyzer input directly to differential DMX A/B lines.

Slow physical coordinate stalls, synchronous TCP send stalls, fixed-scale zoom,
Base glitches and interrupted-install/power-loss recovery limits remain in
[KNOWN_ISSUES](../KNOWN_ISSUES.md). Do not claim those were fixed by host tests.
