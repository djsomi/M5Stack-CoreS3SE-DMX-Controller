"""Local source/build-material preparation. Never packages installable release ZIPs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import sys
import zipfile

import distribution_notices as notices
import frozen_source as frozen
import source_material_inventory as inventory

EXTERNAL_TOOLS = ("toolchain-xtensa-esp32s3", "toolchain-riscv32-esp", "tool-esptoolpy",
                  "tool-scons")
LIBRARIES = ("M5Unified", "M5GFX", "esp_dmx")


def dump(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(name):
    p = PurePosixPath(name)
    if (not name or "\\" in name or ":" in name or p.is_absolute() or
            any(part in ("", ".", "..", ".git", "__pycache__") for part in name.split("/"))):
        raise ValueError("unsafe material path")
    return p


def tree(folder):
    for p in sorted(Path(folder).rglob("*")):
        rel = p.relative_to(folder)
        if any(part in (".git", "__pycache__", ".DS_Store") for part in rel.parts):
            continue
        if p.is_symlink():
            raise ValueError("symlink not permitted in material input")
        if p.is_file():
            yield rel.as_posix(), p


def copy_tree(source, destination, predicate=lambda name: True):
    for name, path in tree(source):
        if predicate(name):
            target = Path(destination) / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)


def file_rows(folder, exclude=()):
    return [dict(path=name, bytes=p.stat().st_size, sha256=sha(p.read_bytes()))
            for name, p in tree(folder) if name not in exclude]


def verify_tree(folder):
    folder = Path(folder)
    m = json.loads((folder / "MATERIALS.json").read_text())
    names = [r["path"] for r in m["files"]]
    for name in names:
        safe_path(name)
    if len(set(names)) != len(names) or file_rows(folder, ("MATERIALS.json",)) != m["files"]:
        raise ValueError("material inventory/checksum mismatch")
    if m["firmware_commit"] != frozen.COMMIT or m["firmware_fingerprint"] != frozen.SOURCE_SHA256:
        raise ValueError("wrong source identity")
    if m.get("schema") == 2:
        reviewed = inventory.verify_payload(folder)
        if m.get("source_material_inventory_sha256") != sha((folder / inventory.MANIFEST).read_bytes()):
            raise ValueError("stale material license review")
        if m.get("reviewed_components") != len(reviewed["components"]):
            raise ValueError("incomplete component inventory")
    return m


def write_zip(folder, path):
    """Exact verified tree; deterministic member order, times and modes."""
    folder, path = Path(folder), Path(path)
    if path.exists():
        raise ValueError("archive already exists")
    with zipfile.ZipFile(path, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, p in tree(folder):
            safe_path(name)
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, p.read_bytes(), compresslevel=9)
    verify_zip(path, folder)
    return dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path.read_bytes()))


def verify_zip(path, folder):
    expected = dict(tree(folder))
    with zipfile.ZipFile(path) as z:
        if z.testzip() or len(z.namelist()) != len(expected) or set(z.namelist()) != set(expected):
            raise ValueError("archive inventory/corruption")
        for name, p in expected.items():
            safe_path(name)
            if z.read(name) != p.read_bytes():
                raise ValueError("archive payload mismatch")


def prepare(root, core, destination):
    root, core, destination = Path(root), Path(core), Path(destination)
    framework = core / "packages/framework-arduinoespressif32"
    libdeps = root / ".pio/libdeps/cores3se_unit"
    notices.verify_installed(libdeps, framework, core / "packages/toolchain-xtensa-esp32s3", root)
    destination.mkdir(parents=True, exist_ok=False)
    frozen.create(root, destination / "frozen-source")
    notices.copy(root, destination)
    tools = destination / "tools"
    tools.mkdir()
    for name in ("frozen_source.py", "publication_materials.py", "distribution_notices.py", "source_material_inventory.py"):
        (tools / name).write_bytes(notices.canonical_file(root / "scripts" / name))
    (destination / "BUILD-AND-RELINK.md").write_bytes(notices.canonical_file(root / "docs/V0_3_0_LGPL_DELIVERY.md"))
    # Actual installed package source, headers, scripts and all SDK inputs for S3.
    # Exclude other chips and host .exe tools: none are needed for this target.
    def framework_file(name):
        return not name.endswith(".exe") and not inventory.exclusion(inventory.FRAMEWORK + name)
    copy_tree(framework, destination / "core/packages/framework-arduinoespressif32", framework_file)
    copy_tree(core / "platforms/espressif32", destination / "core/platforms/espressif32",
              lambda name: not inventory.exclusion(inventory.PLATFORM + name))
    for name in LIBRARIES:
        copy_tree(libdeps / name, destination / "libdeps" / name,
                  lambda path, library=name: not inventory.exclusion("libdeps/" + library + "/" + path))
    reviewed = inventory.install(root, destination)
    # Retain every source-file notice (including unused source supplied in this
    # archive) in place; binary-only inventory is explicitly not its sole license.
    info = []
    for name in EXTERNAL_TOOLS:
        package = core / "packages" / name
        metadata = json.loads((package / "package.json").read_text())
        pm = json.loads((package / ".piopm").read_text())
        rows = file_rows(package)
        info.append(dict(name=name, version=metadata["version"], owner=pm["spec"]["owner"],
            role="externally acquired build tool; NOT included in this source archive",
            inventory_sha256=sha(dump(rows)), files=rows,
            command="pio pkg install --global --tool %s/%s@%s" % (pm["spec"]["owner"], name, metadata["version"])))
    (destination / "BUILD-TOOLS.json").write_bytes(dump(dict(platformio_core="6.2.0", tools=info)))
    sdk = dict(identity="v4.4.7-dirty (installed IDF_VER; exact SDK source commit not asserted)",
               package="platformio/framework-arduinoespressif32@3.20016.0",
               delivery="ESP32-S3 headers, static archives, linker scripts and build configuration included verbatim",
               upstream_source_reference="https://github.com/espressif/esp-idf/tree/v4.4.7")
    m = dict(schema=2, classification="LOCAL-VALIDATION-SOURCE-MATERIALS; not a firmware release",
             firmware_commit=frozen.COMMIT, firmware_fingerprint=frozen.SOURCE_SHA256,
             arduino_source="complete installed core plus required Preferences/SPI/Wire libraries, S3 variant and unchanged PIO build recipes",
             source_material_inventory_sha256=sha((destination / inventory.MANIFEST).read_bytes()),
             reviewed_components=len(reviewed["components"]),
             sdk=sdk, files=file_rows(destination))
    (destination / "MATERIALS.json").write_bytes(dump(m))
    verify_tree(destination)
    return m


def restore(materials, project, core):
    materials, project, core = Path(materials), Path(project), Path(core)
    verify_tree(materials)
    # Never overlay an existing core/framework (it might be a production install).
    if core.exists() or project.exists():
        raise ValueError("reconstruction requires NEW project and core directories")
    frozen.restore(materials / "frozen-source", project)
    copy_tree(materials / "core", core)
    for environment in ("cores3se_unit", "cores3se_base"):
        copy_tree(materials / "libdeps", project / ".pio/libdeps" / environment)
    return dict(project=project.name, core=core.name, firmware_commit=frozen.COMMIT)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="action", required=True)
    a = sub.add_parser("prepare"); a.add_argument("root", type=Path); a.add_argument("core", type=Path); a.add_argument("destination", type=Path)
    a = sub.add_parser("restore"); a.add_argument("materials", type=Path); a.add_argument("project", type=Path); a.add_argument("core", type=Path)
    a = sub.add_parser("verify"); a.add_argument("materials", type=Path)
    a = p.parse_args()
    if a.action == "prepare": result = prepare(a.root, a.core, a.destination)
    elif a.action == "restore": result = restore(a.materials, a.project, a.core)
    else: result = verify_tree(a.materials)
    print(json.dumps({k: v for k, v in result.items() if k != "files"}, indent=2))
