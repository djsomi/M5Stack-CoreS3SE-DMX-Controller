"""Create an audited separate-image bundle. No FULL merge, erase, serial access or upload."""
import argparse
import json
from pathlib import Path
import shutil

from web_asset_pipeline import ROOT, verify, build_source_digest
from firmware_layout import sha, write_json

ENVIRONMENTS = ("cores3se_unit", "cores3se_base", "cores3se_unit_diagnostics", "cores3se_base_diagnostics")


def package(root, environment, destination):
    build = root / ".pio/build" / environment
    receipt = json.loads((build / "infrastructure.json").read_text())
    assets = verify(root)
    if receipt["profile"] != environment or receipt["source_sha256"] != build_source_digest(root):
        raise ValueError("stale/mixed-profile build; run pio run for this environment first")
    if receipt["assets_sha256"] != assets["bundle_sha256"] or receipt["packaging"] != "separate-images-only":
        raise ValueError("asset/layout packaging mismatch")
    # The post-build verifier copied the audited SDK OTA initialization file.
    # Packaging never writes a flat gap across NVS.
    paths = {c["file"]: build / c["file"] for c in receipt["components"]}
    ota = root / ".pio/build" / environment / "boot_app0.bin"
    if not ota.exists():
        raise ValueError("missing audited boot_app0.bin; rebuild environment")
    for component in receipt["components"]:
        data = paths[component["file"]].read_bytes()
        if len(data) != component["size"] or sha(data) != component["sha256"]:
            raise ValueError("stale/modified image: " + component["file"])
    destination.mkdir(parents=True, exist_ok=False)
    for name, path in paths.items():
        shutil.copyfile(path, destination / name)
    write_json(destination / "manifest.json", receipt)
    names = sorted(paths) + ["manifest.json"]
    (destination / "SHA256SUMS.txt").write_text("".join(
        sha((destination / name).read_bytes()) + "  " + name + "\n" for name in names), encoding="ascii", newline="\n")
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", required=True, choices=ENVIRONMENTS)
    parser.add_argument("--output", type=Path, required=True, help="NEW directory; never overwritten")
    args = parser.parse_args()
    try:
        result = package(ROOT, args.env, args.output)
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, "packaging: " + str(error) + "\n")
