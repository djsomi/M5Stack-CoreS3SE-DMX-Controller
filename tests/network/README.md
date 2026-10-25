# Stage 5 HTTP verification

Final production builds default `DMX_STAGE5A_TRACE` to zero, including both
normal and Stage 0 diagnostic environments. `/status` resource/transport metrics
remain enabled. The handwritten bounded startup trace/core tests are retained
as an explicitly opt-in investigation tool; no trace captures or generated host
diagnostics belong in Git. Private monitor logs and Python caches are ignored.

Check the default with the installed host compiler (no ESP headers or device):

```powershell
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -DDMX_HARDWARE_UNIT -Iinclude src/wifi_startup_trace.cpp tests/network/test_trace_default.cpp -o dist/trace-default-tests.exe
.\dist\trace-default-tests.exe
```

Host tests compile actual `src/http_policy.cpp` and generated asset table:

```powershell
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude src/http_policy.cpp .pio/build/cores3se_unit/generated/web_assets.cpp tests/network/test_http.cpp -o dist/http-tests.exe
.\dist\http-tests.exe
python -B tests/network/check_integration.py
python -B tests/dispatcher/check_integration.py
```

They test routing, exact flash-asset metadata, encoding/cache/Host/Origin policy,
fragmented transfer budgets, oversize/overflow, absolute slow/idle deadlines and
bounded parallel request simulation. They do not execute the ESP Wi-Fi stack,
NVS, HTTP parser, sockets or task scheduler on hardware.

After separately authorized normal Unit upload and manual AP connection:

```powershell
python -B tests/network/probe_device.py --host 192.168.4.1 --output dist/NEW-http-probe.jsonl
```

Use the address confirmed in Settings; the firmware configures and verifies
192.168.4.1/24. The probe opens HTTP sockets only when explicitly run. It refuses
log overwrites and sends GET requests only. No credentials are logged. See the
implementation report for the complete browser/radio/DMX/memory matrix.

Stage 5C adds the actual credential/station policy and extracted native DHCP-call
contract tests (build the firmware first for the installed-library guards):

```powershell
python -B tests/network/check_softap_integration.py --dhcp-header dist/stage5c-host/dhcp_under_test.h
.\dist\host-toolchain\ziglang\zig.exe c++ -std=c++11 -Wall -Wextra -Werror -Isrc -Iinclude -Idist/stage5c-host src/softap_policy.cpp tests/network/test_softap_policy.cpp -o dist/stage5c-host/softap-tests.exe
.\dist\stage5c-host\softap-tests.exe
```

The DHCP tests verify real call arguments and propagate simulated SDK errors;
they do not deliver real leases or establish iOS cellular routing. Follow
`docs/STAGE5C_SOFTAP_USABILITY.md` for fresh/renewed leases, local reset and
multi-station hardware acceptance. The manual probe now expects Stage 5C status
fields and remains a read-only, explicitly invoked HTTP check.
