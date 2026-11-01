"""Reviewed source-distribution scope and notices; never imported by firmware."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = "licenses/source_materials"
MANIFEST = DIRECTORY + "/manifest.json"
SDK = "core/packages/framework-arduinoespressif32/tools/sdk/esp32s3/"
FRAMEWORK = "core/packages/framework-arduinoespressif32/"
PLATFORM = "core/platforms/espressif32/"
REQUIRED_NOTICE_IDS = frozenset(("audited-binary-notices", "boost", "pinned-asio", "esp-dl", "esp-littlefs",
    "pinned-libsodium", "pinned-tinycbor", "unity", "pinned-cmock", "pinned-coap", "pinned-spiffs",
    "pinned-cjson", "pinned-protobuf-c", "pinned-tinyusb", "pinned-nimble", "nimble-notice",
    "pinned-bt-controller", "littlefs-core", "esp-camera", "idf-components_freemodbus_LICENSE",
    "idf-components_console_argtable3_LICENSE", "idf-components_console_linenoise_LICENSE",
    "gnu-font-exception-plain", "freefont-copying", "freefont-authors", "freefont-readme",
    "freefont-preferred-source", "freefont-upstream-license-page", "adafruit-fontconvert",
    "fontconvert-source", "installed-source-notices", "upstream-sdk-source-notices", "nimble-port-notices",
    "ipa-font-license", "efont-copyright", "libb64-dedication", "platformio-license"))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(path):
    return Path(path).read_bytes().replace(b"\r\n", b"\n")


def exclusion(name):
    """Return a reason, never edit a retained upstream source/build recipe."""
    if name.startswith(FRAMEWORK):
        p = name[len(FRAMEWORK):]
        if p.startswith("cores/"): return None
        if p.startswith("libraries/"):
            return None if p.split("/")[1] in ("Preferences", "SPI", "Wire") and "/examples/" not in p else "Unused Arduino library/example; frozen LDF uses only Preferences/SPI/Wire"
        if p.startswith("variants/"):
            return None if p.startswith("variants/esp32s3/") else "Other-board variant; frozen board uses esp32s3"
        if p.startswith("tools/sdk/esp32s3/"):
            relative = p[len("tools/sdk/esp32s3/"):]
            first = relative.split("/")[0]
            if first in ("dio_opi", "dio_qspi", "opi_opi", "opi_qspi", "qio_opi"):
                return "Inactive memory mode; both frozen profiles use qio_qspi"
            if relative.startswith("include/idf_test/") or "/tests/" in relative:
                return "SDK test fixtures; not part of either build's includes"
            return None
        if p.startswith("tools/"):
            return None if p in ("tools/platformio-build.py", "tools/platformio-build-esp32s3.py", "tools/gen_esp32part.py", "tools/partitions/boot_app0.bin") else "Other-target/IDE/upload/debug tool; unchanged PIO build uses the retained scripts"
        return None if p in (".piopm", "package.json", "CMakeLists.txt", "Kconfig.projbuild", "installed.json") else "Arduino IDE/other-target metadata; not an input to the frozen PIO build"
    if name.startswith(PLATFORM):
        p = name[len(PLATFORM):]
        if p.startswith("boards/"):
            return None if p == "boards/esp32-s3-devkitc-1.json" else "Other-board PlatformIO definition"
        if p.startswith(("examples/", ".github/", "monitor/")):
            return "Platform examples/CI/monitor support; not used by compilation/relink"
        return None
    if name.startswith("libdeps/"):
        p = name.split("/", 2)[2]
        if p.startswith(("examples/", "media/", "docs/", ".github/")):
            return "Dependency example/media/documentation outside required source/license material"
        if p.startswith("src/picture_frame/"):
            return "M5GFX desktop-only preview images; included only in the non-ESP_PLATFORM branch"
        return None
    return None


def comments(data):
    try: text = data.decode("utf-8").replace("\r\n", "\n")
    except UnicodeDecodeError: return []
    pattern = r"/\*.*?\*/|(?m:^[ \t]*//[^\n]*(?:\n[ \t]*//[^\n]*)*)"
    return [m.group() for m in re.finditer(pattern, text, re.S)
            if re.search(r"copyright|licen[cs]e|redistribut|public domain|permission is hereby", m.group(), re.I)]


def component(name):
    if name.startswith(PLATFORM): return "platformio-espressif32"
    if name.startswith(SDK):
        rest = name[len(SDK):]; parts = rest.split("/")
        if parts[0] == "include": return "sdk-" + parts[1]
        if rest.endswith(".a"):
            lib = Path(rest).name[3:-2]
            aliases = {"net80211":"esp_wifi", "pp":"esp_wifi", "smartconfig":"esp_wifi", "wapi":"esp_wifi", "coexist":"esp_wifi", "mesh":"esp_wifi", "core":"esp_wifi", "espnow":"esp_wifi", "phy":"esp_phy", "btbb":"bt", "btdm_app":"bt", "cat_face_detect":"esp-dl", "human_face_detect":"esp-dl", "color_detect":"esp-dl", "dl":"esp-dl", "mfn":"esp-dl", "xt_hal":"xtensa", "mbedcrypto":"mbedtls", "mbedx509":"mbedtls", "mbedtls_2":"mbedtls"}
            return "sdk-" + aliases.get(lib, lib)
        return "sdk-build-config"
    if name.startswith(FRAMEWORK):
        rest = name[len(FRAMEWORK):]
        if rest.startswith("cores/esp32/libb64/"): return "libb64"
        if rest.startswith("libraries/Preferences/"): return "arduino-preferences"
        return "arduino-esp32"
    if name.startswith("libdeps/M5GFX/"):
        if "/Fonts/IPA/" in name: return "ipa-font"
        if "/Fonts/efont/" in name: return "efont"
        if "/Fonts/GFXFF/Free" in name: return "gnu-freefont-bitmaps"
        if name.endswith("/Fonts/GFXFF/TomThumb.h"): return "tomthumb"
        if name.endswith("/utility/result.hpp"): return "result"
        for suffix, group in (("lgfx_miniz", "miniz"), ("lgfx_pngle", "pngle"), ("lgfx_tjpgd", "tjpgdec"), ("lgfx_qrcode", "qrcode-richard-moore"), ("lgfx_qoi", "qoi-m5gfx")):
            if Path(name).stem == suffix: return group
        if name.endswith(("/Fonts/glcdfont.h", "/Fonts/GFXFF/license.txt")): return "adafruit-classic-font"
        return "m5gfx"
    if name.startswith("libdeps/M5Unified/"): return "m5unified"
    if name.startswith("libdeps/esp_dmx/"): return "esp-dmx"
    raise ValueError("unclassified dependency file: " + name)


def payload_files(folder):
    for prefix in ("core", "libdeps"):
        for path in sorted((Path(folder) / prefix).rglob("*")):
            if path.is_file() and "__pycache__" not in path.parts:
                yield path.relative_to(folder).as_posix(), path


def classification(name):
    leaf = Path(name).name.lower()
    if any(word in leaf for word in ("license", "copying", "copyright")) or leaf.startswith("readme"):
        return "documentation-or-license-material"
    if name.startswith((SDK, PLATFORM)) or leaf in (".piopm", "package.json", "library.json", "library.properties", "cmakelists.txt", "kconfig.projbuild", "installed.json"):
        return "required-supporting-build-material"
    return "required-for-rebuilding-relinking"


def verify_repository(root=ROOT):
    root = Path(root)
    manifest = json.loads((root / MANIFEST).read_text())
    if manifest.get("schema") != 1 or manifest.get("review_status") != "source-and-notice-reviewed":
        raise ValueError("missing reviewed source-material inventory")
    expected = set()
    for row in manifest["notice_files"]:
        name = row["path"]
        if (not name.startswith(DIRECTORY + "/") and name != "licenses/THIRD_PARTY_LICENSES.txt") or ".." in Path(name).parts:
            raise ValueError("unsafe material notice path")
        data = canonical(root / name) if row.get("text", True) else (root / name).read_bytes()
        if sha(data) != row["sha256"] or len(data) != row["bytes"]:
            raise ValueError("missing/modified material license: " + name)
        expected.add(name)
    actual = {p.relative_to(root).as_posix() for p in (root / DIRECTORY).rglob("*") if p.is_file()}
    if actual != {name for name in expected if name.startswith(DIRECTORY + "/")} | {MANIFEST}:
        raise ValueError("unreviewed/missing material notice file")
    refs = {row["id"] for row in manifest["notice_files"]}
    if refs != REQUIRED_NOTICE_IDS or len(refs) != len(manifest["notice_files"]):
        raise ValueError("missing/duplicate/unreviewed source-material license identity")
    for row in manifest["components"]:
        if not row["license_expression"] or not row["provenance"] or not row["notice_ids"]:
            raise ValueError("incomplete component review: " + row["id"])
        if not set(row["notice_ids"]) <= refs:
            raise ValueError("component refers to absent license: " + row["id"])
        if row["id"] == "sdk-asio" and ("BSL-1.0" not in row["license_expression"] or "boost" not in row["notice_ids"]):
            raise ValueError("actual Asio BSL declaration is not covered")
    return manifest


def install(root, destination):
    root, destination = Path(root), Path(destination)
    manifest = verify_repository(root)
    for name in [MANIFEST] + [row["path"] for row in manifest["notice_files"]]:
        target = destination / name; target.parent.mkdir(parents=True, exist_ok=True)
        text = name == MANIFEST or next(row for row in manifest["notice_files"] if row["path"] == name).get("text", True)
        target.write_bytes(canonical(root / name) if text else (root / name).read_bytes())
    return verify_payload(destination)


def verify_payload(folder):
    folder = Path(folder)
    manifest = verify_repository(folder)
    rows = []
    for name, path in payload_files(folder):
        reason = exclusion(name)
        if reason: raise ValueError("excluded/unneeded file in final distribution: " + name)
        rows.append(dict(path=name, bytes=path.stat().st_size, sha256=sha(path.read_bytes()), component=component(name), classification=classification(name)))
    if rows != manifest["files"]:
        raise ValueError("unreviewed/missing/modified source-build payload")
    components = {r["id"] for r in manifest["components"]}
    if not {r["component"] for r in rows} <= components:
        raise ValueError("file lacks component license coverage")
    bodies = [canonical(folder / row["path"]) for row in manifest["notice_files"] if row.get("text", True)]
    for name, path in payload_files(folder):
        if any(word in path.name.lower() for word in ("license", "copying", "copyright")):
            body = canonical(path).strip(b"\n")
            if not any(body in text for text in bodies):
                raise ValueError("inherited standalone license absent from notice set: " + name)
    return manifest
