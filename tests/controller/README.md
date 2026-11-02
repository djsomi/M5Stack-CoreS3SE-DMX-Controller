# Controller Host Tests

These tests compile the actual firmware Controller, not a translated model.
They use in-memory preset storage and a selected-channel observer. No Arduino,
display, NVS device, PlatformIO native environment, or networking is required.
They do not validate the ESP32 NVS adapter, real display/touch behavior, or DMX
electrical output; those require the hardware checks in `docs/TESTING.md`.

From the repository root with any C++11 compiler, for example:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude src/dmx_controller.cpp tests/controller/test_controller.cpp -o controller-tests
./controller-tests
```

Use a temporary output path outside the repository or under ignored `dist/`.

On the development Windows machine, the test compiler is isolated in ignored
`dist/host-toolchain/`; it is not a firmware dependency. Reproduce with:

```powershell
python -m pip install --target dist/host-toolchain ziglang==0.13.0
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude src/dmx_controller.cpp tests/controller/test_controller.cpp -o dist/controller-tests.exe
.\dist\controller-tests.exe
```

Coverage includes channels 1 and 512 and invalid indices, values 0..255 and
invalid values, clamped adjustments, zero start code, monotonic notifications,
selected-channel synchronization, exact blackout entry restoration, edits and
preset loads during blackout, preset save/load/copy/clear, persisted logical
occupancy, invalid slots, and storage inspect/read/save/erase failures.
