"""Offline asset generator/verifier; standard library only. No build-time writes to sources."""
import argparse
import base64
import gzip
import hashlib
import io
import json
from pathlib import Path
import sys
import zlib
import web_ui_bundle

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ("index.html", "style.css", "core.js", "ui.js")
MIME = {"index.html": "text/html; charset=utf-8",
        "style.css": "text/css; charset=utf-8",
        "core.js": "text/javascript; charset=utf-8",
        "ui.js": "text/javascript; charset=utf-8"}
TOKENS = (b"{{STYLE_URL}}", b"{{CORE_URL}}", b"{{UI_URL}}", b"{{CORE_INTEGRITY}}", b"{{UI_INTEGRITY}}")
GENERATOR = {"python": "3.11.7", "zlib": "1.2.13"}
LIMIT = 256 * 1024
MAX_ASSET_BYTES = 16384  # Existing src/http_policy.h HTTP cap; never silently expand.


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode()


def source_files(root):
    web_ui_bundle.verify(root)
    result = {}
    for name in INPUTS:
        path = root / "web/src" / name
        if not path.is_file():
            raise ValueError("missing required web input: " + str(path))
        data = path.read_bytes().replace(b"\r\n", b"\n")
        text = data.decode("utf-8", errors="strict")
        if not text.strip() or "\x00" in text or "\r" in text or len(data) > LIMIT:
            raise ValueError("invalid/empty/oversized UTF-8 web input: " + name)
        result[name] = data
    for token in TOKENS:
        if result["index.html"].count(token) != 1:
            raise ValueError("index.html must contain exactly one " + token.decode())
    return result


def compress(data):
    output = io.BytesIO()
    with gzip.GzipFile(fileobj=output, mode="wb", filename="", mtime=0, compresslevel=9) as stream:
        stream.write(data)
    return output.getvalue()


def rendered(root):
    sources = source_files(root)
    urls = {name: "/" + name.rsplit(".", 1)[0] + "." + sha(sources[name])[:16] + "." + name.rsplit(".", 1)[1]
            for name in INPUTS if name != "index.html"}
    urls["index.html"] = "/"
    bodies = dict(sources)
    replacements = {b"{{STYLE_URL}}": urls["style.css"], b"{{CORE_URL}}": urls["core.js"], b"{{UI_URL}}": urls["ui.js"]}
    for name in ("core", "ui"):
        replacements[("{{" + name.upper() + "_INTEGRITY}}").encode()] = "sha256-" + base64.b64encode(
            hashlib.sha256(sources[name + ".js"]).digest()).decode("ascii")
    for token, replacement in replacements.items():
        bodies["index.html"] = bodies["index.html"].replace(token, replacement.encode("ascii"))
    return sources, urls, bodies


def generate(root=ROOT, output=None):
    if {"python": sys.version.split()[0], "zlib": zlib.ZLIB_RUNTIME_VERSION} != GENERATOR:
        raise ValueError("asset regeneration requires Python 3.11.7 / zlib 1.2.13; normal builds only verify checked files")
    output = output or root / "web/generated"
    web_ui_bundle.generate(root)  # Explicit generation only; verify() remains read-only.
    sources, urls, bodies = rendered(root)
    entries = []
    blobs = {}
    for name in INPUTS:
        blob = compress(bodies[name]); blobs[name + ".gz"] = blob
        if len(blob) > MAX_ASSET_BYTES:
            raise ValueError("compressed HTTP asset exceeds 16 KiB: " + name)
        entries.append(dict(source=name, source_sha256=sha(sources[name]), path=urls[name],
                            file=name + ".gz", content_type=MIME[name], content_encoding="gzip",
                            original_size=len(bodies[name]), gzip_size=len(blob), sha256=sha(blob)))
    manifest = dict(schema=1, generator=GENERATOR, assets=entries, bundle_sha256=sha(canonical(entries)))
    if sum(len(b) for b in blobs.values()) > LIMIT:
        raise ValueError("compressed asset budget exceeded")
    output.mkdir(parents=True, exist_ok=True)
    (output / "app.js.gz").unlink(missing_ok=True)
    for name, data in blobs.items():
        (output / name).write_bytes(data)
    (output / "manifest.json").write_bytes(canonical(manifest))
    return manifest


def verify(root=ROOT):
    folder = root / "web/generated"
    if not (folder / "manifest.json").is_file():
        raise ValueError("missing generated assets; run python scripts/web_asset_pipeline.py --generate")
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if {p.name for p in folder.glob("*.gz")} != {name + ".gz" for name in INPUTS}:
        raise ValueError("unexpected/missing generated asset inventory")
    sources, urls, bodies = rendered(root)
    if manifest.get("schema") != 1 or manifest.get("generator") != GENERATOR:
        raise ValueError("unsupported asset manifest/generator")
    assets = manifest.get("assets", [])
    if len(assets) != len(INPUTS) or manifest.get("bundle_sha256") != sha(canonical(assets)):
        raise ValueError("invalid asset count/bundle hash")
    total = 0
    for entry, name in zip(assets, INPUTS):
        blob = (folder / (name + ".gz")).read_bytes()
        if len(blob) > MAX_ASSET_BYTES:
            raise ValueError("compressed HTTP asset exceeds 16 KiB: " + name)
        expected = dict(source=name, source_sha256=sha(sources[name]), path=urls[name], file=name + ".gz",
                        content_type=MIME[name], content_encoding="gzip", original_size=len(bodies[name]),
                        gzip_size=len(blob), sha256=sha(blob))
        if entry != expected or blob[:10] != bytes.fromhex("1f8b08000000000002ff"):
            raise ValueError("stale/invalid asset metadata or gzip header: " + name)
        if gzip.decompress(blob) != bodies[name]:
            raise ValueError("gzip round-trip mismatch: " + name)
        total += len(blob)
    if total > LIMIT:
        raise ValueError("compressed asset budget exceeded")
    return manifest


def build_source_digest(root=ROOT):
    rows = []
    for folder in ("src", "include", "scripts", "patches", "partitions", "web"):
        for path in sorted((root / folder).rglob("*")):
            if not path.is_file() or "__pycache__" in path.parts:
                continue
            data = path.read_bytes()
            if path.suffix != ".gz":
                data = data.replace(b"\r\n", b"\n")
            rows.append((path.relative_to(root).as_posix(), sha(data)))
    rows.append(("platformio.ini", sha((root / "platformio.ini").read_bytes().replace(b"\r\n", b"\n"))))
    return sha(canonical(rows))


def emit_cpp(root, destination, profile, source_digest):
    manifest = verify(root)
    lines = ['// Generated: do not edit. No static constructors or runtime allocation.',
             '#include "web_assets.h"', '#include <string.h>', 'namespace {']
    for index, entry in enumerate(manifest["assets"]):
        data = (root / "web/generated" / entry["file"]).read_bytes()
        lines.append("alignas(4) const uint8_t asset_%d[] = {" % index)
        for start in range(0, len(data), 16):
            lines.append(",".join("0x%02x" % n for n in data[start:start+16]) + ",")
        lines.append("};")
    lines.append("const web_assets::Asset assets[] = {")
    for index, a in enumerate(manifest["assets"]):
        lines.append('{%s,%s,"gzip",asset_%d,%d,%d,%s},' % (
            json.dumps(a["path"]), json.dumps(a["content_type"]), index, a["gzip_size"],
            a["original_size"], json.dumps(a["sha256"])))
    lines += ['};', '}', 'extern "C" {',
              'extern const web_assets::Bundle web_asset_bundle = {assets,%d,%s,%s,%s,"cores3se-16m-ota6-v1"};' % (
                  len(manifest["assets"]), json.dumps(manifest["bundle_sha256"]), json.dumps(source_digest), json.dumps(profile)),
              '}', 'namespace web_assets {', 'const Bundle& bundle() { return web_asset_bundle; }',
              'const Asset* find(const char* path) {', '  if (!path) return nullptr;',
              '  for (size_t i=0; i<bundle().count; ++i)',
              '    if (strcmp(path, assets[i].path)==0) return &assets[i];',
              '  return nullptr;', '}', '}', '']
    data = "\n".join(lines).encode()
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_bytes() != data:
        destination.write_bytes(data)
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate", action="store_true")
    args = parser.parse_args()
    try:
        result = generate() if args.generate else verify()
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, UnicodeError) as error:
        parser.exit(1, "web assets: " + str(error) + "\n")
