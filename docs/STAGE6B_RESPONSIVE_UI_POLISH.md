# Stage 6B — Consolidated Responsive UI Polish

2026-10-29. Branch: `codex/stage6b-web-ui`.
Status: responsive layout and final blackout color follow-up physically accepted
on iPhone; approved for Stage 6B.5A local baseline closure.

## Baseline and scope

Started from the current uncommitted, physically tested Stage 6B.5A Settings
implementation. The unchanged HEAD is `ce1b9f63431874ae05588c1275644878ee29a973`;
HEAD alone is not the comparison baseline because Stage 6B.5A is uncommitted.
The actual pre-change source/assets and file/stash fingerprints are preserved in
ignored `dist/responsive-ui-20261029/before/` and `before-state.json`. Browser
comparisons below load those exact old gzip assets, not a reconstructed mock UI.

The original responsive layout changes were HTML/CSS only. The blackout color
follow-up below adds a state-derived class in the existing app renderer. All other JavaScript factories,
Controller/Dispatcher/transport, Settings preview/commit/cancel ownership, NVS,
HTTP limits, protocol, timing, partition layout and Wi-Fi behavior remain intact.
No pull/reset/stash/discard, staging, commit, upload, push, merge, tag, release or
NVS operation was performed. Existing historical/unrelated files and stash remain.

## Settings card structure

Added one `settings-device-grid` wrapper around the existing independent Device
Settings card. Browser Preferences remains the same separate sibling card below.
There is exactly one device card in production: no Maintenance placeholder,
button, hidden card or reserved empty desktop column.

Below 680 px, the grid is one full-width column. The old 92%/360 px restriction
is removed: each Settings input now fills its card's inner width. Padding is
symmetric (18 px padding plus 1 px border = 19 px from each outer card edge).
All three tracks share the same width; labels stay inside the card.

At the existing desktop/tablet breakpoint, auto-fit tracks use
`minmax(min(100%, 320px), 400px)`. The card itself is compact, at most 400 px,
rather than placing a narrow control column inside an oversized card. Empty
tracks collapse. This current grid was tested with a temporary browser-only
second child, then removed; no second card is embedded. Browser Preferences
follows the grid below the tallest populated row, using the normal content width.

Future requirement, explicitly deferred: when Device Maintenance is implemented,
Device Settings and Device Maintenance must occupy equal 50/50 responsive columns
on desktop, stacking vertically at full width on mobile. That future requirement
supersedes the earlier suggestion of two 400 px tracks. The current grid remains
unchanged; no empty Maintenance placeholder or reserved column is rendered.

The approved native 44 × 18 px rectangular thumb, 44 px touch height, track style,
five stops, timeout order (1/2/3/4/Always On), quiet values-only status and real-error
feedback remain. Native measurements found that the 1 px track borders contribute
to thumb endpoints. Tick inset is corrected from 22 to 23 px (half of the unchanged
44 px handle plus the border); usable Settings travel is input width minus 46 px.
All five actual native thumb positions on all three inputs coincide with the
visible tick centers within 0.1 CSS px at the six functional-test widths. Only
the input rectangle captures slider gestures; labels/empty surfaces retain normal
pan scrolling. No new event handlers or touch suppression were added.

## Editor shared sizing and mapping

The existing fader is a native vertical range (`writing-mode: vertical-lr`,
`direction: rtl`, 0–255, step 1). `fader.js` forwards the native value to existing
control pacing; it does not compute pointer coordinates. No JS change is needed.

The original base profiles are retained: phone/small tablet clamp 160–300 px,
large tablet/desktop clamp 200–330 px, and the existing short-landscape 150 px
profile. They now set `--fader-base-travel`. One shared sizing expression applies
to every profile:

```css
--fader-travel: calc((var(--fader-base-travel) - 20px) * .85 + 20px);
```

The measured native nonmoving extent is 20 px: the unchanged 18 px handle and
the two unchanged 1 px track borders. This scales **selectable thumb-center travel
by 85%**, instead of reducing the handle or introducing custom value scaling.
Total CSS range height therefore decreases by roughly 13–14%; usable travel by
15%. Keeping the existing handle/borders accounts for the difference.

The rail and numeric-entry wrapper already use `--fader-travel + 104px`, including
both unchanged 44 px +/- buttons and their gaps. They shrink naturally with the
input, so no oversized wrapper cancels the benefit. Fader columns remain ordinary
visible-overflow flex columns, with no independent scrolling. Handle 44 × 18 px,
54 px input touch width, 6 px track width, labels, percentage display and 4/6/8
responsive channel counts remain unchanged. Native 255 is at the top and 0 at
the bottom, inside the borders without clipping or control overlap.

## Measured Editor geometry

CSS pixels, measured from real rendered before/after assets in Chromium. “Range”
is the input element's height; “usable” is actual native thumb-center distance.
“Card” is `.editor-panel`; “whole view” also includes heading and blackout summary,
not the application header/navigation. Viewport heights are explicit to make the
svh-dependent results reproducible. Usable reduction is 15.000% in every case.

| Viewport | Range before → after | Usable before → after | Card before → after | Whole view before → after |
|---|---:|---:|---:|---:|
| 320 × 900 | 300 → 258 | 280 → 238 | 754 → 712 | 880 → 838 |
| 375 × 812 | 300 → 258 | 280 → 238 | 754 → 712 | 880 → 838 |
| 390 × 844 | 300 → 258 | 280 → 238 | 754 → 712 | 880 → 838 |
| 430 × 932 | 300 → 258 | 280 → 238 | 754 → 712 | 880 → 838 |
| 600 × 800 | 300 → 258 | 280 → 238 | 685 → 643 | 811 → 769 |
| 768 × 900 | 330 → 283.5 | 310 → 263.5 | 727 → 680.5 | 853 → 806.5 |
| 1280 × 900 | 330 → 283.5 | 310 → 263.5 | 727 → 680.5 | 853 → 806.5 |
| 390 × 600 | 160 → 139 | 140 → 119 | 614 → 593 | 740 → 719 |
| 390 × 700 | 260 → 224 | 240 → 204 | 714 → 678 | 840 → 804 |
| 768 × 600 | 200 → 173 | 180 → 153 | 597 → 570 | 723 → 696 |
| 768 × 700 | 280 → 241 | 260 → 221 | 677 → 638 | 803 → 764 |
| 932 × 430 | 150 → 130.5 | 130 → 110.5 | 547 → 527.5 | 673 → 653.5 |

The whole view/card reductions equal the input-height reduction exactly: e.g.
42 px on iPhone-sized cases and 46.5 px on 900 px-high tablet/desktop cases.
The native browser may quantize a pointer coordinate to a CSS pixel, particularly
at very short heights; midpoint drag tests permit only one pixel's worth of DMX
resolution. They require exact 0/255 endpoints and matching final transmitted,
displayed and native values. Independent DOM/native-position checks verify value
128 against its expected center position. No application rounding or coarser
step was introduced; +/- still changes by one and numeric entry remains exact.

## Measured Settings widths

CSS pixels. These show the original inner restriction and the new card-fitting
width. At mobile sizes the card remains full content width; desktop shrinks the
card itself while retaining a compact rail. All three inputs match.

| Viewport | Card before → after | Slider before → after |
|---|---:|---:|
| 320 × 900 | 288 → 288 | 230 → 250 |
| 375 × 812 | 343 → 343 | 280.59 → 305 |
| 390 × 844 | 358 → 358 | 294.39 → 320 |
| 430 × 932 | 398 → 398 | 331.19 → 360 |
| 600 × 800 | 568 → 568 | 360 → 530 |
| 768 × 900 | 712 → 400 | 360 → 362 |
| 1280 × 900 | 968 → 400 | 360 → 362 |
| 390 × 600 | 358 → 358 | 294.39 → 320 |
| 390 × 700 | 358 → 358 | 294.39 → 320 |
| 768 × 600 | 712 → 400 | 360 → 362 |
| 768 × 700 | 712 → 400 | 360 → 362 |
| 932 × 430 | 876 → 400 | 360 → 362 |

## Validation

- Full frontend factory/navigation/live suites passed, including existing channel,
  +/-/numeric editing, multi-client state, presets, blackout and recovery cases.
- New responsive browser suite passed all twelve before/after cases above. It
  measures actual native thumb positions, effective travel, unclipped endpoints,
  midpoint coherence, unchanged handle/touch/button sizes, rail/form/card/view
  shrinkage, column count and navigation geometry. It verifies full-width mobile
  Settings, symmetric padding, tick endpoints, no horizontal overflow, scrolling
  policy, compact desktop card, collapsed empty tracks and future-card stacking.
- Focused Settings browser suite passed at 320/375/390/430/768/1280 px, including
  all fifteen native stop positions, live preview, one final persistence operation,
  pointer cancellation, physical/peer synchronization, conflict ownership,
  failed-save feedback, quiet loading/success, keyboard focus and reconnect
  without mutation replay. No Refresh/Apply controls were reintroduced.
- Actual native fader dragging in representative phone, short-phone and desktop
  cases reached 0/255 and a midpoint, with final-value delivery checked. +/- and
  numeric entry passed; opening the numeric editor does not change card height.
- Existing seven-width header/gesture suite, preset-action browser suite and
  native local HTTP gzip/SRI/cache/startup/recovery suite passed. These use
  synthetic server state, never real fixtures or device NVS.
- Ten pipeline/network/Dispatcher/infrastructure/patch-state commands passed,
  plus 22 existing freshly compiled host cases and three Settings host executables.
  Controller, Dispatcher, ISR ownership/fault injection, HTTP deadlines and
  settings persistence/sleep/warning-volume protections remain covered.
- Deterministic generation twice, all four builds, final build/component/source
  identities and `git diff --check` passed. Every gzip asset is below 16 KiB.
  Before/after measurements and inspected screenshots are under ignored
  `dist/responsive-ui-20261029/browser/`.

Native Chromium geometry/touch emulation does not establish real Safari toolbar,
Home Screen, native touch sampling, electrical DMX integrity or hardware timing.
The user subsequently physically accepted this responsive layout on iPhone.
The blackout color follow-up below still needs a physical color check;
no automatic upload occurred.

## Resources

| Gzip asset | Before | After | Delta |
|---|---:|---:|---:|
| HTML | 4,260 | 4,268 | +8 |
| CSS | 4,633 | 4,741 | +108 |
| Core JS | 10,103 | 10,103 | 0 |
| UI JS | 10,223 | 10,223 | 0 |

All four firmware builds pass; static RAM is unchanged:

| Environment | Static RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| Unit | 88,644 | 0 | 1,076,057 | +112 |
| Base | 88,644 | 0 | 1,076,033 | +112 |
| Unit diagnostics | 91,356 | 0 | 1,080,657 | +128 |
| Base diagnostics | 91,356 | 0 | 1,080,613 | +112 |

Final source fingerprint shared by all four verified build receipts:
`482989c41342e04c238116496f7b7a64e0b740989d36f920aa268ed3e3b6010e`.

## Exact task inventory

- `web/src/index.html`: one grid wrapper around Device Settings; Preferences stays outside.
- `web/src/style.css`: responsive card sizing, full-width controls, precise tick inset,
  one shared effective-travel scale over the original responsive base heights.
- `web/generated/index.html.gz`
- `web/generated/style.css.gz`
- `web/generated/manifest.json`
- `tests/web/test_header_browser.cjs`: optional explicit viewport height; old default unchanged.
- `tests/web/test_device_settings_browser.cjs`: new responsive card/tick expectations
  and actual native geometry at all fifteen Settings stops.
- `tests/web/test_responsive_layout_browser.cjs`: twelve-case native before/after
  geometry suite and fader interactions. Optional `--baseline` consumes preserved
  old assets; without it, the prior base geometry is explicit in the test.
- `docs/STAGE6B_RESPONSIVE_UI_POLISH.md`: this report.

All other existing source, untracked historical files, stashes and work remain
unchanged. Core/UI JS source and gzip bundles are byte-for-byte unchanged. No
staging, commit, push, merge, tag, release, firmware upload or NVS erase/migration.

## Physical iPhone acceptance checklist

1. Safari and Home Screen, portrait and landscape, toolbar expanded/collapsed:
   verify full-width symmetric Settings rails, matching tracks, readable labels,
   rectangular handles and all five precise stops, including 4 min → Always On.
2. Live Settings drag/release, cancelled drag, physical change, second-client
   change and disconnect/reconnect: verify hardware preview, one final save,
   ownership protection, quiet normal state and real-error visibility.
3. iPhone 13 Pro Max Editor: compare comfortable visible fader area and normal
   scrolling; verify both ends, no clipping/overlap, unchanged +/- targets,
   percentage labels, bank controls and Go to CH.
4. Drag to 255 and 0 slowly/quickly and check actual fixture/display/final values.
   Verify fine +/- and direct numeric entry remain exact. Check touch resolution
   at shorter heights without assuming every DMX value has a distinct physical pixel.
5. Repeat preset loading, blackout ON/OFF/restore, multi-client updates and
   background/resume; confirm bottom navigation, fixed scale and scrolling outside
   interactive surfaces remain normal. Unit stability and experimental Base status
   remain as previously documented.
6. Device Maintenance remains deferred: no Restart, Power Off or credential
   operation is implemented. Add a real independent card only in its approved phase,
   with its own confirmation/authorization and safe transport/shutdown design.

## Final follow-up: authoritative lower blackout status color

The responsive Editor/Settings layout has passed physical iPhone acceptance.
This follow-up only paints the existing lower Editor blackout row:

- The existing row receives an ID; its DOM nodes and text stay intact.
- The app renderer reuses its existing `ready && blackout` condition for a
  `blackout-active` class, writing the class only when it changes.
- Confirmed ON paints all row text, including existing pending/error detail,
  and the dot's border/fill with existing `--red` (`#ff858f`). Contrast against
  the existing dark root background exceeds 4.5:1.
- Confirmed OFF retains the original muted text and outlined dot. Pending ON
  stays neutral; pending OFF stays red until confirmed OFF. Stale state keeps
  the existing unavailable wording and neutral appearance; a fresh complete
  snapshot restores confirmed color.
- CSS changes only color, border-color and background. Text, typography,
  spacing, dimensions, scrolling and main blackout button logic are unchanged.
  No command, Controller, DMX, snapshot, Settings or scheduling changes.

### Automated validation

- Full frontend/live/navigation/Settings/Status suites pass, including startup,
  confirmed/pending ON/OFF and stale-state class assertions.
- New `tests/web/test_blackout_status_browser.cjs` passes at
  320/375/390/430/600/768/1280 px using the shipped UI/native Chromium CSS and
  synthetic server state, with no device access. It checks commands/rejections,
  peer changes, disconnect/reconnect without mutation replay, entire-row/dot
  palette, contrast, retained DOM identity/typography and identical geometry
  with/without the color class at the same text state. Mobile checks keep the
  red row visible above bottom navigation with the header scrolled away. The
  screenshot was visually inspected.
- Existing seven-width header/preset/gesture browser suite passes, including
  preset pending/bulk/ack/reload/foreign/rejected/reconnect button stability,
  genuine ON/OFF, pending/offline, compact status and native fader geometry.
- All ten pipeline/network/status/SoftAP/WebSocket/canvas/startup/Dispatcher/
  infrastructure/reproducible-driver-patch checks pass.
- Two successive generations are byte-identical. All four build receipts match
  current source/assets and binary component hashes. Git whitespace checks pass.
  Logs/screenshots are under ignored `dist/blackout-status-20261029/`.

| Gzip asset | Before follow-up | After | Delta |
|---|---:|---:|---:|
| HTML | 4,268 | 4,269 | +1 |
| CSS | 4,741 | 4,771 | +30 |
| Core JS | 10,103 | 10,103 | 0 |
| UI JS | 10,223 | 10,276 | +53 |

Every compressed asset remains below 16 KiB. All four builds succeed:

| Environment | Static RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| Unit | 88,644 | 0 | 1,076,137 | +80 |
| Base | 88,644 | 0 | 1,076,113 | +80 |
| Unit diagnostics | 91,356 | 0 | 1,080,737 | +80 |
| Base diagnostics | 91,356 | 0 | 1,080,693 | +80 |

Source fingerprint shared by all four receipts:
`a8a8f74ab6d374b0d24da4e20a19277656105a8f0b8d820e5ce115b11570ba98`.

### Follow-up files and physical check

- `web/src/index.html`, `web/src/js/app.js`, `web/src/style.css`: row ID,
  authoritative class and paint-only rules.
- `web/src/ui.js`: regenerated composition.
- `web/generated/index.html.gz`, `style.css.gz`, `ui.js.gz`, `manifest.json`:
  deterministic assets. Core source/gzip unchanged.
- `tests/web/test_live.mjs`, `tests/web/test_blackout_status_browser.cjs`:
  authoritative-state assertions and the focused browser suite.
- `tests/network/device_settings_guard_support.py`: reviewed app fingerprint
  updated for the two added class-rendering lines; other pins unchanged.
- `docs/STAGE6B_RESPONSIVE_UI_POLISH.md`: accepted responsive layout, deferred
  50/50 Maintenance requirement and this follow-up.

Physical check: iPhone Safari/Home Screen, scroll the header away and verify red
ON text/dot and muted OFF, pending/rejection, peer changes and reconnect. Browser
emulation does not establish real Safari rendering or electrical DMX correctness.
Existing uncommitted Stage 6B.5A, historical work, logs and stash remain preserved.
Nothing staged/committed; no upload, push, merge or NVS operation occurred.
