"""Minimal, verifiable build-source snapshot. No refs/history from the publisher."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import re

COMMIT = "59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b"
TAG = "v0.3.0"
TAG_OBJECT = "1c9c388388a31be701c4021bdacb3d70e89906b9"
SOURCE_SHA256 = "cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232"
PREFIXES = ("src/", "include/", "scripts/", "patches/", "partitions/", "web/")
ROOT_FILES = ("platformio.ini", ".gitignore", ".gitattributes", "LICENSE", "CMakeLists.txt")


def git(root, *args, data=None):
    env = os.environ.copy()
    env["GIT_NO_LAZY_FETCH"] = "1"
    return subprocess.check_output(["git", "--no-optional-locks", "-c",
        "safe.directory=" + Path(root).resolve().as_posix(), "-C", str(root), *args],
        input=data, env=env, stderr=subprocess.PIPE)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_path(name):
    if not isinstance(name, str) or "\\" in name or any(p in ("", ".", "..", ".git") for p in name.split("/")):
        raise ValueError("unsafe source path")
    if not (name.startswith(PREFIXES) or name in ROOT_FILES):
        raise ValueError("non-build source path")
    return name


def batch(root, objects):
    output = git(root, "cat-file", "--batch", data="".join(oid + "\n" for oid in objects).encode())
    offset = 0
    result = {}
    for oid in objects:
        end = output.index(b"\n", offset)
        actual, kind, size = output[offset:end].decode().split()
        size = int(size); offset = end + 1
        body = output[offset:offset + size]; offset += size + 1
        if actual != oid or hashlib.sha1(kind.encode() + b" " + str(size).encode() + b"\0" + body).hexdigest() != oid:
            raise ValueError("Git object identity mismatch")
        result[oid] = (kind, body)
    return result


def create(repository, destination):
    repository, destination = Path(repository), Path(destination)
    if git(repository, "rev-parse", TAG).decode().strip() != TAG_OBJECT or git(repository, "rev-parse", TAG + "^{commit}").decode().strip() != COMMIT:
        raise ValueError("frozen tag changed")
    rows = []
    objects = {COMMIT, TAG_OBJECT, git(repository, "rev-parse", COMMIT + "^{tree}").decode().strip()}
    for entry in git(repository, "ls-tree", "-rzt", COMMIT).split(b"\0"):
        if not entry: continue
        metadata, name = entry.split(b"\t", 1)
        mode, kind, oid = metadata.decode().split(); name = name.decode("utf-8")
        if kind == "tree": objects.add(oid)
        elif name.startswith(PREFIXES) or name in ROOT_FILES:
            if kind != "blob" or mode not in ("100644", "100755"):
                raise ValueError("unexpected source object type")
            objects.add(oid); rows.append(dict(path=name, git_blob=oid, mode=mode))
    blobs = batch(repository, sorted({row["git_blob"] for row in rows}))
    for row in rows:
        body = blobs[row["git_blob"]][1]
        row.update(bytes=len(body), sha256=sha(body))
    pack = git(repository, "pack-objects", "--stdout", data="".join(oid + "\n" for oid in sorted(objects)).encode())
    manifest = dict(schema=1, commit=COMMIT, tag=TAG, tag_object=TAG_OBJECT,
        source_sha256=SOURCE_SHA256, pack_sha256=sha(pack), objects=sorted(objects), files=rows,
        history="one shallow commit; build-input blobs only; non-build blobs intentionally omitted")
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "objects.pack").write_bytes(pack)
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def restore(snapshot, destination):
    snapshot, destination = Path(snapshot), Path(destination)
    m = json.loads((snapshot / "manifest.json").read_text(encoding="utf-8"))
    pack = (snapshot / "objects.pack").read_bytes()
    if (m["commit"] != COMMIT or m["tag"] != TAG or m["tag_object"] != TAG_OBJECT or
            m["source_sha256"] != SOURCE_SHA256 or sha(pack) != m["pack_sha256"]):
        raise ValueError("wrong/corrupt frozen source")
    for row in m["files"]:
        source_path(row["path"])
    if len({row["path"] for row in m["files"]}) != len(m["files"]):
        raise ValueError("duplicate source path")
    destination.mkdir(parents=True, exist_ok=False)
    subprocess.run(["git", "init", "--quiet", str(destination)], check=True)
    git(destination, "config", "core.autocrlf", "false")  # This isolated reconstruction only.
    git(destination, "config", "core.sparseCheckout", "true")
    git(destination, "config", "remote.omitted.promisor", "true")  # No URL / no network fallback.
    result = git(destination, "index-pack", "--stdin", data=pack).decode().strip().split()[-1]
    index = destination / ".git/objects/pack" / ("pack-" + result + ".idx")
    packed = git(destination, "verify-pack", "-v", str(index.resolve())).decode().splitlines()
    actual = {line.split()[0] for line in packed if re.match(r"^[0-9a-f]{40} ", line)}
    if actual != set(m["objects"]) or len(actual) != len(m["objects"]):
        raise ValueError("unlisted/duplicate Git objects in source pack")
    (destination / ".git/objects/pack" / ("pack-" + result + ".promisor")).write_bytes(b"")
    (destination / ".git/shallow").write_text(COMMIT + "\n", encoding="ascii")
    ref = destination / ".git/refs/tags" / TAG
    ref.parent.mkdir(parents=True, exist_ok=True); ref.write_text(TAG_OBJECT + "\n", encoding="ascii")
    (destination / ".git/info/sparse-checkout").write_text(
        "".join("/" + p + "\n" for p in (*PREFIXES, *ROOT_FILES)), encoding="utf-8", newline="\n")
    git(destination, "checkout", "--quiet", "--detach", COMMIT)
    objects = batch(destination, m["objects"])
    if sum(kind == "commit" for kind, body in objects.values()) != 1 or sum(kind == "tag" for kind, body in objects.values()) != 1:
        raise ValueError("unexpected repository history/ref objects")
    allowed = {row["git_blob"] for row in m["files"]}
    if {oid for oid, (kind, body) in objects.items() if kind == "blob"} != allowed:
        raise ValueError("unexpected non-build/private blob")
    tree_files = {}
    for entry in git(destination, "ls-tree", "-rz", COMMIT).split(b"\0"):
        if not entry: continue
        metadata, name = entry.split(b"\t", 1)
        name = name.decode("utf-8")
        if name.startswith(PREFIXES) or name in ROOT_FILES:
            mode, kind, oid = metadata.decode().split()
            tree_files[name] = (mode, kind, oid)
    if tree_files != {row["path"]: (row["mode"], "blob", row["git_blob"]) for row in m["files"]}:
        raise ValueError("manifest differs from immutable Git tree")
    for row in m["files"]:
        data = (destination / row["path"]).read_bytes()
        if len(data) != row["bytes"] or sha(data) != row["sha256"]:
            raise ValueError("restored source mismatch: " + row["path"])
    if git(destination, "status", "--porcelain").strip():
        raise ValueError("restored source is not clean")
    return m


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("operation", choices=("create", "restore")); p.add_argument("source", type=Path); p.add_argument("destination", type=Path)
    a = p.parse_args()
    m = create(a.source, a.destination) if a.operation == "create" else restore(a.source, a.destination)
    print("Verified frozen build source:", m["commit"], len(m["files"]), "files;", len(m["objects"]), "objects")
