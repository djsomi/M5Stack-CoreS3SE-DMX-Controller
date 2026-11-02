# Preset archive regressions

Portable models and actual Controller/Dispatcher/Bridge tests:

```powershell
python -B tests/preset_archive/run_tests.py --output dist/preset-tests
```

## Actual ESP-IDF NVS

The native test compiles unmodified `espressif/esp-idf` **v4.4.7** NVS sources,
verifying each Git blob SHA against `native/sdk-source-manifest.json`.
Provide an SDK source checkout (`git clone --branch v4.4.7 --depth 1
https://github.com/espressif/esp-idf.git <sdk-path>`, no submodules needed).
The manifest lists the 35 required files. No download happens during this test.

```powershell
python -B tests/preset_archive/run_native_nvs.py --sdk-source <sdk-path> --output dist/native-nvs
```

Uses the existing host Zig toolchain at `dist/host-toolchain/ziglang/zig.exe`.
`--project-source <tree>` selects a source snapshot for comparison. The current
suite tests current-format factory installations; the historical v0.2 coexistence
regression and its expected exit-42 result remain in earlier Git commits.

Only the NOR-flash device (20 KiB RAM), SDK platform headers/CRC, no-op Linux
RTOS lock and logging wrappers are host adapters. Page allocation,
blob storage, garbage collection, recovery, atomic entries and NVS errors use
actual SDK code, not a map or capacity mock. Real physical Save/Delete handlers
are extracted from the selected source tree; Controller, Dispatcher, Bridge and
protocol are compiled unchanged. No device, serial port or persistent flash is
opened. Artificial fixture credentials/settings contain no real user data.

Covers first empty installation/Save, all eight slots, current-format import, fresh-boot mutations, repeated cycles,
export/cancel/invalid files/reimport, two-client occupancy, blackout/live isolation,
before/after cuts and errors at every flash boundary, half-program/half-erase
interruptions, full NVS and fail-closed invalid selector/CRC. Reboot reinitializes
the actual SDK against retained RAM flash. It checks unrelated namespaces.
Obsolete keys are probed only to verify they are ignored and untouched, not migrated.
Save/Delete additionally receive interruption/error injection at every flash write.
Electrical flash durability, real brownouts and device acceptance still require
hardware testing. Test output must not be claimed as physical validation.
