"""Immutable P documents for archival packaging tests; never release payload.

The public source no longer tracks obsolete stage/migration reports. Reconstruct
these test inputs from their real preserved commit rather than depending on local
ignored copies, synthesizing notices, or weakening historical inventory checks.
"""
from pathlib import Path
import io
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
PUBLICATION = "19a4862b74e310a763f9231d76e7baaeed3ec16f"
_archive = None

def restore(destination):
    global _archive
    destination = Path(destination).resolve()
    assert (ROOT / "dist").resolve() in destination.parents
    destination.mkdir(parents=True, exist_ok=True)
    if _archive is None:
        _archive = subprocess.check_output(["git", "--no-optional-locks", "-c",
            "safe.directory=" + ROOT.as_posix(), "-C", str(ROOT),
            "archive", "--format=tar", PUBLICATION])
    with tarfile.open(fileobj=io.BytesIO(_archive), mode="r:") as archive:
        for item in archive:
            path = destination / item.name
            assert path.resolve().is_relative_to(destination)
            if item.isdir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                assert item.isfile(), "Unexpected archival fixture type"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.extractfile(item).read())
    return destination
