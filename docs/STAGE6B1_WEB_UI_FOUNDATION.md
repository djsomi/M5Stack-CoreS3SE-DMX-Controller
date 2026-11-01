# Stage 6B.1 - Web UI foundation and asset pipeline

Status: Stage 6B.1 implementation and real-device validation accepted by the user,
including the final tab-scroll reset and standalone iPhone Home Screen mode.
Branch: `codex/stage6b-web-ui`, based on Stage 6A commit
`294b8b8ea21d139555a7222f16b5eeff00d16091`.
The approved `docs/STAGE6B_WEB_UI_PLAN.md` is preserved unchanged.
Earlier implementation sections retain their pre-commit checkpoint findings;
the final closure section records hardware acceptance and the local commit scope.
No firmware upload, push, merge, tag or release is part of closure.

## Behavior and scope

The offline page now has a compact dark Editor, Presets, Settings and Status
shell. It explicitly says **NON-LIVE PREVIEW**: displayed fader values are
illustrations, not a Controller snapshot. The connection indicator remains
Not connected; blackout and device information remain unavailable.

Functional local controls are section navigation, Previous/Next bank, Go to CH,
P1-P8 selection and an optional secondary percentage display. Bank selection
and percentage preference are versioned browser-local preferences. They never
change touchscreen selection, device settings or DMX values. Unavailable,
corrupt, oversized or full local storage falls back to usable in-memory state.

All faders, Blackout, Load, Save, Copy and Clear are disabled native controls.
Selecting a preset does not read its contents or imply that its slot is empty.
The page opens no WebSocket, performs no fetch/status polling and submits no
DMX commands. The existing Stage 6A endpoints and protocol remain available to
other clients, unchanged.

Below 480 CSS px the Editor has four faders; from 480 to 679 it has six; from
680 it has eight. Only visible channels are rendered, including partial final
banks, with a centered 1024 px application limit. Narrow screens use bottom
navigation. Short landscape screens use shorter rails and vertical scrolling.
Safe-area padding, viewport sizing, focus indicators and reduced-motion styles
are included. Active-blackout styling is prepared but never assigned a fake
active state.

## Files created and modified

Authored UI and generated outputs:

```text
M web/src/index.html
M web/src/style.css
A web/src/js/state.js
A web/src/js/preferences.js
A web/src/js/fader.js
A web/src/js/editor.js
A web/src/js/presets.js
A web/src/js/settings.js
A web/src/js/status.js
A web/src/js/shell.js
A web/src/js/app.js
M web/src/app.js                   generated, checked in
M web/generated/index.html.gz
M web/generated/style.css.gz
M web/generated/app.js.gz
M web/generated/manifest.json
```

Pipeline, checks, preview and documentation:

```text
A scripts/web_ui_bundle.py
M scripts/web_asset_pipeline.py
M tests/network/check_integration.py
M tests/network/check_websocket_integration.py
A tests/web/check_scope.py
A tests/web/check_pipeline.py
A tests/web/run_tests.mjs
A tests/web/test_navigation.mjs
A tests/web/preview.py
A tests/web/README.md
A docs/STAGE6B1_WEB_UI_FOUNDATION.md
A docs/STAGE6B_WEB_UI_PLAN.md       approved Stage 6B.0 planning baseline
```

There are no C++/header, dependency, platform, transport patch, HTTP server,
partition, touchscreen, USB initialization or packaging-script edits. Existing
historical files, unrelated regression report, logs and stash remain outside
the commit. The approved architecture plan is included. Validation logs/screenshots are ignored under
`dist/stage6b1-validation-20261025/`; clean builds do not depend on them.

## Module boundaries and future integration

Each authored JS file is a factory expression with explicit module/dependency
comments. The Python composer puts factories into separate scopes within one
strict IIFE, in a reviewed topological order. It rejects missing, duplicate,
unlisted, reordered or incorrectly declared modules and invalid source encoding.
It performs no import rewriting, evaluation or minification.

| Module | Current responsibility | Stage 6B.2 integration point |
| --- | --- | --- |
| state | Frozen not-ready presentation state; no invented universe | Private confirmed state, validated snapshots and ordered updates |
| preferences | Version 1 selected channel and percentage only | Remains browser-only; no domain values or pending commands |
| fader | Disabled illustrative channel card | Explicit user intents; confirmed values supplied by Editor |
| editor | Bank calculation, bounds, responsive visible cards | Read-only state selectors and explicit edit intents |
| presets | Local slot selection only | Validated occupancy and Dispatcher-backed operations |
| settings | Useful percentage preference only | No unsupported device settings |
| status | Explicit unavailable fields | Bounded existing-resource reads when approved |
| shell | Hash sections, title, focus, connection presentation | Connection state and authoritative blackout presentation |
| app | Dependency wiring and startup | Wire protocol/transport/command modules when implemented |

Protocol, transport and command-tracking modules are deliberately deferred,
as allowed by the plan's incremental module creation. No speculative socket,
reconnect or request machinery is added. Future integration must preserve the
Stage 6A v1 contract, epochs/generations, snapshot/update sequencing, bounded
pending commands and explicit blackout semantics described in the approved plan.

## Reproducible generation

Use the existing pinned Python 3.11.7 / zlib 1.2.13 environment:

```powershell
python -B scripts/web_asset_pipeline.py --generate
python -B scripts/web_asset_pipeline.py
```

On this Windows installation the pinned interpreter is
`C:/Users/<user>/.platformio/penv/Scripts/python.exe`. Ordinary PlatformIO builds
only verify the checked-in bundle, gzip files and manifest; stale authored
modules or generated output fail clearly. Verification never regenerates
checked-in files. Explicit generation first composes `web/src/app.js`, then uses
the existing deterministic gzip/manifest pipeline. UTF-8 and LF normalization
support Windows checkouts without changing served bytes.

The only served assets remain `/`, content-hashed CSS and content-hashed JS.
Generator versions, deterministic gzip headers, MIME types, hash references,
manifest integrity and linker-retained flash embedding are preserved. Both
generation and verification now enforce the existing HTTP limit of **16384
compressed bytes per asset**, in addition to the original aggregate budget.
There is no filesystem or runtime decompression.

| Served asset | Uncompressed bytes | Gzip bytes | Limit |
| --- | ---: | ---: | ---: |
| index.html, after hashed URL substitution | 9020 | 2686 | 16384 |
| style.css | 12277 | 3488 | 16384 |
| app.js | 9858 | 3201 | 16384 |
| Total | 31155 | 9375 | Per-asset limit applies |

Bundle SHA-256:
`08ac84ddd405f750d307dcc20985200ba0ba6091b378a19acf8eb8225fbf4163`.
Build-input SHA-256:
`e71a24dde2348f138a6cc1037fa1e38c588d97a58c9105b619a3bc84a39b8ea5`.
All four artifact receipts match these digests.

## Firmware resources and build results

All four `pio run -e ...` builds succeeded with the existing 16 MiB layout and
two 6 MiB application slots. Compared with Stage 6A:

| Environment | Static RAM | RAM delta | Flash use | Flash delta | firmware.bin bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| cores3se_unit | 87276 | 0 | 1049397 | +8704 | 1049808 |
| cores3se_base | 87276 | 0 | 1049365 | +8704 | 1049776 |
| cores3se_unit_diagnostics | 89892 | 0 | 1051541 | +8720 | 1051952 |
| cores3se_base_diagnostics | 89892 | 0 | 1051509 | +8720 | 1051920 |

Flash values are PlatformIO ELF-derived usage; firmware.bin includes image
metadata/padding. These are the current tab-scroll-reset build results; deltas are
versus Stage 6A. Incremental viewport-change deltas are recorded below. Partition-image SHA-256
is unchanged in every build:
`6731888b421c3946dac16356659877021e0f46dbd1c55abafa17989eb7265387`.
The existing artifact hook verified flash headers, partition limits, retained
assets and separate-image receipts. Firmware packaging behavior is unchanged;
no release/FULL.bin was generated.

The larger UI lives in const flash arrays. No new firmware allocations or
tasks are introduced, and the existing streaming HTTP path is unchanged.
Actual runtime heap, transfer behavior and client-browser memory still require
device measurement; unchanged static RAM is not a runtime heap measurement.

## Automated and browser validation

- All 16 freshly compiled C++ host cases passed: Controller, Dispatcher,
  transport/real ISR fault injection, WebSocket bridge, native adapter, SoftAP
  policy at three station limits, canvas guard, startup history, four default
  trace configurations, HTTP and embedded asset API.
- Network/WS, Dispatcher, startup-trace, PSRAM-canvas and SoftAP integration
  guards passed. Driver patch-state tests passed for pristine/repeated/mixed/
  unknown/template/version and prior-policy scenarios.
- All six existing infrastructure fixture tests passed, including reproducible
  gzip, corruption/freshness and flash/package failure cases.
- All eight Python UI pipeline cases passed, covering deterministic actual
  output, Windows normalization, module failures, dependency metadata, stale
  output/read-only verification, hard size limits, manifest/body hashes,
  missing/corrupt assets, semantic navigation and disabled device controls.
- Node built-in tests passed against actual factory sources and generated
  bundle syntax: scoped globals, no network calls at factory initialization,
  non-live state, preferences/failure fallback and all 512 bank positions for
  each of 4/6/8 columns. No npm/build runtime dependency was introduced.
- The scoped protection gate compares every existing firmware, header, driver,
  partition and build input against exact Stage 6A Git blobs. Only the named
  composer/pipeline exceptions are allowed. Existing web-byte freezes are
  replaced with actual composition, gzip and scope validation; unrelated
  integration rules remain in force.
- `git diff --check` and added-text whitespace checks passed.

The actual generated gzip bodies were served through a loopback-only test
helper with the device CSP and MIME/encoding headers, using Chromium in the
Codex in-app browser. Tested 320x740, 390x844, 600x390, 768x1024 and 1440x900;
counts were 4/4/6/8/8, with no horizontal page overflow and a 1024 px desktop
cap. All four sections also fit at 320 px. Local P8 selection, Go to CH 512,
last-bank bounds, invalid 513 native validation, percentage/channel persistence
across reload and disabled device controls passed. Captured console warnings
and errors were empty. The helper saw only the three assets plus an optional
browser favicon lookup (404); it implements no device API.

Screenshots and browser-result JSON are in the ignored validation directory.
This is Chromium layout evidence, not physical mobile/Safari, touch, 200% browser
zoom, SoftAP or DMX electrical validation.

## Hardware validation procedure

After reviewing these changes, use the existing migrated 16 MiB device and its
known port; no erase, NVS reset, migration or FULL.bin write is needed:

```powershell
pio run -e cores3se_unit -t upload --upload-port COM8
```

For a separately approved experimental Base test:

```powershell
pio run -e cores3se_base -t upload --upload-port COM8
```

The commands are instructions only; neither was executed in this stage.
The established upload target writes its configured boot/table/app0/application
images and may reset OTA selection; it does not erase presets or credentials.

1. Check normal boot, saved credentials/presets and Settings Wi-Fi READY state.
   Connect a phone/tablet to the existing SoftAP; open `http://192.168.4.1/`.
2. Test current iOS Safari and Chrome portrait/landscape, a tablet and desktop.
   Confirm 4/6/8 banks, readable values, safe-area navigation, normal vertical
   scrolling, visible keyboard focus and no horizontal overflow at 320 px/200%.
3. Verify the preview warning remains visible, every fader/Blackout/preset
   mutation control is disabled, and no illustrative value is presented as
   confirmed device state. Try pointer, touch and keyboard input.
4. Navigate all sections, select P8, move banks at both ends, jump to 512,
   reject invalid entries, resize and reload. Verify local percentage/selection
   persistence, including with browser storage unavailable.
5. Check browser Network/Console: three same-origin assets, no external fonts,
   scripts or CDN; no page-originated `/ws`, `/status` or `/ws-diagnostics`
   request, no DMX command and no JS/CSP error. A browser-requested favicon 404
   is not a UI dependency; no firmware endpoint was added to suppress it.
6. Disconnect internet and repeat local loading/navigation. Confirm existing
   `/status` and `/ws-diagnostics` remain available through separate manual
   requests and Stage 6A clients can still use `/ws` independently.
7. Exercise touchscreen channel/precision/keypad edits, physical presets,
   blackout restore, sleep/wake, sound/battery warnings and continuous fixture
   output while loading the page on multiple clients.
8. Compare actual internal heap, largest DMA block and PSRAM with Stage 6A
   under HTTP loads. Do not infer electrical packet correctness from logs.

## Limitations and deferred work

The user has accepted the final design and navigation on real CoreS3SE hardware,
desktop/tablet browsers, iPhone Safari and standalone Home Screen mode.
Browser-local selection is the target channel
for future editing, not a confirmed device selection. Preview values are
deliberately illustrative and repeat by visible position across banks.

Live synchronization, confirmed universe ownership, bounded command tracking,
precision dialogs, interactive faders, reconnect handling, explicit remote
Blackout and preset operations belong to Stage 6B.2 and later. Firmware settings,
credential editing and OTA remain outside scope. Existing hardware/security/
transport limitations from prior stages are unchanged, including experimental
Base output status and pending external DMX measurements. Closure creates only
a focused local commit; it performs no upload, push, merge, tag or release.

## Earlier iOS navigation and heading-focus polish

The user has confirmed this correction resolved the tab-switching flicker and
unwanted H1 outline on the real iPhone. The following records that earlier pass;
its resource deltas precede the viewport-shell change below.

Source findings: the nav already lives outside the four persistent view sections
and is never recreated. CSS contains no navigation transition/animation and no
changing ancestor transform/stacking context. Existing fixed positioning,
`100svh`, breakpoints, colors, spacing and safe-area padding remain unchanged.

The original links performed native fragment navigation, then a later
`hashchange` handler switched sections and **always** called H1
`focus({preventScroll:true})`. This split native/script route path and
unnecessary pointer-triggered focus are identified scroll/repaint triggers;
programmatic focus also permits the existing H1 `:focus-visible` ring to appear.
WebKit documents [focus-indication heuristics](https://webkit.org/blog/12179/the-focus-indicated-pseudo-class-focus-visible/).
The exact iOS compositor/toolbar cause of the transient missing bar cannot be
proven with Chromium or source inspection; the change removes these concrete
triggers rather than claiming an independently reproduced WebKit rendering bug.

Correction in `web/src/js/shell.js`:

- Ordinary unmodified primary clicks prevent native fragment navigation, retain
  the hash URL through `history.pushState`, and update the view and `aria-current`
  synchronously. No delayed work, transition or extra render is introduced.
- Reveal the destination before hiding the previous view, leaving the shell
  and fixed nav mounted. Initial deep links hide all nonselected views.
- Pointer/touch navigation does not focus H1. Keyboard/AT click activation
  (`detail === 0`) still focuses its `tabindex=-1` heading with `preventScroll`.
  Title, region labels and current-link semantics remain available to assistive
  technology. CSS focus rules on headings and interactive controls are intact;
  there is no global outline reset or pointer-specific CSS workaround.
- Back/Forward (`popstate`) and direct fragment edits (`hashchange`) select the
  correct view without forced focus. Duplicate events and active-tab clicks do
  not rerender/refocus. Modified and nonprimary clicks keep native behavior.

The [History API](https://developer.mozilla.org/en-US/docs/Web/API/History/pushState)
does not emit `hashchange` for `pushState`; the immediate explicit route call is
therefore necessary. The UI still uses fragment routes, not new HTTP paths.
There are no explicit scroll calls or history-restoration overrides. A shorter
destination can naturally clamp a previously scrolled document to its new
maximum, and native history can restore a previous position. Those browser
behaviors are distinct from a forced heading scroll. Do not add blank filler
height, forced scroll-to-top, GPU promotion or fixed timers to conceal them.

Exact files changed by this polish pass (all earlier Stage 6B.1 work preserved):
`web/src/js/shell.js`, generated `web/src/app.js`,
`web/generated/app.js.gz`, `web/generated/index.html.gz`,
`web/generated/manifest.json`, `tests/web/run_tests.mjs`,
new `tests/web/test_navigation.mjs`, and this report. CSS and authored HTML are
byte-for-byte unchanged; generated HTML changes only its content-hashed JS URL.

Validation: the new Node shell test is included by the existing JS runner and
exercises actual source with DOM/history mocks: immediate route changes, 100
switches, no empty view, persistent nav, pointer/keyboard focus, same-tab
idempotence, modified clicks, history deduplication, initial deep links and
invalid fragments. All existing JS checks, seven pipeline cases, six
infrastructure cases, integration/patch-state guards and 16 compiled host
regression cases passed again. All four firmware builds passed with matching
source/asset receipts and unchanged partition images. Tables above are updated.
Compared with the prior Stage 6B.1 build, static RAM delta is zero and flash/
firmware.bin delta is +336 bytes for every environment; gzip total delta +334.

Chromium at phone width confirmed repeated pointer navigation keeps the nav
bottom at viewport height, scroll position zero when starting at the top,
correct current link, exactly one view and no focused H1. Enter activation
focused the heading without scrolling; Tab showed a visible control outline.
Preferences/reload and Back/Forward passed. This does not establish iOS toolbar,
safe-area, VoiceOver or transient compositor behavior. New logs/evidence are
ignored under `dist/stage6b1-safari-polish-20261025/`.

Real iPhone acceptance after the manual Unit upload command above: reload the
page to pick up the new hashed JS; repeatedly tap all four tabs at normal and
rapid speeds, at page top and after scrolling. Test portrait/landscape with
Safari toolbar expanded/collapsed and after the numeric keyboard is dismissed.
Confirm the bar never flashes/disappears, active indication is immediate, no
H1 ring appears on ordinary touch, and browser chrome/home indicator are not
overlapped. Verify Back/Forward, direct `/#presets`, VoiceOver heading context,
external-keyboard Enter/Tab focus, unchanged browser preferences, no horizontal
overflow and disabled preview controls. If flicker remains, record iOS version,
orientation, toolbar state and a screen recording before any further CSS change.
No commit, upload, erase, push, merge, tag or release was performed.

## Stable mobile viewport and bottom navigation

The user has since validated this shell on a real iPhone, including standalone
Home Screen web app mode. The following records that viewport correction;
its incremental measurements precede the scroll reset described at the end.

### Investigation and remaining uncertainty

The user reports a remaining jump during Safari toolbar collapse/expansion,
including long-to-short switches near the end of content. The exact Safari
animation/compositor sequence was not captured by the local tools; no claim is
made to have reproduced the supplied recording in Safari. Source inspection
and Chromium establish a separate, concrete application contribution:

| Item inspected | Before this correction |
| --- | --- |
| html/body | Automatic document height, root/document scrolling; no constrained frame |
| Shell | `min-height:100vh` followed by `min-height:100svh`; content may exceed either |
| Viewport units | svh already used in shell minimum and fader rails; no dvh or lvh |
| Bottom nav | `position:fixed; bottom:0` relative to browser viewport below 680 px |
| Containing blocks | No positioned/transformed ancestor or layout containment on shell |
| Safe area | Existing bottom/side env padding on nav, bottom clearance on shell |
| Scroll regions | Browser document is primary; no app-owned overflow region |
| Tab switching | Synchronous persistent sections, destination shown before old view hidden |
| Scroll restoration | Native browser restoration/clamping; no scroll-to-top code |
| Focus | Keyboard/AT only H1 focus with preventScroll; pointer does not focus H1 |
| Resize handlers | Only width matchMedia for fader count; no viewport repositioning JS |
| Transitions | None affecting nav position, transforms, visibility or dimensions |

At 390x740, scrolling Presets to its end produced document height 1336 and
root scroll 596. Switching to Settings shortened the document to 883 and
clamped root scroll to 143. The fixed bar remained at y=740 in Chromium, so
this is **not** a reproduction of Safari's native toolbar jump. It demonstrates
that content changes adjust the browser's scrolling document independently of
Safari's controls. Root scrolling and viewport-fixed positioning expose the
bar to both browser toolbar movement and application document-height changes.

The layout viewport, the currently unobscured visual viewport, and CSS small/
dynamic viewport units are different concepts. Toolbar/keyboard/zoom may alter
visible space without the same change to layout dimensions. Native chrome
movement cannot be removed by application CSS. The proposed fix isolates the
known document-height contribution and avoids viewport-repositioning feedback.
See [WebKit viewport-unit documentation](https://webkit.org/blog/12445/new-webkit-features-in-safari-15-4/)
and [CSS viewport unit definitions](https://www.w3.org/TR/css-values-4/#viewport-relative-lengths).

### Chosen CSS-only strategy and trade-offs

Within `@supports (height:100svh)` and only below 680 px:

- Root overflow is constrained; the body is a positioned `height:100svh` frame.
- The existing `.app-container` becomes the **single native content scroller**:
  height 100%, min-height 0, overflow-y auto, overscroll-behavior-y contain.
  Header, content and footer still scroll naturally together. There are no
  per-page/nested scrolling boxes, event cancellation or gesture handlers.
- The existing nav changes from fixed to absolute. Its containing block is
  the positioned body, outside the unpositioned/untransformed app scroller.
  Thus its geometry is independent of view height and container scroll offset,
  despite its preserved DOM ancestry. No node is moved, cloned or rebuilt.
  This uses standard [overflow/containing-block behavior](https://www.w3.org/TR/CSS22/visufx.html#overflow-clipping).
- Existing bottom/side safe-area padding, z-index, 51 px link targets and the
  `100px + safe-area-inset-bottom` content clearance stay unchanged. No nav
  height is hardcoded to compensate for an assumed device inset.
- Browsers without svh keep the original document-scroll/fixed-nav fallback.
  At 680 px and above, document scrolling and inline nav remain unchanged,
  including wide-phone landscape layouts using the existing tablet breakpoint.

This deliberately changes the mobile scroll owner because the old document
clamping was reproducible and a minimum-height change alone cannot constrain
either the document or a viewport-fixed bar. A flex shell with additional DOM
wrappers was unnecessary. Simply using dvh would resize the shell with toolbar
animation; simply changing the existing minimum from svh would not solve this
coupling. No JavaScript viewport workaround, timer, polling, transform promotion,
animation, smooth scroll, browser detection or history override was added.

The trade-off is deliberate: when Safari retracts its chrome, extra visible
space may remain below the small-viewport shell instead of expanding the app.
Inner scrolling may also leave Safari's own controls expanded rather than
driving document-scroll toolbar collapse. No browser controls are forcibly
hidden. svh may change on real resize/orientation; older Safari differences
still require physical validation. A software keyboard can obscure part of
the frame because svh is not a keyboard-height API; confirm input visibility
and recovery after dismissal. Zoom remains enabled and the viewport meta tag
is unchanged. Do not claim that CSS can prevent all native viewport movement.

### Changes, tests and resources

Exact files changed in this pass: `web/src/style.css`, generated
`web/generated/style.css.gz`, `web/generated/index.html.gz`,
`web/generated/manifest.json`, `tests/web/check_pipeline.py`,
`tests/web/README.md` and this document. All authored HTML, JS modules, composed
JS, routing/focus handling, firmware and asset-generation code are unchanged.

The shipped-CSS test checks support/breakpoint scoping, the single scroller,
external containing block, unchanged fallback, safe-area/content clearance,
absence of dynamic-viewport/JS repositioning and preserved zoom metadata.
All eight pipeline cases, six infrastructure cases, JS logic/navigation tests,
integration/patch-state checks and all 16 freshly compiled host cases passed.
All four firmware builds passed with matching current source/asset digests and
unchanged partition images. The main asset/resource tables above are current.

| Increment since the preceding navigation-polish build | Unit/Base normal | Unit/Base diagnostics |
| --- | ---: | ---: |
| Static RAM | 0 | 0 |
| ELF flash use | +240 bytes | +256 bytes |
| firmware.bin size | +240 bytes | +256 bytes |

HTML gzip is 2684 bytes, CSS 3488, JS 3080; total 9252, +257 bytes since the
preceding build, with every asset below 16384. JS bytes and authored HTML are
unchanged; generated HTML substitutes the new content-hashed stylesheet URL.
No firmware allocations/tasks or runtime asset decompression were added.

Actual generated-asset Chromium checks covered 320x640, 390x740, 600x390,
768x1024, 844x390 and 1440x900, all four sections and repeated switches. Mobile
root scroll stayed zero, bar bottom stayed at frame height, one section was
visible and no horizontal overflow occurred. Before/after long-to-short test:
the same 596-to-143 clamp now occurs only inside `.app-container`; document
scroll remains zero and bar position stays y=740. Footer bottom at long-view
end was y=639.66, clear above nav top y=674. Enter/Tab focus, preferences/reload,
channel 512 bounds and Back/Forward passed; device controls stayed disabled and
captured console errors/warnings were empty. Desktop/tablet nav remained inline.
This is layout evidence, **not Safari toolbar emulation or hardware acceptance**.
Ignored evidence is under `dist/stage6b1-viewport-20261025/`.

### Physical iPhone acceptance matrix

Use the existing manual Unit upload command above only when instructed, then
reload `http://192.168.4.1/` to pick up the new CSS hash. Do not erase NVS.
Run the matrix in both **normal and private Safari tabs**, recording iOS/Safari
version and whether chrome actually collapses while scrolling the inner region.

| Orientation / browser state | Exercise | Acceptance |
| --- | --- | --- |
| Portrait, controls expanded | Open each view, scroll Editor/Presets to bottom | Bar accessible above browser chrome/home indicator; footer clear |
| Portrait, controls collapsed where Safari permits | Scroll repeatedly up/down | No extra app-induced oscillation; stable small-frame gap acceptable for review |
| Portrait, partially collapsed | Switch long/short views at bottom, then tap rapidly | Correct active tab, persistent bar, no repeated jump/flicker or forced top scroll |
| Landscape, expanded/collapsed | Rotate at top and bottom, repeat switches | Existing width-based inline/tablet nav when >=680 px; no notch clipping/overflow |
| Normal and private tabs | Set percentage preference, navigate/reload | Preference works where storage permits; graceful fallback otherwise |
| Keyboard open/close | Focus Go to CH, enter 512, dismiss, switch tabs | Input reachable; frame/nav recover without stuck offset or clipping |
| Accessibility | Pinch zoom, VoiceOver, external-keyboard Enter/Tab | Zoom allowed, usable navigation/announcements, visible keyboard focus, no touch H1 ring |

Also verify no page-originated WebSocket/API traffic, disabled mutation controls,
unchanged physical UI/DMX operation, and Safari's own controls remaining usable.
If the stable-small-height gap or inner-scrolling toolbar behavior is unsuitable,
record that as a trade-off to review rather than silently adding resize handlers.
If a jump remains, provide a recording with orientation, toolbar/keyboard state
and iOS version before selecting another positioning strategy.
No commit, firmware upload, push, merge, tag or release was performed.

## Final tab-navigation scroll reset

The shared content scroller previously retained its offset across view changes.
The navigation now assigns `scrollTop = 0` synchronously after switching views
and before keyboard/AT heading focus. It selects `.app-container` when its
computed overflow-y is auto; otherwise it uses `document.scrollingElement` for
the established desktop/tablet and unsupported-svh document-scroll layouts.
The style is checked on each successful change, so resizing across the breakpoint
does not leave a cached, incorrect owner. No `window` scroll call, timer, smooth
animation, viewport handler, focus-policy change or history-policy override is
introduced. The existing same-tab early return runs before the reset.

Exact files changed in this pass: `web/src/js/shell.js`, generated
`web/src/app.js`, `web/generated/app.js.gz`, `web/generated/index.html.gz`,
`web/generated/manifest.json`, `tests/web/test_navigation.mjs`, and this report.
All CSS, authored HTML, scroller/nav geometry and firmware inputs outside web
assets remain unchanged. Historical work, logs, stash and the uncommitted
Stage 6B.1 implementation are preserved.

Extended tests cover Editor-to-Presets, Presets-to-Settings, Settings-to-Editor,
repeated changes, same-tab offset preservation, both mobile/document scroll
owners, no writes to the non-owner, and reset-before-keyboard-focus ordering.
Existing pointer/focus, history, persistent-nav and 100-switch checks still pass.
All eight pipeline tests, six infrastructure cases, integration/patch-state
checks, JS tests and 16 freshly compiled host cases passed. All four builds
succeeded with matching source/asset receipts and unchanged partition images.

Chromium at 390x740: Editor scroll 473 became Presets scroll 0; Presets scroll
596 was preserved on a same-tab click, then became Settings scroll 0. Keyboard
Editor activation reset to 0 and focused H1; Tab retained visible control focus.
Root scroll stayed 0 and nav bottom stayed y=740 throughout mobile transitions.
Repeated changes passed with no horizontal overflow or captured console errors.
At 1024x740, the document-scroll fallback reset from 184 to 0 on a new tab.
Ignored evidence is in `dist/stage6b1-scroll-reset-20261025/`.

Incremental sizes versus the validated viewport-shell build: JS +297 raw bytes,
gzip +121; generated HTML unchanged raw size, gzip +2 because of the new hashed
JS URL; CSS unchanged. Total gzip is 9375 bytes, +123. Every asset stays below
16384 bytes. Static RAM is unchanged; ELF flash and firmware.bin each increase
128 bytes in all four configurations. Current absolute sizes are in the tables
above. No firmware runtime allocations or tasks were added.

Final physical acceptance: in both normal Safari and standalone Home Screen
mode, scroll Editor down and open Presets; scroll Presets down and open Settings;
then open Editor. Each different tab must show its heading/first content at the
top immediately. Repeat at both orientations, tap the active tab while scrolled
and confirm its offset stays unchanged, and check that the nav remains stable
through toolbar/keyboard changes. Recheck Enter/Tab/VoiceOver focus, preferences
and disabled controls. This last reset has not been hardware-tested by the agent.
No commit, flash, push or merge was performed.

## Final hardware acceptance and local closure - 2026-10-25

The user physically tested and accepted the complete Stage 6B.1 implementation
on the M5Stack CoreS3SE. Accepted results are:

- Dark Luminair-inspired design, desktop/tablet layouts and iPhone portrait.
- Standalone iOS Home Screen web app operation.
- Stable bottom navigation and Safari toolbar/viewport behavior, with the
  earlier flicker and unwanted pointer-triggered H1 outline resolved.
- Different-tab activation immediately opens the destination at the top;
  active-tab activation preserves its current scroll position.
- Browser preferences and navigation work correctly.
- Existing physical DMX functionality remains operational.

These are user-confirmed hardware observations, separate from Chromium/mock
test evidence. They do not establish external RS-485 electrical correctness,
four-client performance, new runtime-memory measurements or every optional
private-mode/zoom/accessibility matrix combination. No additional UI redesign
or feature work was performed during closure.

The local commit includes the approved Stage 6B.0 architecture plan and all
28 reviewed Stage 6B.1 UI, pipeline, test, generated-asset and documentation
files listed above. Historical development files, the unrelated Stage 3
regression report, logs, backups, firmware binaries and stash are excluded.
Commit message: `Stage 6B.1: Add responsive web UI foundation`.

Closure validation passed: all frontend/JS/navigation tests, eight pipeline
cases, six infrastructure cases, all 16 freshly compiled C++ host cases,
network/WS/Dispatcher/canvas/startup/SoftAP integration guards and driver
patch-state regressions. All four established PlatformIO builds succeeded.
The deterministic gzip/manifest checks, per-asset 16 KiB gates, firmware source
scope guard and artifact/partition/source/asset receipt comparisons passed.
Absolute asset/build sizes above are unchanged from the accepted implementation;
static RAM delta versus Stage 6A is zero. Git whitespace checks and the exact
28-file staged inventory are final pre-commit gates. Local closure evidence is
ignored under `dist/stage6b1-closure-20261025/` and is not a build dependency.
No firmware upload, erase, push, merge, tag or release is authorized or performed.

Stage 6B.2 remains unimplemented: one WebSocket connection/epoch per browser tab,
strict v1 decoding and BigInt ordering, private confirmed state, hello/snapshot
readiness, ordered updates, bounded read-only state.request/result tracking,
reconnect/background recovery without replay and read-only live rendering.
It requires protocol-fixture tests and two-client hardware synchronization
acceptance. Fader/Blackout/preset mutations belong to later approved phases;
the Stage 6B.1 UI remains intentionally non-live with disabled mutation controls.
