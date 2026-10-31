"""Host-only provenance/archive boundaries; no device, Git mutation or credentials."""
import ast
import copy
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests/network"))
import build_provenance as provenance
import package_release_candidate as candidate
import web_asset_pipeline as assets
from release_guard_support import prior


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="release-test-", dir=ROOT / "dist")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def test_single_version_source(self):
        self.assertEqual(provenance.versions(ROOT), ("0.3.0", "0.3.0"))
        with patch.object(provenance, "git", side_effect=[b"a"*40, b"", b""]):
            clean = provenance.identity(ROOT, "cores3se_unit", "b"*64)
        self.assertFalse(clean["source_modified"])
        self.assertEqual(clean["status"], "stable")
        with patch.object(provenance, "git", side_effect=[b"a"*40, b"src/a.cpp", b""]):
            dirty = provenance.identity(ROOT, "cores3se_unit", "b"*64)
        self.assertTrue(dirty["source_modified"])
        self.assertEqual(dirty["status"], "local-candidate")
        self.assertIn(".modified.", dirty["build_id"])
        self.assertEqual(dirty["source_commit"], "a"*40)

    def test_rc_identity_remains_candidate(self):
        with patch.object(provenance, "versions", return_value=("0.3.0-rc.1", "0.3.0")):
            with patch.object(provenance, "git", side_effect=[b"a"*40, b"", b""]):
                self.assertEqual(provenance.identity(ROOT, "cores3se_unit", "b"*64)["status"], "local-candidate")

    def test_untracked_build_input_and_unknown_revision(self):
        with patch.object(provenance, "git", side_effect=[b"a"*40, b"", b"include/new.h\0"]):
            self.assertTrue(provenance.identity(ROOT, "cores3se_unit", "b"*64)["source_modified"])
        with patch.object(provenance, "git", return_value=b"unknown"):
            with self.assertRaises(ValueError):
                provenance.identity(ROOT, "cores3se_unit", "b"*64)

    def test_info_is_generated_and_immutable(self):
        p = provenance.identity(ROOT, "cores3se_unit", "b"*64)
        cpp = self.folder / "assets.cpp"
        assets.emit_cpp(ROOT, cpp, "cores3se_unit", "b"*64, p)
        text = cpp.read_text()
        self.assertIn("const firmware_build::Info build_info", text)
        self.assertIn(p["source_commit"], text)
        self.assertIn(p["build_id"], text)
        self.assertNotIn("malloc(", text)
        first = cpp.read_bytes()
        assets.emit_cpp(ROOT, cpp, "cores3se_unit", "b"*64, p)
        self.assertEqual(cpp.read_bytes(), first)

    def test_readonly_status_and_diagnostic_delta(self):
        for name in ("src/network_service.cpp", "src/hardware_diagnostics.cpp", "scripts/embed_web_assets.py", "scripts/web_asset_pipeline.py"):
            text = (ROOT / name).read_text(encoding="utf-8")
            self.assertNotEqual(prior(name, text), text)
        net = (ROOT / "src/network_service.cpp").read_text(encoding="utf-8")
        block = net.split('int n=snprintf(json,sizeof(json),', 1)[1].split('identity.version', 1)[0]
        fmt = ''.join(ast.literal_eval(v) for v in re.findall(r'"(?:\\.|[^"\\])*"', block))
        values = ["0.3.0-rc.1", "a"*40, "true", "0.3.0-rc.1+"+"a"*12+".modified."+"b"*12,
                  "b"*64, "cores3se_unit", "cores3se-16m-ota6-v1", '"Synthetic AP"',
                  123456, "READY", "192.168.4.1", 4, 4, "false", 100, 2, 456, 0, "false", 4096, 2048]
        data = json.loads((fmt.replace('%lu', '%d') % tuple(values)) + ']}')
        self.assertEqual(data["source_commit"], "a"*40)
        self.assertTrue(data["source_modified"])
        self.assertEqual(data["version"], "0.3.0-rc.1")
        self.assertEqual(data["profile"], "cores3se_unit")
        self.assertLess(len(json.dumps(data)), 4096)

    def test_production_profiles_only(self):
        for env in ("cores3se_unit_diagnostics", "cores3se_base_diagnostics", "other"):
            with self.assertRaises(ValueError):
                candidate.create(ROOT, env, self.folder, self.folder, self.folder)
        self.assertEqual(set(candidate.PROFILES), {"cores3se_unit", "cores3se_base"})

    def test_sector_ranges_and_expected_image_set(self):
        manifest = dict(profile="cores3se_unit", diagnostics=False, layout="cores3se-16m-ota6-v1",
                        flash_size=0x1000000, components=[])
        with self.assertRaises(ValueError):
            candidate.validate_components(self.folder, manifest, self.folder, self.folder)
        for name, offset in candidate.BINARIES.items():
            data = b"test"; (self.folder / name).write_bytes(data)
            manifest["components"].append(dict(file=name, offset=offset, size=4,
                sha256=hashlib.sha256(data).hexdigest(), erase_range=[offset, offset+4096]))
        bad = copy.deepcopy(manifest);bad["components"][0]["offset"] = 0x9000
        with self.assertRaises(ValueError):
            candidate.validate_components(self.folder, bad, self.folder, self.folder)
        bad = copy.deepcopy(manifest);bad["components"][0]["erase_range"] = [0, 0x10000]
        with self.assertRaises(ValueError):
            candidate.validate_components(self.folder, bad, self.folder, self.folder)

    def make_zip(self, extra=False, wrong_hash=False):
        for name in candidate.MEMBERS - {"SHA256SUMS.txt"}:
            (self.folder / name).write_bytes(name.encode())
        sums = ''.join(hashlib.sha256((self.folder / name).read_bytes()).hexdigest()+"  "+name+"\n"
                       for name in sorted(candidate.MEMBERS - {"SHA256SUMS.txt"}))
        if wrong_hash:
            sums = '0'*64 + sums[64:]
        (self.folder / "SHA256SUMS.txt").write_text(sums, encoding="ascii")
        zpath = self.folder / "test.zip"
        with zipfile.ZipFile(zpath, "w") as z:
            for name in candidate.MEMBERS:
                z.writestr(name, (self.folder / name).read_bytes())
            if extra:
                z.writestr("NVS.bin", b"not allowed")
        return zpath

    def test_zip_members_and_every_internal_checksum(self):
        candidate.verify_zip(self.make_zip(), self.folder)

    def test_extra_private_or_full_image_rejected(self):
        with self.assertRaises(ValueError):
            candidate.verify_zip(self.make_zip(extra=True), self.folder)

    def test_bad_checksum_rejected(self):
        with self.assertRaises(ValueError):
            candidate.verify_zip(self.make_zip(wrong_hash=True), self.folder)

    def test_manifest_and_source_input_paths(self):
        data = provenance.input_manifest(ROOT)
        paths = [v["path"] for v in data["files"]]
        self.assertIn("include/firmware_version.h", paths)
        self.assertFalse(any("dist/" in p or "_kezdeti" in p or "backup" in p for p in paths))
        self.assertEqual(len(paths), len(set(paths)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
