"""Notice/distribution tests; synthetic images only, never a release build."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import distribution_notices as notices
import package_release_candidate as candidate


class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="synthetic-notices-", dir=ROOT / "dist")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "source"
        self.root.mkdir()
        notices.copy(ROOT, self.root)
        shutil.copyfile(ROOT / "platformio.ini", self.root / "platformio.ini")

    def test_exact_inventory_and_original_attribution(self):
        manifest = notices.verify(self.root)
        self.assertEqual(len(manifest["notices"]), 24)
        self.assertEqual(manifest["audited_firmware_commit"], "59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b")
        self.assertEqual(manifest["versions"]["M5GFX"], "0.2.28")
        text = notices.canonical_file(self.root / notices.FILES[-1])
        for row in manifest["notices"]:
            self.assertEqual(hashlib.sha256(text[row["offset"]:row["offset"] + row["bytes"]]).hexdigest(), row["sha256"])
        self.assertIn(b"Copyright (c) 2022 M5Stack", (self.root / "LICENSE").read_bytes())
        self.assertIn(b"Copyright (c) 2026 M5Stack CoreS3SE DMX Controller contributors", (self.root / "LICENSE").read_bytes())

    def test_missing_notice_files_rejected(self):
        for name in notices.FILES:
            path = self.root / name
            original = path.read_bytes()
            path.unlink()  # Disposable test fixture only.
            with self.assertRaises(OSError):
                notices.verify(self.root)
            path.write_bytes(original)

    def test_modified_text_and_removed_notice_rejected(self):
        path = self.root / notices.FILES[-1]
        original = path.read_bytes()
        path.write_bytes(original.replace(b"2022 M5Stack", b"2023 M5Stack", 1))
        with self.assertRaises(ValueError): notices.verify(self.root)
        path.write_bytes(original)
        manifest = json.loads((self.root / "licenses/manifest.json").read_text())
        manifest["notices"].pop()
        (self.root / "licenses/manifest.json").write_text(json.dumps(manifest))
        with self.assertRaises(ValueError): notices.verify(self.root)

    def test_missing_original_in_project_license_rejected(self):
        path = self.root / "LICENSE"
        path.write_bytes(path.read_bytes().replace(b"2022 M5Stack", b"2023 M5Stack", 1))
        with self.assertRaises(ValueError): notices.verify(self.root)

    def test_changed_dependency_requires_review(self):
        path = self.root / "platformio.ini"
        path.write_bytes(path.read_bytes().replace(b"M5GFX@0.2.28", b"M5GFX@0.2.29"))
        with self.assertRaisesRegex(ValueError, "dependency configuration changed"):
            notices.verify(self.root)

    def test_publication_documentation_and_local_links(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("completed in v0.3.0", readme)
        self.assertIn("before use in any untrusted environment", readme)
        self.assertIn("accessibility limitation", readme)
        self.assertIn("Separate Base migration", readme)
        for name in ["THIRD_PARTY_NOTICES.md", "docs/V0_3_0_PUBLICATION_READINESS.md",
                     "docs/V0_3_0_INSTALLATION.md", "docs/V0_3_0_RELEASE_NOTES_DRAFT.md"]:
            path = ROOT / name
            text = path.read_text(encoding="utf-8")
            self.assertIsNone(re.search(r"[A-Za-z]:[/\\]Users[/\\](?!<user>(?:[/\\]|$))[^/\\\s]+", text))
            for link in re.findall(r'\]\(([^)]+)\)', text):
                if ":" not in link and not link.startswith("#"):
                    self.assertTrue((path.parent / link.split("#", 1)[0]).is_file(), (name, link))

    def test_lf_crlf_have_identical_distribution_bytes(self):
        first = notices.file_manifest(self.root)
        for name in notices.FILES:
            path = self.root / name
            path.write_bytes(notices.canonical_file(path).replace(b"\n", b"\r\n"))
        self.assertEqual(notices.file_manifest(self.root), first)
        output = Path(self.temp.name) / "canonical"
        notices.copy(self.root, output)
        notices.verify_tree(output, ROOT)
        for name in notices.FILES:
            self.assertNotIn(b"\r", (output / name).read_bytes())

    def test_leading_line_and_block_source_notices(self):
        leading = "// Copyright Example\n\n// Licensed under Apache-2.0.\n\n"
        self.assertEqual(notices.leading_source_notice(leading + '#include "example.h"\n'), leading.rstrip("\n") + "\n")
        self.assertEqual(notices.leading_source_notice("/* MIT terms */\nint value;\n"), "/* MIT terms */\n")
        self.assertEqual(notices.leading_source_notice("#include <stdio.h>\n"), "")
        m = notices.verify(ROOT)
        row = next(r for r in m["notices"] if r["id"] == "arduino-source-notices")
        text = notices.canonical_file(ROOT / m["text_file"])[row["offset"]:row["offset"] + row["bytes"]]
        self.assertIn(b"libraries/Preferences/src/Preferences.cpp", text)
        self.assertIn(b"cores/esp32/esp32-hal-misc.c", text)

    def test_installed_evidence_rejects_modified_dependency(self):
        manifest = notices.verify(ROOT)
        packages = Path.home() / ".platformio/packages"
        originals = {"framework": packages / "framework-arduinoespressif32",
                     "toolchain": packages / "toolchain-xtensa-esp32s3"}
        originals.update({name: ROOT / ".pio/libdeps/cores3se_unit" / name for name in ("M5Unified", "M5GFX", "esp_dmx")})
        if not all(path.is_dir() for path in originals.values()):
            self.skipTest("installed package evidence requires a completed local build")
        fake = Path(self.temp.name) / "installed"
        for name, original in originals.items():
            metadata = "library.json" if name in ("M5Unified", "M5GFX", "esp_dmx") else "package.json"
            target = fake / name / metadata
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original / metadata, target)
        for row in manifest["installed_checks"]:
            target = fake / row["root"] / row["path"]
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(originals[row["root"]] / row["path"], target)
        notices.verify_installed(fake, fake / "framework", fake / "toolchain", self.root)
        (fake / "M5GFX/LICENSE").write_bytes(b"incorrect license\n")
        with self.assertRaisesRegex(ValueError, "installed notice/source differs"):
            notices.verify_installed(fake, fake / "framework", fake / "toolchain", self.root)

    def make_synthetic_bundle(self, profile, output):
        # No real firmware bytes are read/copied. Only the notice portion of the
        # actual creator is exercised; real image validation has its own tests.
        info = dict(version="0.3.0", target_release="0.3.0", source_commit="a" * 40,
                    source_modified=False, source_sha256="b" * 64, build_id="synthetic-test")
        build = self.root / ".pio/build" / profile
        build.mkdir(parents=True, exist_ok=True)
        components = [dict(file=name, offset=offset, size=4,
                           sha256=hashlib.sha256(b"TEST").hexdigest()) for name, offset in candidate.BINARIES.items()]
        receipt = dict(provenance=info, components=components, layout="cores3se-16m-ota6-v1",
                       flash_size=0x1000000, profile=profile)
        (build / "infrastructure.json").write_text(json.dumps(receipt))
        for name in (*candidate.DOCS, *candidate.RELEASE_DOCUMENTS):
            path = self.root / name
            path.parent.mkdir(exist_ok=True)
            path.write_text("Synthetic installation fixture. NOT INSTALLABLE.\n")
        def fake_package(root, environment, folder):
            self.assertEqual(environment, profile)
            folder.mkdir()
            for name in candidate.BINARIES: (folder / name).write_bytes(b"TEST")
        with patch.object(candidate, "identity", return_value=info), \
             patch.object(candidate, "build_source_digest", return_value="b" * 64), \
             patch.object(candidate, "source_patch", return_value=b""), \
             patch.object(candidate, "package", side_effect=fake_package), \
             patch.object(candidate, "validate_components"), \
             patch.object(notices, "verify_installed", return_value=notices.verify(self.root)):
            return candidate.create(self.root, profile, output, self.root, self.root)

    def test_both_profiles_and_deterministic_archives(self):
        for profile in candidate.PROFILES:
            a = self.make_synthetic_bundle(profile, Path(self.temp.name) / (profile + "-a"))
            b = self.make_synthetic_bundle(profile, Path(self.temp.name) / (profile + "-b"))
            self.assertEqual(a.read_bytes(), b.read_bytes())
            with zipfile.ZipFile(a) as archive:
                self.assertEqual(set(archive.namelist()), candidate.MEMBERS)
                for name in notices.FILES:
                    self.assertEqual(archive.read(name), notices.canonical_file(ROOT / name))
                manifest = json.loads(archive.read("manifest.json"))
                self.assertEqual(manifest["distribution_notices"], notices.file_manifest(ROOT))
                self.assertEqual(manifest["distribution_documentation"], candidate.documentation_manifest(self.root))
                self.assertIs(manifest["release_candidate"], True)
                for name in candidate.RELEASE_DOCUMENTS:
                    self.assertEqual(archive.read(name), notices.canonical_file(self.root / name))
                self.assertEqual({c["file"]: c["offset"] for c in manifest["components"]}, candidate.BINARIES)
                self.assertEqual(manifest["nvs"], dict(offset=0x9000, size=0x5000, erased_by_images=False))

    def test_missing_zip_notice_and_forged_notice_metadata_rejected(self):
        path = self.make_synthetic_bundle("cores3se_unit", Path(self.temp.name) / "bundle")
        folder = path.with_suffix("")
        with zipfile.ZipFile(path) as archive: original = {name: archive.read(name) for name in archive.namelist()}
        for missing in notices.FILES:
            bad = Path(self.temp.name) / "missing.zip"
            with zipfile.ZipFile(bad, "w") as archive:
                for name, data in original.items():
                    if name != missing: archive.writestr(name, data)
            with self.assertRaises(ValueError): candidate.verify_zip(bad, folder, self.root)
        manifest = json.loads(original["manifest.json"])
        manifest["distribution_notices"][0]["sha256"] = "0" * 64
        original["manifest.json"] = json.dumps(manifest).encode()
        (folder / "manifest.json").write_bytes(original["manifest.json"])
        bad = Path(self.temp.name) / "forged.zip"
        with zipfile.ZipFile(bad, "w") as archive:
            for name, data in original.items(): archive.writestr(name, data)
        with self.assertRaisesRegex(ValueError, "notice manifest"):
            candidate.verify_zip(bad, folder, self.root)


if __name__ == "__main__":
    unittest.main(verbosity=2)
