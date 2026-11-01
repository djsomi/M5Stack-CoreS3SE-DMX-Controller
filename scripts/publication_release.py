"""Dual-provenance release preflight. Writes reports only, never firmware ZIPs."""
import argparse
import copy
import json
import re
from pathlib import Path

from build_provenance import git, identity, input_manifest
from web_asset_pipeline import build_source_digest, verify
from firmware_layout import inspect_build
import distribution_notices as notices
import frozen_source as frozen
import package_release_candidate as candidate
import publication_materials as materials
import source_material_inventory as inventory

PUBLICATION_PATHS = (
    ".gitattributes", "LICENSE", "README.md", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md",
    "licenses/manifest.json", "licenses/THIRD_PARTY_LICENSES.txt",
    "scripts/distribution_notices.py", "scripts/frozen_source.py",
    "scripts/publication_materials.py", "scripts/publication_release.py", "scripts/validate_relink.py",
    "scripts/source_material_inventory.py", "tests/release/test_material_inventory.py",
    "licenses/source_materials/", "docs/V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md",
    "scripts/package_release_candidate.py", "tests/release/test_candidate.py",
    "tests/release/test_notices.py", "tests/release/test_publication.py", "tests/web/check_scope.py",
    "docs/V0_3_0_PUBLICATION_READINESS.md", "docs/V0_3_0_LGPL_DELIVERY.md",
    "docs/V0_3_0_INSTALLATION.md", "docs/V0_3_0_RELEASE_NOTES_DRAFT.md",
    "KNOWN_ISSUES.md", "docs/V0_3_0_DOCUMENTATION_COVERAGE.md", "docs/PRESET_EXPORT_IMPORT.md",
    "docs/ESP_DMX_UPGRADE_EVALUATION.md", "docs/STAGE4_WEB_INFRASTRUCTURE_DESIGN.md",
    "docs/STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md", "docs/STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md",
    "docs/STAGE6B1_WEB_UI_FOUNDATION.md")


def allowed(name):
    return any(name.startswith(p) if p.endswith("/") else name == p for p in PUBLICATION_PATHS)


def publication_identity(root, require_commit=False):
    root = Path(root)
    head = git(root, "rev-parse", "HEAD").decode().strip()
    git(root, "merge-base", "--is-ancestor", frozen.COMMIT, head)
    changed = git(root, "diff", "--name-only", frozen.COMMIT, "--").decode().splitlines()
    extras = git(root, "ls-files", "--others", "--exclude-standard", "-z").decode().split("\0")
    if any(not allowed(p) for p in changed):
        raise ValueError("non-publication tracked change since frozen firmware")
    dirty = bool(git(root, "diff", "HEAD", "--name-only").strip() or any(allowed(p) for p in extras if p))
    documents = candidate.documentation_manifest(root)
    rows = []
    for name in PUBLICATION_PATHS:
        base = root / name
        if base.is_symlink() or (not base.is_dir() if name.endswith("/") else not base.is_file()):
            raise ValueError("missing/unsafe required publication input: " + name)
        start = len(rows)
        for path in sorted(base.rglob("*")) if base.is_dir() else [base]:
            if path.is_symlink():
                raise ValueError("unsafe publication input symlink: " + str(path))
            if path.is_file() and "__pycache__" not in path.parts:
                data = path.read_bytes() if path.name.endswith(".tar.gz") else notices.canonical_file(path)
                rows.append(dict(path=path.relative_to(root).as_posix(), bytes=len(data), sha256=materials.sha(data)))
        if len(rows) == start:
            raise ValueError("empty required publication input: " + name)
    links = candidate.documentation_links(root)
    if any(p in extras and not allowed(p) for p in links):
        raise ValueError("release documentation links to untracked non-publication material")
    if require_commit and (dirty or head == frozen.COMMIT):
        raise ValueError("publication requires a clean, distinct follow-up commit; firmware tag must stay frozen")
    return dict(commit=None if dirty else head, base_commit=head, dirty=dirty,
                input_sha256=materials.sha(materials.dump(rows)), files=rows,
                distribution_documentation=documents)


def archive_reference(path):
    path = Path(path)
    return dict(file=path.name, bytes=path.stat().st_size, sha256=materials.sha(path.read_bytes()))


def material_notice_findings(folder):
    """The firmware's retained-component inventory is not a whole-SDK audit."""
    try:
        inventory.verify_payload(folder)
    except (ValueError, OSError, KeyError) as error:
        return ["Source/build distribution notice coverage failed: " + str(error)]
    return []


def verify_relink_evidence(proof, materials_tree):
    if proof.get("firmware_commit") != frozen.COMMIT or proof.get("source_sha256") != frozen.SOURCE_SHA256:
        raise ValueError("relink evidence has wrong frozen source")
    if proof.get("materials_manifest_sha256") != materials.sha((Path(materials_tree) / "MATERIALS.json").read_bytes()):
        raise ValueError("relink evidence does not match source/build materials")
    original = Path(materials_tree) / "core/packages/framework-arduinoespressif32/cores/esp32/Print.cpp"
    edit = proof.get("library_edit", {})
    if (edit.get("path") != "cores/esp32/Print.cpp" or edit.get("original_sha256") != materials.sha(original.read_bytes()) or
            edit.get("modified_sha256") == edit.get("original_sha256") or proof.get("marker") != "LGPL_RELINK_TEST_20261101"):
        raise ValueError("invalid modified-library evidence")
    for env in candidate.PROFILES:
        row = proof.get("profiles", {}).get(env, {})
        required = ("passed", "modified_object_marker_present", "modified_archive_marker_present", "modified_elf_marker_present")
        if any(row.get(k) is not True for k in required) or row.get("baseline_marker_present") is not False:
            raise ValueError("missing successful modified-library relink: " + env)
        if (not row.get("retained_symbol") or
                any("Print::printf" not in s or "lgpl_relink_probe" not in s for s in row["retained_symbol"])):
            raise ValueError("missing final linked Print symbol")
        for key in ("baseline_elf_sha256", "modified_elf_sha256"):
            if not re.fullmatch("[0-9a-f]{64}", row.get(key, "")):
                raise ValueError("missing final ELF checksum")
        if row["baseline_elf_sha256"] == row["modified_elf_sha256"]:
            raise ValueError("ELF unchanged after claimed relink")


def verify_notice_tree(root, folder, source_materials):
    """Keep license texts complete; source supplement lives once in source ZIP."""
    root, folder, source_materials = map(Path, (root, folder, source_materials))
    notices.verify_tree(folder, root)
    reviewed = inventory.verify_repository(root)
    expected = set(notices.FILES) | {inventory.MANIFEST}
    for row in reviewed["notice_files"]:
        # This is the sole explicitly identified preferred-source supplement.
        # Its full bytes must be verified in the paired source archive instead.
        source_only = row["id"] == "freefont-preferred-source"
        path = (source_materials if source_only else folder) / row["path"]
        data = notices.canonical_file(path) if row.get("text", True) else path.read_bytes()
        if materials.sha(data) != row["sha256"] or len(data) != row["bytes"]:
            raise ValueError("missing/modified paired source notice material: " + row["path"])
        if not source_only: expected.add(row["path"])
    if set(dict(materials.tree(folder))) != expected:
        raise ValueError("unexpected/missing complete-notice archive member")
    if notices.canonical_file(folder / inventory.MANIFEST) != notices.canonical_file(root / inventory.MANIFEST):
        raise ValueError("notice archive has stale component inventory")


def frozen_receipt(source, build, environment, framework, esptool, notice_root):
    """Compare to the frozen source, NEVER to today's packaging checkout."""
    source, build = Path(source), Path(build)
    if environment not in candidate.PROFILES:
        raise ValueError("production Unit/Base only")
    if git(source, "rev-parse", "HEAD").decode().strip() != frozen.COMMIT or build_source_digest(source) != frozen.SOURCE_SHA256:
        raise ValueError("frozen source mismatch")
    receipt = json.loads((build / "infrastructure.json").read_text())
    p = identity(source, environment, frozen.SOURCE_SHA256)
    if p["status"] != "stable" or p["source_modified"] or receipt.get("provenance") != p:
        raise ValueError("stale/candidate/mixed firmware receipt")
    manifest = verify(source)
    expected = inspect_build(source, build, Path(framework), Path(esptool),
        [(str(offset), str(build / name)) for name, offset in candidate.BINARIES.items()],
        environment, frozen.SOURCE_SHA256, manifest)
    expected["provenance"] = p
    if receipt != expected:
        raise ValueError("receipt does not match frozen source/images/layout")
    reviewed = notices.verify(notice_root)
    app_sha = next(c["sha256"] for c in receipt["components"] if c["file"] == "firmware.bin")
    if app_sha != reviewed["firmware_sha256"][environment]:
        raise ValueError("image is not the frozen accepted production binary")
    # Reuse stricter sector-rounded NVS and embedded provenance checks.
    checked = copy.deepcopy(receipt)
    checked["diagnostics"] = False
    for c in checked["components"]:
        c["erase_range"] = [c["offset"] // 4096 * 4096, (c["offset"] + c["size"] + 4095) // 4096 * 4096]
    candidate.validate_components(build, checked, Path(framework), Path(esptool))
    return checked


def dry_run(root, source, builds, framework, esptool, source_archive, notice_archive,
            materials_tree, notices_tree, relink_evidence, output, require_commit=False):
    output = Path(output)
    if output.exists():
        raise ValueError("report output already exists")
    publication = publication_identity(root, require_commit)
    materials.verify_tree(materials_tree)
    materials.verify_zip(source_archive, materials_tree)
    material_findings = material_notice_findings(materials_tree)
    if not material_findings:
        inventory.verify_repository(root)
        if notices.canonical_file(Path(root) / inventory.MANIFEST) != notices.canonical_file(Path(materials_tree) / inventory.MANIFEST):
            raise ValueError("source archive differs from publisher's reviewed component inventory")
    if require_commit and material_findings:
        raise ValueError("build-material notice closure is incomplete: " + "; ".join(material_findings))
    verify_notice_tree(root, notices_tree, materials_tree)
    materials.verify_zip(notice_archive, notices_tree)
    proof = json.loads(Path(relink_evidence).read_text())
    verify_relink_evidence(proof, materials_tree)
    profiles = [frozen_receipt(source, Path(builds) / env, env, framework, esptool, root)
                for env in candidate.PROFILES]
    report = dict(schema=1, classification="UNPUBLISHED-DRY-RUN; NO FIRMWARE ARCHIVES GENERATED",
        firmware=dict(tag=frozen.TAG, tag_object=frozen.TAG_OBJECT, source_commit=frozen.COMMIT,
                      source_sha256=frozen.SOURCE_SHA256), publication=publication,
        corresponding_source=archive_reference(source_archive), notices=archive_reference(notice_archive),
        relink_evidence=archive_reference(relink_evidence), profiles=profiles,
        publication_ready=bool(require_commit and not material_findings), build_material_notice_findings=material_findings,
        distribution_documentation=candidate.documentation_manifest(root),
        migration=dict(nvs_offset=0x9000, nvs_size=0x5000, sparse_images_only=True,
            unit="user-accepted functional migration", base="experimental; separate migration unverified",
            interrupted_upgrade_recovery="unverified; not claimed safe"),
        release_gate="requires reviewed clean publication commit and equivalent source/notice download access")
    if publication_identity(root, require_commit) != publication:
        raise ValueError("publication inputs changed during verification")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(materials.dump(report))
    return report


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "source", "builds", "framework", "esptool", "source-archive", "notice-archive",
                 "materials-tree", "notices-tree", "relink-evidence", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--require-publication-commit", action="store_true")
    a = p.parse_args()
    result = dry_run(a.root, a.source, a.builds, a.framework, a.esptool, a.source_archive, a.notice_archive,
                     a.materials_tree, a.notices_tree, a.relink_evidence, a.output, a.require_publication_commit)
    print("Verified unpublished dual-provenance report:", a.output)
