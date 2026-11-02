# Web UI and embedded asset tests

The production UI uses four deterministic gzip resources (HTML, CSS, Core JS and
UI JS) plus two byte-identical PNG icons. Core loads UI after its execution, with content hashes/SRI and strict
budgets. All active Editor, Preset, Backup, Settings and Maintenance behavior is
exercised by the current frontend tests; this is no longer a non-live preview.

From the repository root:

```powershell
python -B scripts/web_asset_pipeline.py
python -B tests/web/check_pipeline.py
node tests/web/run_tests.mjs
python -B tests/network/check_integration.py
python -B tests/network/check_websocket_integration.py
python -B tests/infrastructure/test_infrastructure.py
```

Use `python -B scripts/web_asset_pipeline.py --generate` only after intentional
frontend source changes. The generator uses standard-library deterministic gzip;
no node/npm dependency is needed for firmware asset generation. Match the pinned
Python/zlib environment documented by the pipeline tests if byte identity differs.
Generated assets must match the composer, manifest, hashes and 16 KiB per-asset
limit. PNG icons have a separate 32 KiB cap; their fixed routes use revalidation
and work without gzip negotiation. Do not modify generated files independently.

## Optional browser suites

Each `test_*_browser.cjs` can run with an existing Playwright installation and
Chromium. `DMX_PLAYWRIGHT_MODULE` selects its module and `DMX_CHROME_PATH` selects
the browser executable. Example:

```powershell
node tests/web/test_archives_browser.cjs
node tests/web/test_startup_browser.cjs
node tests/web/test_header_browser.cjs
node tests/web/test_device_settings_browser.cjs
node tests/web/test_wifi_browser.cjs
```

Other browser files cover blackout/preset button stability, Export, maintenance,
responsive/card layouts, confirmation styling and Wi-Fi polish. Browser responses,
WebSocket and Controller state are synthetic. Native loopback startup tests cover
gzip/SRI/cache, failed assets, duplicate evaluation, lifecycle recovery, modeled
socket/rate limits and no unsolicited mutation (read-only wifi.get is permitted).
They are not radio/ESP32 electrical measurements or proof of Safari toolbar fixes.

## Regression coverage

- Authoritative snapshots/revisions, stale detection, bounded health probes,
  reconnect/resume and no mutation replay or duplicate connections.
- Faders/final values, +/- and numeric input, bank navigation, preset selection/
  double-tap, overwrite/delete confirmation, blackout and multi-client state.
- Backup JSON validation, full collection replacement, foreground cancellation,
  progress UI, timestamped filenames, conflict/admission and persistence errors.
- Settings discrete mapping/preview/commit/revision ownership, maintenance physical
  countdown/cancel, Wi-Fi physical approval/recovery, and browser-only preferences.
- Stable svh shell/navigation, real content-scroll reset on different tabs,
  same-tab preservation, focus/accessibility, touch policies and no layout shifts.

See [current acceptance and external limits](../../docs/TESTING.md). Repeat the
actual iPhone Safari/Home Screen and real-device DMX checks when relevant;
headless Chromium does not establish their physical behavior.
