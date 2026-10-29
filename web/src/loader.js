// Core delivery template; factories are injected by scripts/web_ui_bundle.py.
(function () {
"use strict";
if (window.__CoreS3SEBoot) return;
/* CORE_FACTORIES */
const core = Object.freeze({/* CORE_EXPORTS */});
let mounted = false, failed = false, timer = null;
function fail() {
  if (failed) return;
  failed = true; window.clearTimeout(timer);
  const notice = document.getElementById("loading-message");
  notice.textContent = "Interface could not load. Reload; if it persists, clear the browser cache.";
  notice.hidden = false;
  document.documentElement.dataset.uiReady = "failed";
  document.getElementById("workspace").inert = true;
  const blackout = document.getElementById("blackout");
  blackout.disabled = true; blackout.inert = true;
}
// One page-lifetime entry point; no Controller instance or mutable universe is global.
Object.defineProperty(window, "__CoreS3SEBoot", {value: Object.freeze({
  mount(create) {
    if (mounted || failed) return;
    mounted = true;
    try { create(core).start(); window.clearTimeout(timer); }
    catch (_) { fail(); }
  }
})});
try {
  const source = document.getElementById("core-script");
  if (!source?.dataset.uiSrc || !source.dataset.uiIntegrity) { fail(); return; }
  const script = document.createElement("script");
  script.src = source.dataset.uiSrc;
  script.integrity = source.dataset.uiIntegrity;
  script.async = false;
  script.onerror = fail;
  script.onload = () => { if (!mounted) fail(); };
  timer = window.setTimeout(fail, 10000);
  // Created only after Core has executed: UI cannot fetch in parallel with Core.
  document.head.append(script);
} catch (_) { fail(); }
})();
