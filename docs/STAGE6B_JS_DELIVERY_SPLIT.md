# Stage 6B — Two-bundle JavaScript delivery

Status: implemented, automatically validated and **physically accepted**.
Branch: `codex/stage6b-web-ui`.

The user accepted Core/UI delivery on the real CoreS3SE. At the reported
diagnostic snapshot, four Wi-Fi stations were associated and three WebSocket
sessions were active. WebSocket send failures, queue overflows, Wi-Fi disconnects
and DMX TX errors were all zero. The device remained operational.
This confirms the reported setup; it is not evidence of four active WebSocket
sessions or external electrical timing verification.

## Baseline and scope

The hardware-accepted fixes were committed separately before this migration:

- `3f9230c72ae8ea7453023511eee4264b10c47c12` —
  `fix(network): prevent unlimited sub-millisecond HTTP socket timeout`.
- `03872b99b1ccfb256f010342d8e4e8449c515b80` —
  `fix(web): stabilize blackout presentation and restore channel status`.

The second commit is the Phase A comparison baseline. Its parent is the HTTP
commit, whose parent is `0924c63bddd755a00710b03856fa3addf0a73e90`.
Validation was rerun before committing; all four successful build receipts were
verified against then-current source, assets and component hashes.

This migration changes composition, delivery and startup only. All fourteen
application factories under `web/src/js/` are byte-for-byte identical to Phase A.
The scope gate checks that invariant. Controller, Dispatcher, transport, network
service, HTTP policy, presets/NVS, credentials, touchscreen and partition layout
are unchanged. In particular, the HTTP `remaining < 1000` guard is preserved.

## Module ownership

| Core | UI |
|---|---|
| protocol | fader |
| commands | editor |
| state | presets |
| transport | settings |
| control | status |
| actions | shell |
| preferences | app |

The existing dependency declarations and factory interfaces remain in use.
`scripts/web_ui_bundle.py` validates all names/dependencies, composes each
factory once, and produces `web/src/core.js` and `web/src/ui.js`.
`web/src/loader.js` is an authored Core wrapper, not a fifteenth application
factory. The old composed `web/src/app.js` and `web/generated/app.js.gz` are
removed. Generation removes those exact obsolete outputs; verification rejects
legacy or unexpected gzip inventory.

Core exposes one non-writable, frozen page-lifetime entry point:
`window.__CoreS3SEBoot.mount(createUI)`. Its closure owns seven Core factory
objects, not a second universe or running Controller instance. The UI callback
receives their references, creates the seven UI factories, and returns `app`.
The loader calls `app.start()` once. Re-evaluation of either delivered script
cannot create another mount, WebSocket, listener set or command stream.

## Loading and failure behavior

1. HTML has one deferred Core script with its content-hashed URL and SHA-256
   integrity attribute. CSS retains its existing independent hashed resource.
2. HTML also supplies the UI URL and integrity value as data attributes.
   These do not cause a second speculative script fetch.
3. Only after Core execution does the loader create the UI script element.
   UI loading is eager, not view-dependent, but serial relative to Core.
4. UI invokes the guarded mount. The original application then handles hello,
   complete snapshot, LIVE state and ordinary reconnect as before.

Integrity values cover the decoded JavaScript bytes, while the firmware serves
gzip. HTML supplies both resource URLs and both integrity values, so a UI-only
change does not force a Core hash change simply by embedding a new UI URL.
The existing CSP is sufficient: no inline handler, eval, blob URL, CDN or CSP
relaxation is needed. Integrity establishes resource byte consistency; it is
not administrative authorization or a substitute for authenticated delivery.

Core failure leaves the static loading notice visible and mutation controls
disabled. UI network/integrity failure, execution without a mount, startup
exception or a ten-second UI loading deadline causes visible failure feedback.
The workspace and blackout control are made inert; late UI delivery cannot
revive that failed document. There is one bounded startup timer, cleared after
success/failure. This timer is a client policy, not a measured device latency.
Reload creates a new document and fresh bootstrap lifetime. WebSocket recovery
does not reload scripts or mount another application.

Native Chromium testing found that a deliberately incorrect HTTP 200 script
with immutable caching can remain cached despite integrity rejection. Ordinary
reload can therefore stay safely uninitialized. Clearing/bypassing the browser
cache recovered the test; startup notices describe that option. Missing/partial
uncached responses recovered with ordinary reload. No automatic mutation replay
or cache-busting resource loop is introduced.

## Embedded HTTP and packaging

The manifest now lists exactly four assets: HTML, CSS, Core and UI. URL lookup,
gzip/MIME handling and response policy remain generic and unchanged. The C++
emitter derives the bundle count from the manifest rather than emitting `3`.
Normal PlatformIO builds verify checked-in generated output without regeneration.
The post-build receipts verify all four compressed bodies are retained in flash.
There is no additional legacy full-size bundle in the application image.

Unchanged server limits:

- Six shared sockets; four configured SoftAP stations.
- Twelve admitted HTTP requests per second globally, including WS handshakes.
- 20,480-byte ordinary HTTP transmit budget, 2,048-byte receive budget.
- Three-second absolute HTTP session deadline and existing timeout cleanup.
- 16,384-byte maximum compressed body per asset.

Serial Core/UI loading reduces per-page asset concurrency. It does not guarantee
every simultaneous cold load can succeed. Four cold pages require sixteen asset
requests before handshakes, optional favicon requests or telemetry; this can
exceed the unchanged admission policy. Failures remain visible and safe, with
manual reload. Real WS handshakes add requests not represented by the HTTP-only
admission experiment. Established WS sessions also occupy real firmware sockets.

## Automated evidence

Passed:

- 82 live-state groups, 12 asynchronous Status groups, navigation/focus tests,
  preferences, faders, numeric and +/- controls, presets and blackout regressions.
- Generated-loader tests: sequential request, integrity handoff, exact-once
  mount, duplicate scripts, error/empty/late UI, startup exception and timeout.
- Fifteen pipeline tests, infrastructure tests, network/HTTP/WS integration,
  source-preservation checks, compatibility/ownership patch checks.
- Twenty-two compiled host cases including Controller, Dispatcher, transport,
  actual extracted HTTP/WS I/O, station/SoftAP policies and embedded asset lookup.
- Seven-width Chromium UI checks at 320/375/390/430/600/768/1280 px, preserving
  existing gestures, navigation, compact status and blackout presentation.
- Two byte-identical generations, manifest/SHA checks and `git diff --check`.
- All four PlatformIO environments with the existing dependencies and layout.

`tests/web/test_startup_browser.cjs` uses a temporary native loopback HTTP server
and the actual gzip files; it does not intercept requests, so browser caching
and integrity checks are real. WebSockets/Controller data remain simulated.
It tests cold/warm loading, two-version resource URLs, duplicate evaluation,
background/resume, reconnect, a preset mutation after reconnect, eight Core/UI
missing/wrong/empty/partial response cases, and held-Core request ordering.

Recorded host-browser metrics:

- Four manifest requests on cold startup.
- Zero hashed CSS/JS requests reaching the server on warm reload.
- One active simulated WS per application; no duplicate navigation listeners.
- Four staggered browser instances succeeded with modeled reserved WS slots.
- Peak admitted modeled socket use: 5 of 6 in the final run (4 in the preceding run).
- Four-page cold burst: four admission rejections, safe failure and later manual
  reload recovery. This is a limit test, not a four-client cold-start guarantee.

These are host observations/models, not ESP32 timing, radio, heap or Safari
measurements. In particular, the server model does not run the ESP-IDF HTTP
task or the actual WS TCP streams. No electrical DMX claims follow from it.

## Resources versus Phase A

| Asset | Phase A gzip | Split gzip |
|---|---:|---:|
| HTML | 3,972 | 4,109 |
| CSS | 4,354 | 4,354 |
| Single JS | 16,380 | removed |
| Core JS | — | 8,903 |
| UI JS | — | 8,841 |
| Total JS | 16,380 | 17,744 |

Independent compression plus startup/integrity/failure handling adds 1,364 JS
gzip bytes. Core has 7,481 bytes and UI 7,543 bytes of per-asset headroom.
Total compressed asset growth is 1,501 bytes. This is a headroom and
maintainability change, not a claimed total-download or RAM reduction.

| Environment | Static RAM | Phase A flash | Split flash | Flash delta |
|---|---:|---:|---:|---:|
| cores3se_unit | 87,796 | 1,067,505 | 1,069,081 | +1,576 |
| cores3se_base | 87,796 | 1,067,473 | 1,069,049 | +1,576 |
| cores3se_unit_diagnostics | 90,516 | 1,072,065 | 1,073,641 | +1,576 |
| cores3se_base_diagnostics | 90,516 | 1,072,033 | 1,073,605 | +1,572 |

Static RAM delta is zero in all builds. Runtime Wi-Fi heap/PSRAM, real cold-load
latency and three/four-device stress still require hardware observation.

## Reproduction

Use the existing pinned Python 3.11.7 / zlib 1.2.13 runtime:

```text
python -B scripts/web_asset_pipeline.py --generate
python -B tests/web/check_pipeline.py
node tests/web/run_tests.mjs
python -B tests/network/check_websocket_integration.py
python -B tests/infrastructure/test_infrastructure.py
python -B tests/network/run_websocket_tests.py --compiler path/to/zig.exe --zig
pio run -e cores3se_unit
pio run -e cores3se_base
pio run -e cores3se_unit_diagnostics
pio run -e cores3se_base_diagnostics
git diff --check
```

Optional browser suites use the already installed Playwright/Chromium selected
by `DMX_PLAYWRIGHT_MODULE` and `DMX_CHROME_PATH`:

```text
node tests/web/test_header_browser.cjs --output dist/NEW-responsive-check
node tests/web/test_startup_browser.cjs
```

The latter binds only 127.0.0.1 and closes its server/browser when finished.
It never contacts the device. No product runtime library is added.

## Physical acceptance checklist

The user has accepted the migration with the results above. The checklist is
retained for future firmware/browser changes; unreported stress and electrical
measurements are not implied by this acceptance.

- iPhone Safari and Home Screen: cold/warm/repeated reload, background/resume,
  all four views, correct header/nav/scrolling, faders, +/- and numeric entry.
- Core/UI load failures: honest loading/failure state, no mutation availability;
  reload/cache-clear recovery as applicable.
- One/two/three clients; four stations where feasible. Keep existing sessions
  live while another device cold-loads. Record HTTP rejections, current WS
  clients, queue/send counters, heap/largest blocks and DMX error counters.
- Simultaneous cold refresh: distinguish admission rejection from WS failure;
  never interpret modeled host success as proof of real radio capacity.
- Preset Save/Load/Delete, overwrite/delete confirmation, double-tap, Loaded/
  Modified sync, blackout ON/OFF and restore snapshot across clients.
- Verify stable compact channel status, stable main blackout button during load,
  and unchanged physical touchscreen/DMX operation and preset persistence.

The historical synchronous WS send-stall limitation remains unresolved and is
outside this migration. Experimental DMX Base output glitches remain a separate
hardware concern. No Stage 6B.5 Settings features were implemented.

## Deferred UI issue — do not fix in this migration

LOAD, SAVE and DELETE briefly change appearance during preset load. The user
explicitly deferred investigation until after two-bundle physical acceptance.
Possible causes include disabled/pending presentation, redundant rendering or
transitions; no new diagnosis is asserted here. The existing admission,
confirmation and authorization behavior is preserved exactly.

## Rollback

The complete pre-split tested baseline is
`03872b99b1ccfb256f010342d8e4e8449c515b80`. A separate checkout of that commit
builds the previous single-bundle implementation including all three fixes.
Do not reset this workspace or disturb historical files/stashes to inspect it.
No upload, push, merge, tag, release or NVS operation was performed by this task.

## Phase B file inventory

- `docs/STAGE6B_JS_DELIVERY_SPLIT.md`
- `scripts/web_asset_pipeline.py`
- `scripts/web_ui_bundle.py`
- `tests/infrastructure/test_asset_api.cpp`
- `tests/infrastructure/test_infrastructure.py`
- `tests/web/README.md`
- `tests/web/check_pipeline.py`
- `tests/web/check_scope.py`
- `tests/web/fixture_server.py`
- `tests/web/preview.py`
- `tests/web/run_tests.mjs`
- `tests/web/test_header_browser.cjs`
- `tests/web/test_loader.mjs`
- `tests/web/test_startup_browser.cjs`
- `web/generated/app.js.gz` (removed)
- `web/generated/core.js.gz`
- `web/generated/index.html.gz`
- `web/generated/manifest.json`
- `web/generated/ui.js.gz`
- `web/src/app.js` (removed)
- `web/src/core.js`
- `web/src/index.html`
- `web/src/loader.js`
- `web/src/ui.js`
