"""Stage 4B format, reproducibility and failure tests; no device access."""
import copy
import gzip
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
import web_asset_pipeline as assets
import firmware_layout as layout
from package_firmware import package

CORE = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
FRAMEWORK = CORE / "packages/framework-arduinoespressif32"


class InfrastructureTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "dist").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="infrastructure-", dir=ROOT / "dist")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "web", self.root / "web")
        shutil.copy2(ROOT / "platformio.ini", self.root / "platformio.ini")

    def test_gzip_reproducible_and_round_trip(self):
        one, two = self.root / "one", self.root / "two"
        assets.generate(self.root, one); assets.generate(self.root, two)
        self.assertEqual({p.name: p.read_bytes() for p in one.iterdir()},
                         {p.name: p.read_bytes() for p in two.iterdir()})
        self.assertEqual((one / "manifest.json").read_bytes(), (self.root / "web/generated/manifest.json").read_bytes())
        manifest = assets.verify(self.root)
        for entry in manifest["assets"]:
            data = (one / entry["file"]).read_bytes()
            self.assertEqual(len(data), entry["gzip_size"])
            self.assertEqual(len(gzip.decompress(data)), entry["original_size"])
            self.assertEqual(data[3:8], b"\x00" * 5)

    def test_missing_invalid_and_stale_inputs_rejected(self):
        path = self.root / "web/src/app.js"
        original = path.read_bytes()
        for invalid in (b"", b"\xff", b"\x00"):
            path.write_bytes(invalid)
            with self.assertRaises((ValueError, UnicodeError)):
                assets.verify(self.root)
        path.write_bytes(original + b"// stale\n")
        with self.assertRaises(ValueError):
            assets.verify(self.root)
        path.unlink()
        with self.assertRaises(ValueError):
            assets.verify(self.root)

    def test_manifest_and_corruption_rejected(self):
        path = self.root / "web/generated/manifest.json"
        original = json.loads(path.read_text())
        for key, bad in (("gzip_size", 999), ("content_type", "wrong"), ("content_encoding", "identity"),
                         ("original_size", 0), ("path", "/wrong")):
            data = copy.deepcopy(original); data["assets"][0][key] = bad
            data["bundle_sha256"] = assets.sha(assets.canonical(data["assets"]))
            path.write_bytes(assets.canonical(data))
            with self.assertRaises(ValueError):
                assets.verify(self.root)
        path.write_bytes(assets.canonical(original))
        blob = self.root / "web/generated/index.html.gz"
        blob.write_bytes(blob.read_bytes()[:-1])
        with self.assertRaises((ValueError, EOFError)):
            assets.verify(self.root)

    def test_crlf_sources_and_generated_cpp(self):
        source = self.root / "web/src/style.css"
        source.write_bytes(source.read_bytes().replace(b"\n", b"\r\n"))
        manifest = assets.verify(self.root)
        cpp = self.root / "generated/web_assets.cpp"
        digest = assets.build_source_digest(self.root)
        assets.emit_cpp(self.root, cpp, "cores3se_unit", digest)
        first = cpp.read_bytes()
        assets.emit_cpp(self.root, cpp, "cores3se_unit", digest)
        self.assertEqual(first, cpp.read_bytes())
        self.assertIn(manifest["bundle_sha256"].encode(), first)
        self.assertNotIn(b"malloc(", first)

    def test_approved_partition_csv_binary_and_negative_cases(self):
        sdk = layout.partition_sdk(FRAMEWORK)
        table = sdk.PartitionTable.from_csv((ROOT / "partitions/cores3se_16mb_ota.csv").read_text())
        layout.validate_table(table)
        layout.validate_table(sdk.PartitionTable.from_binary(table.to_binary()))
        self.assertEqual(len(table.to_binary()), 3072)
        self.assertEqual(table[2].size, 6 * 1024 * 1024)
        for index, field, value in ((2, "offset", 0x11000), (3, "offset", 0x600000),
                                    (3, "size", 0x610000), (0, "offset", 0x8000),
                                    (5, "size", 0x20000), (1, "size", 0x1000)):
            bad = copy.deepcopy(table); setattr(bad[index], field, value)
            with self.assertRaises((ValueError, sdk.InputError)):
                layout.validate_table(bad)
        bad_binary = bytearray(table.to_binary()); bad_binary[8] ^= 1
        with self.assertRaises(sdk.InputError):
            sdk.PartitionTable.from_binary(bytes(bad_binary))

    def test_packaging_separate_images_and_stale_rejection(self):
        environment = "cores3se_unit"
        build = self.root / ".pio/build" / environment
        build.mkdir(parents=True)
        components = []
        for name, offset in (("bootloader.bin", 0), ("partitions.bin", 0x8000),
                             ("boot_app0.bin", 0xe000), ("firmware.bin", 0x10000)):
            data = name.encode(); (build / name).write_bytes(data)
            components.append(dict(file=name, offset=offset, size=len(data), sha256=assets.sha(data)))
        receipt = dict(profile=environment, source_sha256=assets.build_source_digest(self.root),
                       assets_sha256=assets.verify(self.root)["bundle_sha256"],
                       packaging="separate-images-only", components=components)
        layout.write_json(build / "infrastructure.json", receipt)
        package(self.root, environment, self.root / "bundle")
        self.assertEqual(set(p.name for p in (self.root / "bundle").iterdir()),
                         {c["file"] for c in components} | {"manifest.json", "SHA256SUMS.txt"})
        with self.assertRaises(FileExistsError):
            package(self.root, environment, self.root / "bundle")
        (build / "firmware.bin").write_bytes(b"modified")
        with self.assertRaises(ValueError):
            package(self.root, environment, self.root / "corrupt-bundle")
        (self.root / "web/src/app.js").write_bytes(b"// changed\n")
        with self.assertRaises(ValueError):
            package(self.root, environment, self.root / "stale-bundle")


if __name__ == "__main__":
    unittest.main(verbosity=2)
