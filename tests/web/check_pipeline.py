"""Actual composed/served-output tests; no device access or package dependencies."""
from pathlib import Path
from html.parser import HTMLParser
import gzip
import hashlib
import json
import random
import re
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import web_asset_pipeline as assets
import web_ui_bundle as bundle
from check_scope import check


class Elements(HTMLParser):
    def __init__(self, text):
        super().__init__(); self.nodes = []; self.feed(text)
    def handle_starttag(self, tag, attrs):
        self.nodes.append((tag, dict(attrs)))


class PipelineTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="web-ui-test-", dir=ROOT / "dist")
        self.addCleanup(folder.cleanup); self.root = Path(folder.name)
        shutil.copytree(ROOT / "web", self.root / "web")

    def test_deterministic_order_and_normalization(self):
        first = bundle.compose(self.root)
        second = bundle.compose(self.root)
        self.assertEqual(first, second)
        self.assertEqual(first, (self.root / "web/src/app.js").read_bytes().replace(b"\r\n", b"\n"))
        positions = [first.index(("const " + name + " = (").encode()) for name, _ in bundle.MODULES]
        self.assertEqual(positions, sorted(positions))
        path = self.root / "web/src/js/preferences.js"
        path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.assertEqual(first, bundle.compose(self.root))
        a, b = self.root / "a", self.root / "b"
        assets.generate(self.root, a); assets.generate(self.root, b)
        self.assertEqual({p.name: p.read_bytes() for p in a.iterdir()}, {p.name: p.read_bytes() for p in b.iterdir()})

    def test_missing_duplicate_reordered_and_unlisted(self):
        with self.assertRaises(ValueError): bundle.compose(self.root, bundle.MODULES + (bundle.MODULES[0],))
        with self.assertRaises(ValueError): bundle.compose(self.root, tuple(reversed(bundle.MODULES)))
        module = self.root / "web/src/js/fader.js"; original = module.read_bytes(); module.unlink()
        with self.assertRaises(OSError): bundle.compose(self.root)
        module.write_bytes(original)
        (self.root / "web/src/js/stray.js").write_text("() => ({})")
        with self.assertRaises(ValueError): bundle.compose(self.root)

    def test_dependency_declaration(self):
        module = self.root / "web/src/js/editor.js"
        module.write_text(module.read_text(encoding='utf-8').replace("requires: fader", "requires: missing"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "dependency"): bundle.compose(self.root)

    def test_stale_module_bundle_and_read_only_verification(self):
        path = self.root / "web/src/js/state.js"
        # Change source without adding prose to a nearly full compressed asset.
        # This fixture tests stale detection; size-limit rejection has its own test.
        original = path.read_bytes()
        self.assertEqual(original.count(b"  function create()"), 1)
        path.write_bytes(original.replace(b"  function create()", b" function create()", 1))
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        with self.assertRaisesRegex(ValueError, "stale UI bundle"): assets.verify(self.root)
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob("*") if p.is_file()})
        old = (self.root / "web/generated/manifest.json").read_bytes()
        assets.generate(self.root); assets.verify(self.root)
        self.assertNotEqual(old, (self.root / "web/generated/manifest.json").read_bytes())

    def test_gzip_limit_generation_and_verification(self):
        module = self.root / "web/src/js/fader.js"
        random_text = "".join(random.Random(7).choices("abcdefghijklmnopqrstuvwxyz0123456789", k=40000))
        module.write_text(module.read_text(encoding='utf-8') + "\n// " + random_text + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "16 KiB"): assets.generate(self.root)
        target = (self.root / "web").resolve()
        assert target.is_relative_to((ROOT / "dist").resolve())
        shutil.rmtree(target); shutil.copytree(ROOT / "web", self.root / "web")
        (self.root / "web/generated/style.css.gz").write_bytes(b"x" * 16385)
        with self.assertRaisesRegex(ValueError, "16 KiB"): assets.verify(self.root)

    def test_manifest_actual_bodies_corruption_missing(self):
        manifest = assets.verify(self.root)
        self.assertEqual(len(manifest["assets"]), 3)
        for entry in manifest["assets"]:
            data = (self.root / "web/generated" / entry["file"]).read_bytes()
            self.assertLessEqual(len(data), 16384)
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"])
            self.assertEqual(len(gzip.decompress(data)), entry["original_size"])
        path = self.root / "web/generated/app.js.gz"; data = path.read_bytes()
        path.write_bytes(data[:-1])
        with self.assertRaises(ValueError): assets.verify(self.root)
        path.unlink()
        with self.assertRaises(OSError): assets.verify(self.root)

    def test_served_markup_safety_and_navigation(self):
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode()
        elements = Elements(html); ids = [a["id"] for _, a in elements.nodes if "id" in a]
        self.assertEqual(len(ids), len(set(ids)))
        links = [a for t, a in elements.nodes if t == "a" and "data-section" in a]
        self.assertEqual([a["href"] for a in links], ["#editor", "#presets", "#settings", "#status"])
        for name in ("editor", "presets", "settings", "status"):
            self.assertIn("view-" + name, ids); self.assertIn("heading-" + name, ids)
        actions = [a for _, a in elements.nodes if "data-device-action" in a or a.get("id") == "blackout"]
        self.assertEqual(len(actions), 4); self.assertTrue(all("disabled" in a for a in actions))
        slots = [a for _, a in elements.nodes if "data-preset" in a]; self.assertEqual(len(slots), 8)
        self.assertNotIn("preview-note", html)
        self.assertNotIn("monitor-tag", html)
        self.assertNotIn("LIVE CONTROL", html)
        for tag, attrs in elements.nodes:
            self.assertFalse(any(key.startswith("on") or key == "style" for key in attrs))
            for key in ("src", "href"):
                if key in attrs: self.assertTrue(attrs[key].startswith(("/", "#")), (tag, attrs))
        js = (self.root / "web/src/app.js").read_text(encoding='utf-8')
        import re
        self.assertFalse(re.search(r"\b(?:XMLHttpRequest|EventSource|sendBeacon|eval)\s*\(", js))
        self.assertEqual(len(re.findall(r"\bfetch\s*\(", js)), 1)
        telemetry = (self.root / "web/src/js/status.js").read_text(encoding="utf-8")
        self.assertIn('["/status", "/ws-diagnostics"]', telemetry)
        self.assertIn('redirect: "error"', telemetry)
        self.assertIn('cache: "no-store"', telemetry)
        self.assertNotIn("localStorage", telemetry)
        self.assertNotRegex(js, r'"(?:preset\.copy|preset\.rename|scene\.play)"')
        self.assertIn('type: "state.request"', js)
        self.assertIn('type: "channel.set"', js)
        self.assertNotIn("setInterval(", js)

    def test_bank_arrow_touch_action_is_scoped_and_zoom_remains_available(self):
        css = (self.root / "web/src/style.css").read_text(encoding="utf-8")
        rules = re.findall(r"([^{}]+)\{([^{}]*touch-action\s*:[^{}]*)\}", re.sub(r"/\*.*?\*/", "", css, flags=re.S))
        self.assertEqual(len(rules), 5)
        self.assertEqual({selector.strip() for selector, _ in rules}, {".bank-buttons .icon-button", ".fader-step, .fader-value", ".preset-actions button, .preset-slot, .blackout-button", ".fader input[type=range]", ".app-container, .app-container *"})
        for selector, declarations in rules:
            if selector.strip() == ".fader input[type=range]":
                self.assertIn("touch-action: none;", declarations)
            else:
                self.assertEqual(declarations.strip(), "touch-action: pan-x pan-y;")
        html = (self.root / "web/src/index.html").read_text(encoding="utf-8")
        controls = re.search(r'<div class="bank-buttons">(.*?)</div>', html).group(1)
        buttons = [attrs for tag, attrs in Elements(controls).nodes if tag == "button"]
        self.assertEqual(len(buttons), 2)
        self.assertEqual({attrs.get("id") for attrs in buttons}, {"bank-previous", "bank-next"})
        self.assertTrue(all(attrs.get("class") == "icon-button" for attrs in buttons))
        editor = (self.root / "web/src/js/editor.js").read_text(encoding="utf-8")
        self.assertNotRegex(editor, r"touchstart|touchend|touchmove|gesturestart|gesturechange")

    def test_served_selection_policy_preserves_editing_and_diagnostic_copy(self):
        css = gzip.decompress((self.root / "web/generated/style.css.gz").read_bytes()).decode("utf-8")
        css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
        body = re.search(r"(?:^|\n)body\s*\{([^}]*)\}", css).group(1)
        for declaration in ("-webkit-user-select: none;", "user-select: none;"):
            self.assertIn(declaration, body)
        exceptions = re.search(r'input, textarea, \[contenteditable\]:not\(\[contenteditable="false"\]\), \.status-panel dd, #connection-error\s*\{([^}]*)\}', css).group(1)
        self.assertIn("-webkit-user-select: text;", exceptions)
        self.assertIn("user-select: text;", exceptions)
        self.assertLess(css.index("#connection-error"), css.index(".fader input[type=range] {"))
        slider = re.search(r"\.fader input\[type=range\]\s*\{([^}]*)\}", css).group(1)
        self.assertIn("-webkit-user-select: none;", slider)
        self.assertIn("user-select: none;", slider)
        self.assertNotIn("touch-callout", css)  # No evidence a callout restriction is needed.
        self.assertNotRegex(css, r"pointer-events:\s*none")
        self.assertEqual(len(re.findall(r"touch-action:\s*none", css)), 1)
        self.assertRegex(css, r"\.fader input\[type=range\]\s*\{[^}]*touch-action: none;")
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        nodes = Elements(html).nodes
        self.assertTrue(any(tag == "input" and attrs.get("id") == "channel-jump" for tag, attrs in nodes))
        self.assertIn('<div class="panel status-panel">', html)
        self.assertTrue(any(tag == "dd" and attrs.get("id") == "status-revision" for tag, attrs in nodes))
        self.assertTrue(any(attrs.get("id") == "connection-error" for tag, attrs in nodes))
        js = (self.root / "web/src/js/fader.js").read_text(encoding="utf-8")
        self.assertNotRegex(js, r"touchstart|touchmove|touchend|gesturestart")
        self.assertEqual(js.count("event.preventDefault()"), 2)  # Only form submit and Escape.
        self.assertIn('form.addEventListener("submit"', js)
        self.assertIn('event.key === "Escape"', js)
        self.assertNotRegex(js, r"setTimeout|setInterval|touchstart|touchmove")

    def test_compact_numeric_and_step_layout_contract(self):
        css = gzip.decompress((self.root / "web/generated/style.css.gz").read_bytes()).decode("utf-8")
        self.assertNotIn(".preview-note", css)
        self.assertRegex(css, r"\.fader-step\s*\{[^}]*width: var\(--fader-control-width\);[^}]*min-height: 44px;")
        self.assertRegex(css, r"\.fader-plus\s*\{\s*order: -1;")
        self.assertRegex(css, r"\.fader-minus\s*\{\s*order: 1;")
        self.assertRegex(css, r"\.value-entry \.dmx-draft-input\s*\{[^}]*font-size: 18px;")
        self.assertEqual(css.count("height: calc(var(--fader-travel) + 104px)"), 2)
        self.assertRegex(css, r"\.value-error\s*\{[^}]*max-width: 100%;[^}]*overflow-wrap: anywhere;")
        self.assertIn("grid-template-columns: repeat(4, minmax(0, 1fr))", css)
        self.assertIn("grid-template-columns: repeat(6, minmax(0, 1fr))", css)
        self.assertIn("grid-template-columns: repeat(8, minmax(0, 1fr))", css)
        # CSS budget at the 280px supported minimum: no 44px target needs overflow.
        for width, columns, padding, grid_padding, gap in [(280,4,32,24,8),(320,4,32,24,8),(600,6,32,24,8),(768,8,56,44,12)]:
            card_width = (width - padding - grid_padding - 2 - gap * (columns-1)) / columns
            self.assertGreaterEqual(card_width, 44)
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        nodes = Elements(html).nodes
        ids = {attrs["id"] for _,attrs in nodes if "id" in attrs}
        for _,attrs in nodes:
            for key in ("aria-describedby", "aria-labelledby"):
                for target in attrs.get(key, "").split(): self.assertIn(target, ids)

    def test_preset_blackout_actions_remain_bounded_and_initially_disabled(self):
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        nodes = Elements(html).nodes
        by_id = {attrs["id"]:(tag,attrs) for tag,attrs in nodes if "id" in attrs}
        for name in ("preset-load", "preset-save", "preset-clear", "blackout"):
            self.assertIn("disabled", by_id[name][1])
        self.assertIn("hidden", by_id["preset-confirmation"][1])
        self.assertIn("preset-confirm", by_id); self.assertIn("preset-cancel", by_id)
        self.assertNotIn('data-device-action="preset.copy"', html)
        self.assertIn('role="status"', html)
        commands = (self.root / "web/src/js/commands.js").read_text(encoding="utf-8")
        for operation in ("preset.load", "preset.save", "preset.clear", "blackout.set"):
            self.assertIn(operation, commands)
        self.assertNotIn("preset.rename", commands)
        self.assertNotIn("preset.delete", commands)
        actions = (self.root / "web/src/js/actions.js").read_text(encoding="utf-8")
        self.assertNotRegex(actions, r"localStorage|fetch\(|setInterval|window.confirm|prompt\(")

    def test_polished_header_faders_footer_and_status_layout(self):
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        css = gzip.decompress((self.root / "web/generated/style.css.gz").read_bytes()).decode("utf-8")
        self.assertEqual(html.count('id="connection-label"'), 1)
        self.assertIn('role="status" aria-labelledby="connection-label"', html)
        self.assertNotIn("connection-compact", html + css)
        mobile = css[css.index("@media (max-width: 479px)"):css.index("@media (min-width: 480px)")]
        self.assertIn("grid-template-columns: minmax(0, 1fr) 100px", mobile)
        self.assertRegex(mobile, r"\.connection\s*\{[^}]*grid-row: 2;[^}]*white-space: nowrap;")
        self.assertNotIn("overflow-wrap: anywhere", mobile)
        self.assertNotRegex(mobile, r"\.brand(?:-mark|-name|-caption)?\s*\{[^}]*(?:font-size|width|display)")
        self.assertIn("width: 33px; height: 33px", css)
        self.assertIn("font-size: 19px", css)
        self.assertIn("--fader-control-width: 44px", css)
        self.assertIn("width: min(calc(var(--fader-control-width) + 10px), 100%)", css)
        self.assertNotIn("--thumb-width", css)
        for vendor in ("webkit-slider-thumb", "moz-range-thumb"):
            self.assertRegex(css, r"::-" + vendor + r"\s*\{[^}]*box-sizing: border-box;[^}]*width: var\(--fader-control-width\);[^}]*height: 18px;")
        self.assertRegex(mobile, r"\.view\s*\{\s*margin-top: 4px;")
        self.assertRegex(css, r"@media \(hover: hover\) and \(pointer: fine\)")
        self.assertIn("button:hover:not(:disabled):not(.preset-slot)", css)
        self.assertRegex(css, r"\.status-grid\s*\{[^}]*align-items: start;")
        self.assertNotRegex(css, r"\.status-grid\s*\{[^}]*repeat\(2")
        self.assertRegex(css, r"details\.status-panel\s*\{\s*margin-top: 16px;")
        self.assertIn("System &amp; Firmware", html)
        self.assertIn("Memory &amp; Resources", html)
        self.assertNotRegex(html, r"<details[^>]*\bopen\b")
        self.assertNotIn("flex-direction: column; gap: 8px", css)
        self.assertRegex(css, r"\.app-footer\s*\{[^}]*justify-content: flex-start;[^}]*white-space: nowrap;")
        app = (self.root / "web/src/js/app.js").read_text(encoding="utf-8")
        self.assertNotIn("Controller working values", app)
        self.assertIn('operation.last.message', app)

    def test_fixed_scale_application_policy_preserves_scroll_and_lower_blackout(self):
        css = gzip.decompress((self.root / "web/generated/style.css.gz").read_bytes()).decode("utf-8")
        self.assertRegex(css, r"\.app-container, \.app-container \*\s*\{\s*touch-action: pan-x pan-y;")
        self.assertEqual(len(re.findall(r"touch-action:\s*none", css)), 1)
        self.assertRegex(css, r"\.fader input\[type=range\]\s*\{[^}]*touch-action: none;")
        html = (self.root / "web/src/index.html").read_text(encoding="utf-8")
        self.assertIn('class="blackout-summary"', html)
        self.assertIn('id="blackout-summary-label"', html)
        self.assertIn("maximum-scale=1,user-scalable=no", html)
        for name in ("app", "presets", "fader", "shell"):
            js = (self.root / ("web/src/js/" + name + ".js")).read_text(encoding="utf-8")
            self.assertNotRegex(js, r"addEventListener\(\s*[\"'](?:touchstart|touchmove|touchend|dblclick)[\"']")

    def test_served_mobile_viewport_contract(self):
        # Test shipped bytes, not merely the presence of an authored filename.
        import re
        css = gzip.decompress((self.root / "web/generated/style.css.gz").read_bytes()).decode("utf-8")
        css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
        start = css.index("@supports (height: 100svh)")
        end = css.index("@media (max-width: 479px)", start)
        mobile = css[start:end]
        self.assertIn("@media (max-width: 679px)", mobile)
        self.assertRegex(mobile, r"html\s*\{[^}]*height:\s*100%;[^}]*overflow:\s*hidden;")
        self.assertRegex(mobile, r"body\s*\{[^}]*position:\s*relative;[^}]*height:\s*100svh;")
        container = re.search(r"\.app-container\s*\{([^}]*)\}", mobile).group(1)
        self.assertIn("height: 100%", container)
        self.assertIn("min-height: 0", container)
        self.assertIn("overflow-y: auto", container)
        self.assertNotRegex(container, r"(?:position|transform|contain)\s*:")
        self.assertRegex(mobile, r"\.section-nav\s*\{\s*position:\s*absolute;\s*\}")
        self.assertIn("position: fixed", css[:start])  # Unsupported-svh fallback.
        self.assertIn("env(safe-area-inset-bottom)", css[:start])
        self.assertIn("calc(100px + env(safe-area-inset-bottom))", css[:start])
        self.assertNotRegex(css, r"\d+dvh|scroll-behavior:\s*smooth")
        js = (self.root / "web/src/app.js").read_text(encoding="utf-8")
        self.assertNotIn("visualViewport", js)
        shell = (self.root / "web/src/js/shell.js").read_text(encoding="utf-8")
        self.assertNotRegex(shell, r"\b(?:scrollTo|scrollBy|setTimeout|requestAnimationFrame)\s*\(")
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        viewport = next(attrs["content"] for tag, attrs in Elements(html).nodes
                        if tag == "meta" and attrs.get("name") == "viewport")
        self.assertIn("viewport-fit=cover", viewport)
        self.assertIn("maximum-scale=1", viewport)
        self.assertIn("user-scalable=no", viewport)


if __name__ == "__main__":
    (ROOT / "dist").mkdir(exist_ok=True)
    check()
    unittest.main()
