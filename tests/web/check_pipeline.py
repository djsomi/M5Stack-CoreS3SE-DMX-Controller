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
        path.write_bytes(path.read_bytes() + b"\n// changed source\n")
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
        self.assertEqual(len(actions), 5); self.assertTrue(all("disabled" in a for a in actions))
        slots = [a for _, a in elements.nodes if "data-preset" in a]; self.assertEqual(len(slots), 8)
        self.assertIn("NON-LIVE PREVIEW", html)
        for tag, attrs in elements.nodes:
            self.assertFalse(any(key.startswith("on") or key == "style" for key in attrs))
            for key in ("src", "href"):
                if key in attrs: self.assertTrue(attrs[key].startswith(("/", "#")), (tag, attrs))
        js = (self.root / "web/src/app.js").read_text(encoding='utf-8')
        import re
        self.assertFalse(re.search(r"\b(?:fetch|XMLHttpRequest|EventSource|sendBeacon|eval)\s*\(", js))
        self.assertNotRegex(js, r'"(?:channel\.set|blackout\.set|preset\.(?:load|save|copy|clear))"')
        self.assertIn('type: "state.request"', js)
        self.assertNotIn("setInterval(", js)

    def test_bank_arrow_touch_action_is_scoped_and_zoom_remains_available(self):
        css = (self.root / "web/src/style.css").read_text(encoding="utf-8")
        rules = re.findall(r"([^{}]+)\{([^{}]*touch-action\s*:[^{}]*)\}", css)
        self.assertEqual(len(rules), 1)
        selector, declarations = rules[0]
        self.assertEqual(selector.strip(), ".bank-buttons .icon-button")
        self.assertEqual(declarations.strip(), "touch-action: manipulation;")
        html = (self.root / "web/src/index.html").read_text(encoding="utf-8")
        controls = re.search(r'<div class="bank-buttons">(.*?)</div>', html).group(1)
        buttons = [attrs for tag, attrs in Elements(controls).nodes if tag == "button"]
        self.assertEqual(len(buttons), 2)
        self.assertEqual({attrs.get("id") for attrs in buttons}, {"bank-previous", "bank-next"})
        self.assertTrue(all(attrs.get("class") == "icon-button" for attrs in buttons))
        editor = (self.root / "web/src/js/editor.js").read_text(encoding="utf-8")
        self.assertNotRegex(editor, r"touchstart|touchend|touchmove|gesturestart|gesturechange")

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
        self.assertNotRegex(css, r"\d+dvh|touch-action:\s*none|scroll-behavior:\s*smooth")
        js = (self.root / "web/src/app.js").read_text(encoding="utf-8")
        self.assertNotIn("visualViewport", js)
        shell = (self.root / "web/src/js/shell.js").read_text(encoding="utf-8")
        self.assertNotRegex(shell, r"\b(?:scrollTo|scrollBy|setTimeout|requestAnimationFrame)\s*\(")
        html = gzip.decompress((self.root / "web/generated/index.html.gz").read_bytes()).decode("utf-8")
        viewport = next(attrs["content"] for tag, attrs in Elements(html).nodes
                        if tag == "meta" and attrs.get("name") == "viewport")
        self.assertIn("viewport-fit=cover", viewport)
        self.assertNotIn("maximum-scale", viewport)
        self.assertNotIn("user-scalable", viewport)


if __name__ == "__main__":
    (ROOT / "dist").mkdir(exist_ok=True)
    check()
    unittest.main()
