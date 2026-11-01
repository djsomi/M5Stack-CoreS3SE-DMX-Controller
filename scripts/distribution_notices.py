"""Offline notice verification/copying for distributions; never part of firmware."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ("LICENSE", "THIRD_PARTY_NOTICES.md", "licenses/manifest.json",
         "licenses/THIRD_PARTY_LICENSES.txt")
REQUIRED = frozenset(("m5stack-dmx512tools", "m5unified", "m5gfx", "esp-dmx",
    "lovyangfx", "adafruit-font", "result", "arduino", "esp-idf",
    "esp-idf-copyright", "freertos", "lwip", "lwip-header", "wpa-supplicant",
    "fast-pbkdf2", "cc0", "mbedtls", "http-parser", "gcc-runtime-license",
    "gcc-runtime-exception", "newlib", "libgloss", "xtensa-hal", "arduino-source-notices"))


def canonical_file(path):
    # Git's platform line-ending conversion must not alter archive bytes/hashes.
    data = Path(path).read_bytes().replace(b"\r\n", b"\n")
    data.decode("utf-8", errors="strict")
    return data


def sha(data):
    return hashlib.sha256(data).hexdigest()


def leading_source_notice(text):
    """Preserve leading block/line comments, including blank-separated // terms."""
    lines = text.replace("\r\n", "\n").splitlines(keepends=True)
    result = []; inside = False
    for line in lines:
        stripped = line.lstrip()
        if inside:
            result.append(line)
            if "*/" in line: inside = False
        elif not stripped.strip() or stripped.startswith("//"):
            result.append(line)
        elif stripped.startswith("/*"):
            result.append(line); inside = "*/" not in stripped
        else:
            break
    return "".join(result).strip("\n") + "\n" if any(line.strip() for line in result) else ""


def verify(root=ROOT):
    root = Path(root)
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / "platformio.ini", encoding="utf-8")
    dependencies = {line.strip() for line in config["env"]["lib_deps"].splitlines()
                    if line.strip() and not line.strip().startswith(";")}
    if config["env"]["platform"] != "espressif32@6.7.0" or config["env"]["framework"] != "arduino" or dependencies != {
            "M5Stack/M5Unified@0.2.21", "M5Stack/M5GFX@0.2.28",
            "https://github.com/someweisguy/esp_dmx/archive/refs/tags/v2.02.zip"}:
        raise ValueError("dependency configuration changed; review distribution notices")
    data = {name: canonical_file(root / name) for name in FILES}
    manifest = json.loads(data["licenses/manifest.json"])
    text = data["licenses/THIRD_PARTY_LICENSES.txt"]
    if manifest.get("schema") != 1 or manifest.get("text_file") != FILES[-1] or manifest.get("text_sha256") != sha(text):
        raise ValueError("invalid/stale third-party license manifest")
    notices = manifest.get("notices", [])
    if len(notices) != len(REQUIRED) or {row["id"] for row in notices} != REQUIRED:
        raise ValueError("missing/duplicate/unreviewed third-party notice")
    end = 0
    for row in notices:
        offset, length = row["offset"], row["bytes"]
        if type(offset) is not int or type(length) is not int or offset < end or length <= 0 or offset + length > len(text):
            raise ValueError("invalid/overlapping notice range: " + row["id"])
        body = text[offset:offset + length]
        if sha(body) != row["sha256"] or not row.get("source"):
            raise ValueError("changed/truncated notice: " + row["id"])
        if row["id"] == "m5stack-dmx512tools" and body not in data["LICENSE"]:
            raise ValueError("project LICENSE must retain complete original M5Stack notice")
        end = offset + length
    if b"Copyright (c) 2022 M5Stack" not in data["LICENSE"]:
        raise ValueError("missing original M5Stack copyright")
    if b"LGPL" not in data["THIRD_PARTY_NOTICES.md"]:
        raise ValueError("missing framework distribution explanation")
    return manifest


def file_manifest(root=ROOT):
    verify(root)
    return [dict(path=name, bytes=len(canonical_file(Path(root) / name)),
                 sha256=sha(canonical_file(Path(root) / name))) for name in FILES]


def copy(root, destination):
    verify(root)  # Reject missing/changed notices before writing anything.
    for name in FILES:
        path = Path(destination) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(canonical_file(Path(root) / name))


def verify_tree(folder, root=ROOT):
    verify(root)
    for name in FILES:
        if (Path(folder) / name).read_bytes() != canonical_file(Path(root) / name):
            raise ValueError("missing/modified distribution notice: " + name)


def verify_installed(libdeps, framework, toolchain, root=ROOT):
    manifest = verify(root)
    roots = {"framework": Path(framework), "toolchain": Path(toolchain)}
    roots.update({name: Path(libdeps) / name for name in ("M5Unified", "M5GFX", "esp_dmx")})
    for name, folder in roots.items():
        metadata = folder / ("library.json" if name in ("M5Unified", "M5GFX", "esp_dmx") else "package.json")
        if json.loads(metadata.read_text(encoding="utf-8"))["version"] != manifest["versions"][name]:
            raise ValueError("unreviewed installed dependency version: " + name)
    for row in manifest["installed_checks"]:
        if sha(canonical_file(roots[row["root"]] / row["path"])) != row["sha256"]:
            raise ValueError("installed notice/source differs: " + row["root"] + "/" + row["path"])
    source_row = next(row for row in manifest["notices"] if row["id"] == "arduino-source-notices")
    text = canonical_file(Path(root) / FILES[-1])
    captured = text[source_row["offset"]:source_row["offset"] + source_row["bytes"]]
    for row in manifest["installed_checks"]:
        if row["root"] == "framework" and row["path"].startswith(("cores/", "libraries/")):
            header = leading_source_notice(canonical_file(roots["framework"] / row["path"]).decode()).encode()
            if header and header not in captured:
                raise ValueError("missing captured source-file notice: " + row["path"])
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--libdeps", type=Path)
    parser.add_argument("--framework", type=Path)
    parser.add_argument("--toolchain", type=Path)
    args = parser.parse_args()
    supplied = (args.libdeps, args.framework, args.toolchain)
    if any(supplied) and not all(supplied):
        parser.error("installed verification requires --libdeps, --framework and --toolchain")
    result = verify_installed(*supplied) if all(supplied) else verify()
    print("PASS: %d complete notices, deterministic distribution paths%s" %
          (len(result["notices"]), "; installed versions/source checks match" if all(supplied) else ""))
