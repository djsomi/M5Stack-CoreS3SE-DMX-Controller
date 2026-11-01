"""Create two local sparse-image candidates. Never uses serial, tags or publishing."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import zipfile

from build_provenance import identity, input_manifest, git, INPUTS
from firmware_layout import EXPECTED, LAYOUT_ID, validate_image, partition_sdk, validate_table
from package_firmware import package
from web_asset_pipeline import ROOT, verify, build_source_digest
import distribution_notices

PROFILES = {"cores3se_unit": "UNIT", "cores3se_base": "BASE-EXPERIMENTAL"}
DOCS = ("docs/V0_3_0_INSTALLATION.md", "docs/V0_3_0_MIGRATION_ACCEPTANCE.md")
RELEASE_DOCUMENTS = ("KNOWN_ISSUES.md", "docs/V0_3_0_DOCUMENTATION_COVERAGE.md",
                     "docs/PRESET_EXPORT_IMPORT.md")
BINARIES = {"bootloader.bin": 0, "partitions.bin": 0x8000,
            "boot_app0.bin": 0xe000, "firmware.bin": 0x10000}
MEMBERS = set(BINARIES) | {"manifest.json", "SHA256SUMS.txt", "INSTALL.md",
                            "MIGRATION-TEST.md", "SOURCE-MANIFEST.json", "SOURCE.patch"} | set(distribution_notices.FILES) | set(RELEASE_DOCUMENTS)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def dump(data):
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")


def documentation_manifest(root):
    rows = []
    for name in RELEASE_DOCUMENTS:
        path = Path(root) / name
        if path.is_symlink() or not path.is_file():
            raise ValueError("missing/unsafe required release document: " + name)
        data = distribution_notices.canonical_file(path)
        rows.append(dict(path=name, bytes=len(data), sha256=sha(data)))
    return rows


def documentation_links(root):
    """Check repository-context links; do not copy their targets into bundles."""
    root = Path(root).resolve()
    targets = set()
    documentation_manifest(root)
    for name in RELEASE_DOCUMENTS:
        text = distribution_notices.canonical_file(root / name).decode("utf-8")
        if re.search(r"[A-Za-z]:[/\\]Users[/\\](?!<user>(?:[/\\]|$))[^/\\\s]+", text):
            raise ValueError("personal machine path in release document: " + name)
        for target in re.findall(r'\]\(([^)]+)\)', text):
            if target.startswith(("https://", "http://", "mailto:", "#")):
                continue
            target = target.split("#", 1)[0]
            path = (root / name).parent.joinpath(target).resolve()
            if not path.is_relative_to(root) or not path.exists():
                raise ValueError("missing/unsafe documentation link: " + name + " -> " + target)
            relative = path.relative_to(root).as_posix()
            if relative.startswith(("dist/", "_kezdeti", ".git/", ".pio/")):
                raise ValueError("private build/history link in release document: " + relative)
            targets.add(relative)
    return sorted(targets)


def copy_documentation(root, folder):
    rows = documentation_manifest(root)
    documentation_links(root)
    for row in rows:
        target = Path(folder) / row["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(distribution_notices.canonical_file(Path(root) / row["path"]))
    return rows


def verify_documentation(folder, root, manifest):
    expected = documentation_manifest(root)
    if manifest.get("distribution_documentation") != expected:
        raise ValueError("missing/incorrect documentation manifest")
    for row in expected:
        if (Path(folder) / row["path"]).read_bytes() != distribution_notices.canonical_file(Path(root) / row["path"]):
            raise ValueError("missing/modified release document: " + row["path"])


def source_patch(root):
    paths = (*INPUTS, "tests", ".gitattributes", "README.md", *distribution_notices.FILES,
             *DOCS, *RELEASE_DOCUMENTS, "docs/V0_3_0_RELEASE_NOTES_DRAFT.md", "docs/V0_3_0_PUBLICATION_READINESS.md")
    data = git(root, "diff", "--binary", "HEAD", "--", *paths)
    extras = git(root, "ls-files", "-z", "--others", "--exclude-standard", "--", *paths)
    for name in sorted(p for p in extras.decode().split("\0") if p and "__pycache__" not in Path(p).parts):
        text = (Path(root) / name).read_text(encoding="utf-8").replace("\r\n", "\n")
        delta = "diff --git a/%s b/%s\nnew file mode 100644\n" % (name, name)
        delta += "".join(difflib.unified_diff([], text.splitlines(keepends=True),
                                           fromfile="/dev/null", tofile="b/" + name))
        data += delta.encode("utf-8")
    return data


def validate_components(folder, manifest, framework, esptool):
    if manifest["profile"] not in PROFILES or manifest.get("diagnostics") is not False:
        raise ValueError("production Unit/Base only")
    if manifest["layout"] != LAYOUT_ID or manifest["flash_size"] != 0x1000000:
        raise ValueError("wrong flash layout/capacity")
    components = manifest["components"]
    if len(components) != 4 or {c["file"]: c["offset"] for c in components} != BINARIES:
        raise ValueError("expected exactly four individually addressed images")
    for c in components:
        data = (folder / c["file"]).read_bytes()
        if len(data) != c["size"] or sha(data) != c["sha256"]:
            raise ValueError("component checksum/length mismatch")
        capacity = {"bootloader.bin": 0x8000, "partitions.bin": 0x1000,
                    "boot_app0.bin": 0x2000, "firmware.bin": 0x600000}[c["file"]]
        if not data or len(data) > capacity:
            raise ValueError("image exceeds reserved range")
        start = c["offset"] // 4096 * 4096
        end = (c["offset"] + len(data) + 4095) // 4096 * 4096
        if start < 0xe000 and end > 0x9000:
            raise ValueError("erased sectors overlap NVS")
        c_expected = [start, end]
        if c.get("erase_range") != c_expected:
            raise ValueError("incorrect sector range metadata")
    for name in ("bootloader.bin", "firmware.bin"):
        validate_image(folder / name, esptool)
    sdk = partition_sdk(framework)
    validate_table(sdk.PartitionTable.from_binary((folder / "partitions.bin").read_bytes()))
    if (folder / "boot_app0.bin").read_bytes() != (framework / "tools/partitions/boot_app0.bin").read_bytes():
        raise ValueError("wrong OTA initialization")
    image = (folder / "firmware.bin").read_bytes()
    p = manifest["provenance"]
    for marker in (p["version"], p["source_commit"], p["source_sha256"], p["build_id"], manifest["profile"]):
        if marker.encode() not in image:
            raise ValueError("missing linked provenance")


def verify_zip(path, folder, root=ROOT):
    distribution_notices.verify_tree(folder, root)
    with zipfile.ZipFile(path) as z:
        if len(z.namelist()) != len(MEMBERS) or set(z.namelist()) != MEMBERS or z.testzip():
            raise ValueError("unexpected/corrupt archive contents")
        for name in MEMBERS:
            if z.read(name) != (folder / name).read_bytes():
                raise ValueError("archive differs from verified bundle")
        manifest = json.loads(z.read("manifest.json"))
        if manifest.get("distribution_notices") != distribution_notices.file_manifest(root):
            raise ValueError("missing/incorrect notice manifest")
        verify_documentation(folder, root, manifest)
        if manifest.get("release_candidate") is not True:
            raise ValueError("candidate archive cannot claim stable release classification")
        sums = z.read("SHA256SUMS.txt").decode("ascii").splitlines()
        expected = MEMBERS - {"SHA256SUMS.txt"}
        found = set()
        for line in sums:
            digest, name = line.split("  ", 1)
            if name not in expected or name in found or sha(z.read(name)) != digest:
                raise ValueError("archive checksum mismatch")
            found.add(name)
        if found != expected:
            raise ValueError("incomplete checksums")


def create(root, environment, output, framework, esptool):
    root, output = Path(root), Path(output)
    if environment not in PROFILES:
        raise ValueError("diagnostic/unrecognized environment cannot be a release candidate")
    distribution_notices.verify_installed(root / ".pio/libdeps" / environment, framework,
                                         Path(framework).parent / "toolchain-xtensa-esp32s3", root)
    digest = build_source_digest(root)
    expected = identity(root, environment, digest)
    build = root / ".pio/build" / environment
    receipt = json.loads((build / "infrastructure.json").read_text())
    if receipt.get("provenance") != expected:
        raise ValueError("stale Git/version/build provenance; rebuild this environment")
    output.mkdir(parents=True, exist_ok=True)
    name = "CoreS3SE-DMX-v%s-%s" % (expected["target_release"], PROFILES[environment])
    folder, archive = output / name, output / (name + ".zip")
    if folder.exists() or archive.exists():
        raise FileExistsError("candidate destination already exists; never overwritten")
    package(root, environment, folder)
    distribution_notices.copy(root, folder)
    documents = copy_documentation(root, folder)
    manifest = dict(receipt, schema=2, release_candidate=True, diagnostics=False,
                    distribution_notices=distribution_notices.file_manifest(root),
                    distribution_documentation=documents,
                    hardware_status="recommended" if environment == "cores3se_unit" else "experimental",
                    physical_migration_validation="pending",
                    nvs=dict(offset=0x9000, size=0x5000, erased_by_images=False),
                    partition_entries=[dict(name=n, type=t, subtype=s, offset=o, size=l)
                                       for n, t, s, o, l in EXPECTED],
                    toolchain=dict(platform="espressif32@6.7.0", framework="Arduino 2.0.16 / IDF 4.4.7",
                                   M5Unified="0.2.21", M5GFX="0.2.28", esp_dmx="v2.02 + repository patches"))
    for c in manifest["components"]:
        c["erase_range"] = [c["offset"] // 4096 * 4096,
                            (c["offset"] + c["size"] + 4095) // 4096 * 4096]
    overlay = source_patch(root)
    manifest["source_overlay_sha256"] = sha(overlay)
    manifest["source_note"] = "Git HEAD plus SOURCE.patch; modified inputs are explicitly identified, not a clean tagged release"
    (folder / "SOURCE.patch").write_bytes(overlay)
    (folder / "SOURCE-MANIFEST.json").write_bytes(dump(input_manifest(root)))
    (folder / "MIGRATION-TEST.md").write_text((root / DOCS[1]).read_text(encoding="utf-8"), encoding="utf-8", newline="\n")
    table = "\n".join("| %s | `0x%06x` | %d |" % (c["file"], c["offset"], c["size"]) for c in manifest["components"])
    pairs = " ".join("0x%04x %s" % (c["offset"], c["file"]) for c in manifest["components"])
    install = "# Local candidate: " + name + "\n\n"
    install += "Version: `%s`; environment: `%s`; Git HEAD: `%s`; modified inputs: `%s`.\n\n" % (expected["version"], environment, expected["source_commit"], expected["source_modified"])
    install += "## Packaged public documentation\n\n"
    install += "\n".join("- [%s](%s)" % (n, n) for n in RELEASE_DOCUMENTS) + "\n\n"
    install += ("These files retain repository-context evidence links. Resolve links outside this sparse bundle "
                "against the matching source/publication checkout; historical investigation files are not bundled. "
                "This candidate records HEAD plus SOURCE.patch, not a completed stable publication. "
                "A final frozen-firmware distribution must identify its later publication commit separately.\n\n")
    install += "| File | Offset | Bytes |\n|---|---:|---:|\n" + table + "\n\n"
    install += (root / DOCS[0]).read_text(encoding="utf-8")
    install += "\n## Exact command for this bundle\n\nNOT EXECUTED. Replace PORT; first verify checksums, profile, capacity, backup and acceptance gates.\n\n```text\npython -m esptool --chip esp32s3 --port PORT --baud 1500000 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB " + pairs + "\n```\n"
    (folder / "INSTALL.md").write_text(install, encoding="utf-8", newline="\n")
    (folder / "manifest.json").write_bytes(dump(manifest))
    validate_components(folder, manifest, framework, esptool)
    sums = "".join(sha((folder / n).read_bytes()) + "  " + n + "\n" for n in sorted(MEMBERS - {"SHA256SUMS.txt"}))
    (folder / "SHA256SUMS.txt").write_text(sums, encoding="ascii", newline="\n")
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for n in sorted(MEMBERS):
            info = zipfile.ZipInfo(n, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, (folder / n).read_bytes(), compresslevel=9)
    verify_zip(archive, folder, root)
    if identity(root, environment, build_source_digest(root)) != expected:
        raise ValueError("source changed during packaging")
    return archive


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--framework", type=Path, required=True)
    p.add_argument("--esptool", type=Path, required=True)
    args = p.parse_args()
    archives = [create(ROOT, e, args.output, args.framework, args.esptool) for e in PROFILES]
    (args.output / "SHA256SUMS.txt").write_text("".join(sha(a.read_bytes()) + "  " + a.name + "\n" for a in archives), encoding="ascii", newline="\n")
    for a in archives:
        print(a.name, a.stat().st_size, sha(a.read_bytes()))
