# Dispatcher Host Tests

Run from the repository root with a C++11 compiler:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude src/dmx_controller.cpp src/command_dispatcher.cpp tests/dispatcher/test_dispatcher.cpp -o /tmp/dispatcher-tests
/tmp/dispatcher-tests
python tests/dispatcher/check_integration.py
```

On the current Windows development machine, the isolated compiler is under
ignored `dist/host-toolchain/` (see the Controller test README for installation):

```powershell
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude src/dmx_controller.cpp src/command_dispatcher.cpp tests/dispatcher/test_dispatcher.cpp -o dist/dispatcher-tests.exe
.\dist\dispatcher-tests.exe
python tests/dispatcher/check_integration.py
```

The tests execute the actual Controller and Dispatcher with fake persistence and
publication callbacks. They cover admission/application validation, FIFO payload
ownership and request/ticket correlation, queue full/wrap/recovery, processing
budgets, local execution behind a full queue, final slider value without
coalescing, current-state relative edits, bounded atomic patches, notifications,
no-op revisions, blackout barriers/restore, presets and logical occupancy,
storage/transport errors, mode transitions and reentrancy rejection.

Page-independent host turns are supplemented by source-placement guards proving
that the actual application services the dispatcher before page/wake early
returns. Source guards also compare protected transport, hardware configuration,
NVS adapter, diagnostics, battery and critical UI sections against the explicit
tested Stage 1 commit `0e43640e57519e47177b08fae9c0b86ff8b0e9d9`.

These are not tests of actual touch hardware, visual rendering, real NVS, task
concurrency, DMX electrical timing, or packet coherence. No cross-task safety is
implemented or claimed. Perform the hardware checklist in `docs/STAGE2_DISPATCHER.md`.
