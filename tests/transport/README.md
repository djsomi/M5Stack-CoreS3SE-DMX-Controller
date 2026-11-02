# Stage 3 Transport Tests

Build `cores3se_unit` first. Its installed esp_dmx source must have both reviewed
patches applied. The suite compiles that actual ISR header, the repository's exact
installed task-side API templates, the canonical transition core, actual
EspDmxTransportDriver adapter, DmxTransport, Controller and CommandDispatcher.
Only hardware registers, FIFO/shifter/time, RTOS primitives and persistence are
faked. No device connection is made.

From the repository root, using the already installed Windows host compiler:

```powershell
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -DDMX_TX_HOST_TEST -Itests/transport/fake -Ipatches/esp_dmx_v202 -I.pio/libdeps/cores3se_unit/esp_dmx/src -Isrc -Iinclude src/dmx_controller.cpp src/command_dispatcher.cpp src/dmx_transport.cpp tests/transport/test_transport.cpp -o dist/stage3c-transport-tests.exe
.\dist\stage3c-transport-tests.exe
python tests/dispatcher/check_integration.py
```

Alternatively use a system `g++`/`clang++` with the same flags and a suitable
ignored temporary executable path. The host compiler is not a firmware dependency.
The one vendor aggregate-initialization warning suppressed in the test is scoped
to the existing vendor ISR header, not the application or transition core.

Coverage includes normal/full length, snapshot immutability, 385 unqueued-tail
underrun cases, stale final-refill done, completion-arming event preservation,
FIFO versus shifter drain, zero-byte FIFO write, active timeout, failed retirement,
safe recovery, different-task/ISR API rejection, stale epoch flags, latest/final
value, ongoing unchanged output, presets during TX, ordered blackout ON/OFF,
immutable OFF restore packet, queue-full pre-mutation rejection/FIFO deferral,
ordered barrier retry, retry exhaustion, Receiver isolation and the legacy
shutdown sequence's lack of an actual zero-frame start.

For deterministic patch-state tests, provide a pristine v2.02 library directory.
Do not unpatch production `.pio/libdeps` to obtain one. During this task the
isolated exact Stage 2 archive provides it:

```powershell
python tests/transport/test_patch.py dist/stage3c-evaluation-20261024/stage2-source/.pio/libdeps/cores3se_unit/esp_dmx
```

On a new development machine an independent upstream checkout is sufficient:

```powershell
git clone --depth 1 --branch v2.02 https://github.com/someweisguy/esp_dmx.git dist/esp-dmx-v202-pristine
python tests/transport/test_patch.py dist/esp-dmx-v202-pristine
```

The script modifies only temporary copies below ignored `dist/`. It verifies
fresh and repeated application, all pre/post hashes, unchanged S3 getter file,
and rejection without writes of partial, missing, unknown, wrong-version and
modified-template states. Source hashes normalize CRLF to LF explicitly.

These tests validate software transitions against a deliberately adverse UART
model. They do not establish actual MMIO event latency/order, electrical DMX
integrity, break/MAB duration, analog RS-485 quality, fixture acceptance or the
absence of experimental Base glitches. Follow the separate hardware checklist
in `docs/TESTING.md` before committing this candidate.

## Sender CLASH policy regression

Permanent tests exercise CLASH during the initial FIFO write, repeated raw and
unexpected delivered indications during refill/drain, and READ-mode isolation.
They check 513-byte completion, no false success or snapshot mutation, final-state
delivery and ordered blackout. Genuine underruns/premature DONE/timeout remain
errors; three actual failures still latch and a successful retry resets the count.
No temporary event-history or USB diagnostics are part of this suite/hotfix.

An optional second argument to `test_patch.py` supplies a previous fully patched
library to verify rejection without writes of the old fatal-CLASH revision.
See `docs/TESTING.md` for scope and validation.
