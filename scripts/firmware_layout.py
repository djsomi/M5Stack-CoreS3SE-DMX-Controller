"""Strict Stage 4B layout/image validation. Never opens a serial port or writes flash."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys

FLASH_SIZE = 0x1000000
LAYOUT_ID = "cores3se-16m-ota6-v1"
EXPECTED = [
    ("nvs", 1, 2, 0x9000, 0x5000),
    ("otadata", 1, 0, 0xe000, 0x2000),
    ("app0", 0, 0x10, 0x10000, 0x600000),
    ("app1", 0, 0x11, 0x610000, 0x600000),
    ("spiffs", 1, 0x82, 0xc10000, 0x3e0000),
    ("coredump", 1, 3, 0xff0000, 0x10000),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def partition_sdk(framework):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("stage4_partitions", Path(framework) / "tools/gen_esp32part.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate_table(table):
    table.verify()
    table.verify_size_fits(FLASH_SIZE)
    rows = [(p.name, p.type, p.subtype, p.offset, p.size) for p in table]
    if rows != EXPECTED or any(p.get_flags_list() for p in table):
        raise ValueError("partition table differs from approved cores3se-16m-ota6-v1 layout")
    if max(p.offset + p.size for p in table) != FLASH_SIZE:
        raise ValueError("partition extent must equal exactly 16 MiB")
    return rows


def validate_image(path, esptool_dir):
    sys.path.insert(0, str(esptool_dir))
    from esptool.bin_image import ESP32S3FirmwareImage
    data = Path(path).read_bytes()
    image = ESP32S3FirmwareImage(io.BytesIO(data))
    if image.chip_id != 9 or image.flash_size_freq != 0x4f or image.flash_mode != 2:
        raise ValueError("expected ESP32-S3 / 16MB / 80MHz / DIO image header: " + str(path))
    if image.checksum != image.calculate_checksum():
        raise ValueError("invalid image segment checksum: " + str(path))
    if not image.append_digest or image.stored_digest != image.calc_digest:
        raise ValueError("missing/invalid image SHA256: " + str(path))
    return data


def inspect_build(root, build, framework, esptool_dir, images, profile, source_digest, asset_manifest):
    root, build, framework = Path(root), Path(build), Path(framework)
    sdk = partition_sdk(framework)
    csv = root / "partitions/cores3se_16mb_ota.csv"
    configured = sdk.PartitionTable.from_csv(csv.read_text())
    validate_table(configured)
    actual = sdk.PartitionTable.from_binary((build / "partitions.bin").read_bytes())
    validate_table(actual)
    if configured.to_binary() != (build / "partitions.bin").read_bytes():
        raise ValueError("generated partition binary differs from configured CSV")
    offsets = {Path(path).name: int(str(offset), 0) for offset, path in images}
    expected_offsets = {"bootloader.bin": 0, "partitions.bin": 0x8000,
                        "boot_app0.bin": actual[1].offset, "firmware.bin": actual[2].offset}
    if offsets != expected_offsets or len(images) != 4:
        raise ValueError("unexpected PlatformIO flash images/offsets: " + repr(offsets))
    components = []
    for offset, path in images:
        path = Path(path); offset = int(str(offset), 0); data = path.read_bytes()
        if path.name in ("bootloader.bin", "firmware.bin"):
            validate_image(path, esptool_dir)
        capacity = {"bootloader.bin": 0x8000, "partitions.bin": 0x1000,
                    "boot_app0.bin": 0x2000, "firmware.bin": min(actual[2].size, actual[3].size)}[path.name]
        if not data or len(data) > capacity:
            raise ValueError("image exceeds reserved capacity: " + path.name)
        if path.name == "boot_app0.bin" and data != (framework / "tools/partitions/boot_app0.bin").read_bytes():
            raise ValueError("OTA metadata initialization differs from installed SDK image")
        if offset < 0xe000 and offset + len(data) > 0x9000:
            raise ValueError("sparse image overlaps NVS")
        components.append(dict(file=path.name, offset=offset, size=len(data), sha256=sha(data)))
    ordered = sorted(components, key=lambda c: c["offset"])
    if any(a["offset"] + a["size"] > b["offset"] for a, b in zip(ordered, ordered[1:])):
        raise ValueError("overlapping flash images")
    application = (build / "firmware.bin").read_bytes()
    for entry in asset_manifest["assets"]:
        data = (root / "web/generated" / entry["file"]).read_bytes()
        if data not in application:
            raise ValueError("asset discarded by linker: " + entry["file"])
    for marker in (source_digest, asset_manifest["bundle_sha256"], profile, LAYOUT_ID):
        if marker.encode() not in application:
            raise ValueError("missing linked bundle/provenance marker: " + marker)
    return dict(schema=1, layout=LAYOUT_ID, flash_size=FLASH_SIZE, profile=profile,
                source_sha256=source_digest, assets_sha256=asset_manifest["bundle_sha256"],
                partition_csv_sha256=sha(csv.read_bytes().replace(b"\r\n", b"\n")),
                components=components, packaging="separate-images-only",
                effects="writes bootloader/table/app0 and resets otadata; does not write NVS or reserved storage")


def write_json(path, data):
    Path(path).write_text(json.dumps(data, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
