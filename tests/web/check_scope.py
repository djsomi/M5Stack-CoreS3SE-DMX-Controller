"""Stage6B asset-only scope gate; every existing firmware/driver file is frozen."""
from pathlib import Path
import hashlib
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BASE = "294b8b8ea21d139555a7222f16b5eeff00d16091"
PREFIXES = ("src", "include", "patches", "partitions", "scripts")
ALLOWED = {"scripts/web_asset_pipeline.py", "scripts/web_ui_bundle.py"}


def check():
    entries = subprocess.check_output(["git", "-c", "safe.directory=" + ROOT.as_posix(),
        "ls-tree", "-rz", BASE, "--", *PREFIXES, "platformio.ini"], cwd=ROOT)
    known = set()
    for entry in entries.split(b"\0"):
        if not entry:
            continue
        metadata, raw_name = entry.split(b"\t", 1)
        name = raw_name.decode("utf-8"); known.add(name)
        if name in ALLOWED:
            continue
        data = (ROOT / name).read_bytes().replace(b"\r\n", b"\n")
        oid = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        assert oid == metadata.split()[2].decode(), "Protected Stage6A file changed: " + name
    for folder in PREFIXES:
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                name = path.relative_to(ROOT).as_posix()
                assert name in known or name in ALLOWED, "Unexpected firmware/build input: " + name
    print("PASS exact Stage6A firmware/HTTP/WS/domain/DMX/USB/canvas/partition/package scope; only UI composer/pipeline changes allowed")


if __name__ == "__main__":
    check()
