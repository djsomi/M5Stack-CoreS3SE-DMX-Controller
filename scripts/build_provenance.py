"""Build-input identity, separate from SemVer and the accepted Git base revision."""
from pathlib import Path
import hashlib
import re
import subprocess

INPUTS = ("src", "include", "scripts", "patches", "partitions", "web", "platformio.ini")


def git(root, *args):
    return subprocess.check_output(
        ["git", "--no-optional-locks", "-c", "safe.directory=" + Path(root).as_posix(),
         "-C", str(root), *args])


def versions(root):
    text = (Path(root) / "include/firmware_version.h").read_text(encoding="utf-8")
    def value(name):
        match = re.search(r'^#define ' + name + r' "([0-9A-Za-z.+-]+)"$', text, re.M)
        if not match:
            raise ValueError("missing/invalid release version: " + name)
        return match.group(1)
    return value("DMX_FIRMWARE_VERSION"), value("DMX_TARGET_RELEASE")


def identity(root, environment, source_digest):
    root = Path(root)
    commit = git(root, "rev-parse", "HEAD").decode().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("build requires a verifiable Git HEAD")
    # Unrelated historical/untracked folders do not mark firmware inputs modified.
    changed = git(root, "diff", "HEAD", "--name-only", "--", *INPUTS).strip()
    extra = git(root, "ls-files", "--others", "--exclude-standard", "-z", "--", *INPUTS)
    extra = [p for p in extra.decode().split("\0") if p and "__pycache__" not in Path(p).parts]
    modified = bool(changed or extra)
    version, target = versions(root)
    build_id = version + "+" + commit[:12] + (".modified" if modified else "") + "." + source_digest[:12]
    return dict(schema=1, version=version, target_release=target, status="stable" if version == target and not modified else "local-candidate",
                source_commit=commit, source_modified=modified, source_sha256=source_digest,
                environment=environment, build_id=build_id)


def input_manifest(root):
    root = Path(root)
    rows = []
    for folder in INPUTS:
        base = root / folder
        paths = sorted(base.rglob("*")) if base.is_dir() else [base]
        for path in paths:
            if not path.is_file() or "__pycache__" in path.parts:
                continue
            data = path.read_bytes()
            if path.suffix != ".gz":
                data = data.replace(b"\r\n", b"\n")
            rows.append(dict(path=path.relative_to(root).as_posix(),
                             sha256=hashlib.sha256(data).hexdigest(), bytes=len(data)))
    return dict(schema=1, normalization="LF for text; gzip bytes unchanged", files=rows)
