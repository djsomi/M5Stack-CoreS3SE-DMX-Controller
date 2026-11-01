"""Publication-only safety tests. Real frozen Git reads, disposable reconstructions."""
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import distribution_notices as notices
import frozen_source as frozen
import publication_materials as materials
import publication_release as publication
from web_asset_pipeline import build_source_digest


class PublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Keep generated Git fixtures under ignored dist for inspection; Git on
        # Windows can retain directory handles beyond subprocess completion.
        cls.directory = Path(tempfile.mkdtemp(prefix="publication-test-", dir=ROOT / "dist"))
        cls.snapshot = cls.directory / "snapshot"
        cls.manifest = frozen.create(ROOT, cls.snapshot)
        cls.restored = cls.directory / "frozen"
        frozen.restore(cls.snapshot, cls.restored)

    def test_frozen_fingerprint_exact(self):
        self.assertEqual(build_source_digest(self.restored), frozen.SOURCE_SHA256)
        self.assertEqual(frozen.git(self.restored, "rev-parse", "HEAD").decode().strip(), frozen.COMMIT)
        self.assertEqual(frozen.git(self.restored, "status", "--porcelain"), b"")

    def test_only_minimum_history_refs_and_source_blobs(self):
        objects = frozen.batch(self.restored, self.manifest["objects"])
        self.assertEqual(sum(t == "commit" for t, _ in objects.values()), 1)
        self.assertEqual(sum(t == "tag" for t, _ in objects.values()), 1)
        self.assertEqual(frozen.git(self.restored, "show-ref").decode().strip(), frozen.TAG_OBJECT + " refs/tags/v0.3.0")
        self.assertEqual({oid for oid, (t, _) in objects.items() if t == "blob"},
                         {row["git_blob"] for row in self.manifest["files"]})
        for row in self.manifest["files"]:
            frozen.source_path(row["path"])
            self.assertFalse(row["path"].startswith(("docs/", "dist/", "tests/")))
        self.assertFalse((self.restored / ".git/logs/refs/stash").exists())
        self.assertEqual(frozen.git(self.restored, "remote").strip(), b"omitted")
        config = frozen.git(self.restored, "config", "--local", "--list").decode()
        self.assertNotIn(".url=", config)
        self.assertIn("remote.omitted.promisor=true", config)

    def bad_snapshot(self, name, mutate):
        p = self.directory / name
        shutil.copytree(self.snapshot, p)
        m = copy.deepcopy(self.manifest)
        mutate(m, p)
        (p / "manifest.json").write_bytes(materials.dump(m))
        with self.assertRaises((ValueError, OSError)):
            frozen.restore(p, self.directory / (name + "-restored"))

    def test_corrupt_pack_rejected(self):
        self.bad_snapshot("corrupt", lambda m, p: (p / "objects.pack").write_bytes(b"bad"))

    def test_wrong_commit_tag_or_fingerprint_rejected(self):
        for field in ("commit", "tag_object", "source_sha256"):
            self.bad_snapshot("wrong-" + field, lambda m, p, key=field: m.update({key: "0" * len(m[key])}))

    def test_private_path_and_duplicate_manifest_rejected(self):
        self.bad_snapshot("private-path", lambda m, p: m["files"][0].update(path="docs/private.txt"))
        self.bad_snapshot("duplicate-path", lambda m, p: m["files"].append(m["files"][0]))

    def test_file_hash_and_immutable_tree_mismatch_rejected(self):
        self.bad_snapshot("bad-sha", lambda m, p: m["files"][0].update(sha256="0" * 64))
        self.bad_snapshot("bad-tree", lambda m, p: m["files"][0].update(path="src/extra.cpp"))

    def test_unlisted_history_object_rejected(self):
        parent = frozen.git(ROOT, "rev-parse", frozen.COMMIT + "^").decode().strip()
        def extra(m, p):
            objects = sorted(set(m["objects"]) | {parent})
            pack = frozen.git(ROOT, "pack-objects", "--stdout", data="".join(o + "\n" for o in objects).encode())
            (p / "objects.pack").write_bytes(pack)
            m["pack_sha256"] = materials.sha(pack)
        self.bad_snapshot("extra-history", extra)

    def test_paths_and_symlink_boundary(self):
        for path in ("/etc/passwd", "../secret", "src/../private", "src/.git/config", "C:/private", "src\\private"):
            with self.assertRaises(ValueError): frozen.source_path(path)
        for path in ("/absolute", "../parent", "core//file", "core/.git/HEAD", "C:secret", "core\\secret"):
            with self.assertRaises(ValueError): materials.safe_path(path)

    def fixture(self, name):
        p = self.directory / name
        p.mkdir()
        (p / "LICENSE").write_text("synthetic license fixture\n")
        (p / "MATERIALS.json").write_bytes(materials.dump(dict(
            firmware_commit=frozen.COMMIT, firmware_fingerprint=frozen.SOURCE_SHA256,
            files=materials.file_rows(p))))
        return p

    def test_material_manifest_tampering_and_missing_files(self):
        p = self.fixture("material-fixture")
        materials.verify_tree(p)
        (p / "LICENSE").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "inventory"): materials.verify_tree(p)
        (p / "LICENSE").unlink()
        with self.assertRaisesRegex(ValueError, "inventory"): materials.verify_tree(p)

    def test_archive_determinism_and_exact_members(self):
        p = self.fixture("archive-fixture")
        a, b = self.directory / "a.zip", self.directory / "b.zip"
        materials.write_zip(p, a); materials.write_zip(p, b)
        self.assertEqual(a.read_bytes(), b.read_bytes())
        with self.assertRaises(ValueError): materials.write_zip(p, a)
        with zipfile.ZipFile(b, "a") as z: z.writestr("unexpected", b"not allowed")
        with self.assertRaises(ValueError): materials.verify_zip(b, p)

    def test_restore_cannot_overlay_an_existing_core(self):
        p = self.fixture("restore-fixture")
        with self.assertRaisesRegex(ValueError, "NEW"):
            materials.restore(p, self.restored, self.directory / "new-core")

    def test_dirty_publication_has_no_invented_commit(self):
        responses = [frozen.COMMIT.encode(), b"", b"scripts/publication_release.py\n",
                     b"scripts/publication_release.py\0", b"scripts/publication_release.py\n"]
        with patch.object(publication, "git", side_effect=responses):
            actual = publication.publication_identity(ROOT)
        self.assertIsNone(actual["commit"])
        self.assertTrue(actual["dirty"])
        binary = next(r for r in actual["files"] if r["path"].endswith(".tar.gz"))
        raw = (ROOT / binary["path"]).read_bytes()
        self.assertEqual(binary["bytes"], len(raw))
        self.assertEqual(binary["sha256"], materials.sha(raw))
        with patch.object(publication, "git", side_effect=responses):
            with self.assertRaisesRegex(ValueError, "clean, distinct"):
                publication.publication_identity(ROOT, True)

    def test_production_change_rejected(self):
        def fake(root, *args):
            if args[:2] == ("rev-parse", "HEAD"): return frozen.COMMIT.encode()
            if args[0] == "merge-base": return b""
            if args[0] == "diff": return b"src/src.cpp\n"
            if args[0] == "ls-files": return b""
            raise AssertionError(args)
        with patch.object(publication, "git", side_effect=fake):
            with self.assertRaisesRegex(ValueError, "non-publication"):
                publication.publication_identity(ROOT)

    def test_exact_document_publication_paths_and_no_broad_docs_permission(self):
        for name in publication.candidate.RELEASE_DOCUMENTS:
            self.assertIn(name, publication.PUBLICATION_PATHS)
            self.assertTrue(publication.allowed(name))
        self.assertNotIn("docs/", publication.PUBLICATION_PATHS)
        for name in ("docs/unreviewed.md", "docs/FT6336U_THDIFF_ABA_TEST.md", "dist/private.log"):
            self.assertFalse(publication.allowed(name))

    def test_required_documents_fingerprinted_and_missing_inputs_rejected(self):
        root = self.directory / "publication-document-inputs"
        for name in publication.PUBLICATION_PATHS:
            source = ROOT / name;target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if source.is_dir(): shutil.copytree(source, target)
            else: shutil.copyfile(source, target)
        for name in publication.candidate.documentation_links(ROOT):
            target = root / name
            if not target.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, target)
        def fake(_, *args):
            if args[0] == "rev-parse": return frozen.COMMIT.encode()
            if args[0] == "merge-base": return b""
            if args[0] == "diff": return b"KNOWN_ISSUES.md\n"
            if args[0] == "ls-files": return b""
            raise AssertionError(args)
        with patch.object(publication, "git", side_effect=fake):
            first = publication.publication_identity(root)
            paths = {row["path"] for row in first["files"]}
            self.assertTrue(set(publication.candidate.RELEASE_DOCUMENTS) <= paths)
            self.assertIsNone(first["commit"])
            for name in publication.candidate.RELEASE_DOCUMENTS:
                path = root / name;data = path.read_bytes()
                path.write_bytes(data+b"\nReviewed fixture change.\n")
                changed = publication.publication_identity(root)
                self.assertNotEqual(changed["input_sha256"], first["input_sha256"])
                self.assertNotEqual(changed["distribution_documentation"], first["distribution_documentation"])
                path.unlink()
                with self.assertRaisesRegex(ValueError, "required release document"):
                    publication.publication_identity(root)
                path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "clean, distinct"):
                publication.publication_identity(root, True)
            (root / "scripts/publication_release.py").unlink()
            with self.assertRaisesRegex(ValueError, "required publication input"):
                publication.publication_identity(root)

    def test_untracked_local_report_link_rejected_without_packaging_report(self):
        local = "docs/FT6336U_THDIFF_ABA_TEST.md"
        responses = [frozen.COMMIT.encode(), b"", b"KNOWN_ISSUES.md\n", local.encode()+b"\0", b"KNOWN_ISSUES.md\n"]
        with patch.object(publication, "git", side_effect=responses), \
             patch.object(publication.candidate, "documentation_links", return_value=[local]):
            with self.assertRaisesRegex(ValueError, "untracked non-publication"):
                publication.publication_identity(ROOT)

    def test_diagnostic_profiles_and_changed_source_rejected(self):
        for profile in ("cores3se_unit_diagnostics", "cores3se_base_diagnostics", "unknown"):
            with self.assertRaises(ValueError): publication.frozen_receipt(self.restored, ROOT, profile, ROOT, ROOT, ROOT)
        with patch.object(publication, "build_source_digest", return_value="0" * 64):
            with self.assertRaisesRegex(ValueError, "frozen source mismatch"):
                publication.frozen_receipt(self.restored, ROOT, "cores3se_unit", ROOT, ROOT, ROOT)

    def test_stale_receipt_and_rc_cannot_be_relabelled(self):
        folder = self.directory / "receipt-test"
        folder.mkdir()
        for field, value in (("version", "0.3.0-rc.1"), ("source_sha256", "0" * 64), ("source_modified", True)):
            receipt = json.loads((ROOT / ".pio/build/cores3se_unit/infrastructure.json").read_text())
            receipt["provenance"][field] = value
            (folder / "infrastructure.json").write_bytes(materials.dump(receipt))
            with self.assertRaisesRegex(ValueError, "stale/candidate/mixed"):
                publication.frozen_receipt(self.restored, folder, "cores3se_unit", ROOT, ROOT, ROOT)

    def test_archive_only_claim_is_not_final_relink_evidence(self):
        p = self.directory / "proof-fixture"
        source = p / "core/packages/framework-arduinoespressif32/cores/esp32/Print.cpp"
        source.parent.mkdir(parents=True)
        source.write_bytes(b"synthetic original")
        (p / "MATERIALS.json").write_bytes(b"{}")
        row = dict(passed=True, baseline_marker_present=False, modified_object_marker_present=True,
            modified_archive_marker_present=True, modified_elf_marker_present=True,
            retained_symbol=["3c000001 d Print::printf(char const*, ...)::lgpl_relink_probe"],
            baseline_elf_sha256="1"*64, modified_elf_sha256="2"*64)
        proof = dict(firmware_commit=frozen.COMMIT, source_sha256=frozen.SOURCE_SHA256,
            materials_manifest_sha256=materials.sha(b"{}"), marker="LGPL_RELINK_TEST_20261101",
            library_edit=dict(path="cores/esp32/Print.cpp", original_sha256=materials.sha(source.read_bytes()), modified_sha256="3"*64),
            profiles={env:copy.deepcopy(row) for env in publication.candidate.PROFILES})
        publication.verify_relink_evidence(proof, p)
        for key, value in (("modified_elf_marker_present",False), ("baseline_marker_present",True), ("retained_symbol",[]), ("modified_elf_sha256","1"*64)):
            bad=copy.deepcopy(proof);bad["profiles"]["cores3se_unit"][key]=value
            with self.assertRaises(ValueError): publication.verify_relink_evidence(bad,p)
        proof["materials_manifest_sha256"]="0"*64
        with self.assertRaisesRegex(ValueError,"does not match"): publication.verify_relink_evidence(proof,p)

    def test_expanded_sdk_notices_are_not_assumed_covered(self):
        p = self.directory / "scope-fixture"
        header = p / "core/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/asio/asio/asio/include/asio.hpp"
        header.parent.mkdir(parents=True)
        header.write_text("// Distributed under the Boost Software License, Version 1.0.\n")
        (p / "licenses").mkdir()
        (p / "licenses/THIRD_PARTY_LICENSES.txt").write_text("synthetic firmware-only inventory")
        found=publication.material_notice_findings(p)
        self.assertTrue(any("notice coverage failed" in text for text in found))
        self.assertTrue(any("manifest.json" in text for text in found))

    def test_complete_notice_pair_requires_every_text_and_source_reference(self):
        folder = self.directory / "notice-pair"
        folder.mkdir()
        notices.copy(ROOT, folder)
        reviewed = publication.inventory.verify_repository(ROOT)
        names = [publication.inventory.MANIFEST] + [r["path"] for r in reviewed["notice_files"]
                 if r["id"] != "freefont-preferred-source"]
        for name in names:
            target = folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / name).read_bytes())
        publication.verify_notice_tree(ROOT, folder, ROOT)
        boost = folder / publication.inventory.DIRECTORY / "texts/boost.txt"
        original = boost.read_bytes(); boost.unlink()
        with self.assertRaises(OSError): publication.verify_notice_tree(ROOT, folder, ROOT)
        boost.write_bytes(original)
        with self.assertRaises(OSError): publication.verify_notice_tree(ROOT, folder, self.directory / "absent-source")
        (folder / "unexpected.bin").write_bytes(b"not a notice")
        with self.assertRaisesRegex(ValueError, "archive member"):
            publication.verify_notice_tree(ROOT, folder, ROOT)


if __name__ == "__main__":
    unittest.main(verbosity=2)
