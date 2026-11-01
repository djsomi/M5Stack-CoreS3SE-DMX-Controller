"""Source-distribution coverage tests; no production files or hardware modified."""
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import source_material_inventory as inventory
import publication_materials as materials


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="material-notice-test-", dir=ROOT / "dist")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / inventory.DIRECTORY, self.root / inventory.DIRECTORY)
        shutil.copyfile(ROOT / "licenses/THIRD_PARTY_LICENSES.txt", self.root / "licenses/THIRD_PARTY_LICENSES.txt")

    def manifest(self):
        return json.loads((self.root / inventory.MANIFEST).read_text())

    def save(self, value):
        (self.root / inventory.MANIFEST).write_bytes(materials.dump(value))

    def test_reviewed_inventory_and_total_classification(self):
        m = inventory.verify_repository(self.root)
        self.assertEqual(len(m["components"]), 112)
        self.assertEqual(len(m["files"]), 2818)
        self.assertEqual(len(m["excluded_files"]), 1316)
        for row in m["files"]:
            self.assertIsNone(inventory.exclusion(row["path"]))
            self.assertEqual(row["component"], inventory.component(row["path"]))
            self.assertEqual(row["classification"], inventory.classification(row["path"]))
        for row in m["excluded_files"]:
            self.assertEqual(row["reason"], inventory.exclusion(row["path"]))

    def test_every_required_notice_cannot_be_removed(self):
        m = self.manifest()
        for row in m["notice_files"]:
            path = self.root / row["path"]
            original = path.read_bytes()
            path.unlink()
            with self.assertRaises(OSError): inventory.verify_repository(self.root)
            path.write_bytes(original)

    def test_changed_boost_and_fake_coverage_rejected(self):
        path = self.root / inventory.DIRECTORY / "texts/boost.txt"
        path.write_text("Boost license omitted")
        with self.assertRaisesRegex(ValueError, "missing/modified"):
            inventory.verify_repository(self.root)
        m = self.manifest()
        m["notice_files"] = [r for r in m["notice_files"] if r["id"] != "boost"]
        path.unlink(); self.save(m)
        with self.assertRaises(ValueError): inventory.verify_repository(self.root)

    def test_asio_declaration_not_relabelled_as_mit(self):
        m = self.manifest()
        row = next(r for r in m["components"] if r["id"] == "sdk-asio")
        self.assertIn("1.14.1", row["version"])
        row["license_expression"] = "MIT"
        self.save(m)
        with self.assertRaisesRegex(ValueError, "Asio BSL"):
            inventory.verify_repository(self.root)

    def test_unreviewed_and_duplicate_notices_rejected(self):
        m = self.manifest(); m["notice_files"].append(copy.deepcopy(m["notice_files"][0])); self.save(m)
        with self.assertRaises(ValueError): inventory.verify_repository(self.root)
        shutil.copyfile(ROOT / inventory.MANIFEST, self.root / inventory.MANIFEST)
        (self.root / inventory.DIRECTORY / "unexpected.txt").write_text("unreviewed")
        with self.assertRaises(ValueError): inventory.verify_repository(self.root)

    def test_source_payload_cannot_be_silently_omitted(self):
        with self.assertRaisesRegex(ValueError, "source-build payload"):
            inventory.verify_payload(self.root)

    def test_unchanged_recipe_and_selected_mode_policy(self):
        prefix = inventory.SDK
        self.assertIsNone(inventory.exclusion(prefix + "lib/libasio.a"))
        self.assertIsNone(inventory.exclusion(prefix + "qio_qspi/libfreertos.a"))
        self.assertIsNotNone(inventory.exclusion(prefix + "opi_opi/libfreertos.a"))
        self.assertIsNone(inventory.exclusion("libdeps/M5GFX/src/lgfx/Fonts/GFXFF/FreeSans9pt7b.h"))
        self.assertIsNotNone(inventory.exclusion("libdeps/M5GFX/src/picture_frame/frame_image_M5Tab5.h"))
        self.assertIsNotNone(inventory.exclusion(inventory.FRAMEWORK + "libraries/BLE/src/BLEDevice.cpp"))

    def test_fonts_and_codecs_have_distinct_terms(self):
        m = self.manifest(); c = {r["id"]: r for r in m["components"]}
        self.assertIn("GPL-3.0", c["gnu-freefont-bitmaps"]["license_expression"])
        self.assertIn("freefont-preferred-source", c["gnu-freefont-bitmaps"]["notice_ids"])
        self.assertEqual(c["ipa-font"]["license_expression"], "IPA")
        self.assertIn("ipa-font-license", c["ipa-font"]["notice_ids"])
        self.assertIn("efont-copyright", c["efont"]["notice_ids"])
        self.assertIn("libb64-dedication", c["libb64"]["notice_ids"])
        self.assertEqual(c["miniz"]["license_expression"], "Unlicense")
        self.assertEqual(c["tjpgdec"]["license_expression"], "LicenseRef-ChaN-TJpgDec")
        self.assertIn("nimble-port-notices", c["sdk-bt"]["notice_ids"])

    def test_inherited_license_must_reach_notice_archive(self):
        name = "libdeps/M5GFX/LICENSE"
        path = self.root / name
        path.parent.mkdir(parents=True)
        path.write_text("Synthetic inherited permission not reproduced elsewhere\n")
        m = self.manifest()
        m["files"] = [dict(path=name, bytes=path.stat().st_size,
            sha256=materials.sha(path.read_bytes()), component=inventory.component(name),
            classification=inventory.classification(name))]
        self.save(m)
        with self.assertRaisesRegex(ValueError, "standalone license absent"):
            inventory.verify_payload(self.root)


if __name__ == "__main__":
    unittest.main(verbosity=2)
