"""Local LGPL relink experiment. Only edits a newly reconstructed private core."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

import frozen_source as frozen
import publication_materials as materials
from web_asset_pipeline import build_source_digest

MARKER = b"LGPL_RELINK_TEST_20261101"
SIGNATURE = b"size_t Print::printf(const char *format, ...)\n{"
PROBE = (b'\n    // Local relink validation, 2026-11-01; never accepted firmware.\n'
         b'    static const char lgpl_relink_probe[] = "' + MARKER + b'";\n'
         b'    __asm__ __volatile__("" : : "r"(lgpl_relink_probe) : "memory");\n')


def run(pio, project, core, output, materials_tree):
    project, core, output, materials_tree = map(Path, (project, core, output, materials_tree))
    if output.exists(): raise ValueError("use a NEW evidence directory")
    m = materials.verify_tree(materials_tree)
    # Require exact copied framework before modifying anything. A global core
    # cannot accidentally pass the source-scope check as a reconstruction.
    if not (project / ".git/shallow").is_file() or not (project / ".git/objects/pack").is_dir():
        raise ValueError("expected isolated shallow source reconstruction")
    if build_source_digest(project) != frozen.SOURCE_SHA256 or frozen.git(project, "status", "--porcelain").strip():
        raise ValueError("source reconstruction is not clean/frozen")
    protected = [r for r in m["files"] if r["path"].startswith("core/")]
    for row in protected:
        p = core / row["path"][len("core/"):]
        if materials.sha(p.read_bytes()) != row["sha256"]:
            raise ValueError("core is not an exact pristine delivered copy: " + row["path"])
    for environment in ("cores3se_unit", "cores3se_base"):
        for row in m["files"]:
            if row["path"].startswith("libdeps/"):
                path = project / ".pio/libdeps" / environment / row["path"][len("libdeps/"):]
                if materials.sha(path.read_bytes()) != row["sha256"]:
                    raise ValueError("library is not an exact delivered copy: " + environment + "/" + row["path"])
    # Never accept the known normal PlatformIO installation as edit target.
    if core.resolve() == (Path.home() / ".platformio").resolve():
        raise ValueError("refusing production PlatformIO core")
    output.mkdir(parents=True)
    env = os.environ.copy()
    env.update(PLATFORMIO_CORE_DIR=str(core), PYTHONDONTWRITEBYTECODE="1", PLATFORMIO_DISABLE_PROGRESSBAR="true")
    command = [str(pio), "run", "-d", str(project), "-e", "cores3se_unit", "-e", "cores3se_base"]
    proof = dict(schema=1, firmware_commit=frozen.COMMIT, source_sha256=frozen.SOURCE_SHA256,
                 command=["pio", "run", "-d", "<reconstructed-source>", "-e", "cores3se_unit", "-e", "cores3se_base"],
                 materials_manifest_sha256=materials.sha((materials_tree / "MATERIALS.json").read_bytes()),
                 marker=MARKER.decode(), profiles={})
    def build(phase):
        with (output / (phase + ".log")).open("wb") as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode: raise RuntimeError(phase + " build failed; inspect isolated log; no production artifacts changed")
        for name in ("cores3se_unit", "cores3se_base"):
            folder = project / ".pio/build" / name
            keep = output / phase / name
            keep.mkdir(parents=True)
            for file in ("firmware.elf", "firmware.bin", "firmware.map", "infrastructure.json"):
                shutil.copyfile(folder / file, keep / file)
    build("baseline")
    for name in ("cores3se_unit", "cores3se_base"):
        if MARKER in (output / "baseline" / name / "firmware.elf").read_bytes():
            raise ValueError("baseline already contains probe")
    path = core / "packages/framework-arduinoespressif32/cores/esp32/Print.cpp"
    original = path.read_bytes()
    text = original.replace(b"\r\n", b"\n")
    if text.count(SIGNATURE) != 1: raise ValueError("unexpected Arduino Print implementation")
    changed = text.replace(SIGNATURE, SIGNATURE + PROBE)
    path.write_bytes(changed)
    (output / "Print-original.cpp").write_bytes(original)
    (output / "Print-modified.cpp").write_bytes(changed)
    proof["library_edit"] = dict(path="cores/esp32/Print.cpp", original_sha256=materials.sha(original),
                                 modified_sha256=materials.sha(changed), date="2026-11-01")
    build("modified")
    nm = core / "packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-nm.exe"
    for name in ("cores3se_unit", "cores3se_base"):
        folder = project / ".pio/build" / name
        for file in ("FrameworkArduino/Print.cpp.o", "libFrameworkArduino.a", "firmware.elf"):
            if MARKER not in (folder / file).read_bytes():
                raise ValueError("modified library not retained: " + name + "/" + file)
        symbols = subprocess.check_output([str(nm), "-C", str(folder / "firmware.elf")]).decode()
        retained = [line for line in symbols.splitlines() if "Print::printf" in line and "lgpl_relink_probe" in line]
        if not retained: raise ValueError("missing final linked Print probe symbol")
        map_text = (folder / "firmware.map").read_text(errors="replace")
        if "libFrameworkArduino.a(Print.cpp.o)" not in map_text:
            raise ValueError("missing Print archive member in final map")
        (output / (name + "-probe-symbol.txt")).write_text("\n".join(retained) + "\n")
        a = output / "baseline" / name / "firmware.elf"
        b = output / "modified" / name / "firmware.elf"
        proof["profiles"][name] = dict(passed=True, baseline_marker_present=False,
            modified_object_marker_present=True, modified_archive_marker_present=True,
            modified_elf_marker_present=True, retained_symbol=retained,
            baseline_elf_sha256=materials.sha(a.read_bytes()), modified_elf_sha256=materials.sha(b.read_bytes()))
    if build_source_digest(project) != frozen.SOURCE_SHA256:
        raise ValueError("frozen application inputs changed")
    (output / "relink-evidence.json").write_bytes(materials.dump(proof))
    print("PASS modified-library relink: Unit and Base; accepted images untouched")
    return proof


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("pio", "project", "core", "output", "materials"):
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run(a.pio, a.project, a.core, a.output, a.materials)
