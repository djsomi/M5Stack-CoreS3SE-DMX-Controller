"""Fresh/repeated/partial/unknown patch tests, entirely in ignored temp trees."""
from pathlib import Path
import importlib.util
import json
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("tx_patch", ROOT / "scripts/esp_dmx_tx_patch.py")
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
upstream = Path(sys.argv[1]).resolve()


def contents(root):
    return {str(p.relative_to(root)): p.read_bytes() for p in root.rglob("*") if p.is_file()}


with tempfile.TemporaryDirectory(prefix="tx-patch-", dir=ROOT / "dist") as temp:
    temp = Path(temp)

    def fresh(name):
        dest = temp / name
        shutil.copytree(upstream, dest, ignore=shutil.ignore_patterns(".git", ".pio", "__pycache__"))
        for n, sha in patch.BEFORE.items():
            assert patch.digest((dest / "src" / n).read_text(encoding="utf-8")) == sha
        return dest

    def rejects(dest):
        before = contents(dest)
        try:
            patch.apply(dest / "src")
        except RuntimeError:
            pass
        else:
            raise AssertionError("Accepted bad patch state: " + str(dest))
        assert contents(dest) == before, "Failure modified dependency files"

    clean = fresh("clean")
    ll = (clean / "src/impl/dmx_ll.h").read_bytes()
    patch.apply(clean / "src")
    applied = contents(clean)
    patch.apply(clean / "src")
    assert contents(clean) == applied
    assert (clean / "src/impl/dmx_ll.h").read_bytes() == ll
    for n, sha in patch.AFTER.items():
        assert patch.digest((clean / "src" / n).read_text(encoding="utf-8")) == sha
    partial = fresh("partial")
    shutil.copyfile(clean / "src/esp_dmx.c", partial / "src/esp_dmx.c")
    rejects(partial)
    missing_asset = temp / "missing-asset"
    shutil.copytree(clean, missing_asset)
    (missing_asset / "src/impl/tx_core.h").unlink()
    rejects(missing_asset)
    unknown = fresh("unknown")
    with (unknown / "src/impl/intr_handlers.h").open("a", encoding="utf-8") as out:
        out.write("\n/* unknown source */\n")
    rejects(unknown)
    unknown_hal = fresh("unknown-hal")
    with (unknown_hal / "src/impl/dmx_hal.h").open("a", encoding="utf-8") as out:
        out.write("\n/* unknown HAL */\n")
    rejects(unknown_hal)
    missing = fresh("missing-source")
    (missing / "src/esp_dmx.c").unlink()
    rejects(missing)
    wrong_version = fresh("wrong-version")
    manifest = wrong_version / "library.json"
    data = json.loads(manifest.read_text(encoding="utf-8"))
    data["version"] = "4.1.0"
    manifest.write_text(json.dumps(data), encoding="utf-8")
    rejects(wrong_version)
    assets = temp / "assets"
    shutil.copytree(patch.ASSETS, assets)
    patch.ASSETS = assets
    with (assets / "tx_api.inc").open("a", encoding="utf-8") as out:
        out.write("\n/* unreviewed template */\n")
    rejects(clean)  # Even a fully installed old patch cannot hide changed repository templates.
print("PASS fresh/repeated patch, unchanged S3 getters, atomic prevalidation, mixed/missing/unknown/version/template rejection")
