"""Stage6B scope gate; domain/driver inputs frozen, named network diagnostic/I/O exceptions."""
from pathlib import Path
import hashlib
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"network"))
from preset_loaded_guard_support import prior, check_new_header

ROOT = Path(__file__).resolve().parents[2]
BASE = "294b8b8ea21d139555a7222f16b5eeff00d16091"
PREFIXES = ("src", "include", "patches", "partitions", "scripts")
ALLOWED = {"scripts/web_asset_pipeline.py", "scripts/web_ui_bundle.py", "src/network_service.cpp", "include/status_ssid.h",
    "src/websocket_diagnostics.cpp", "include/websocket_diagnostics.h", "src/websocket_bridge.cpp", "include/preset_double_tap.h"}


def check():
    # Delivery-only migration: all fourteen approved application factories stay exact.
    for path in (ROOT / "web/src/js").glob("*.js"):
        original = subprocess.check_output(["git", "-c", "safe.directory=" + ROOT.as_posix(),
            "show", "03872b99b1ccfb256f010342d8e4e8449c515b80:" + path.relative_to(ROOT).as_posix()], cwd=ROOT)
        assert path.read_bytes().replace(b"\r\n", b"\n") == original, "Application factory changed during delivery split: " + path.name
    check_new_header()
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
        data = prior(name,(ROOT / name).read_text(encoding="utf-8")).encode("utf-8")
        oid = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        assert oid == metadata.split()[2].decode(), "Protected Stage6A file changed: " + name
    for folder in PREFIXES:
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                name = path.relative_to(ROOT).as_posix()
                assert name in known or name in ALLOWED, "Unexpected firmware/build input: " + name
    bridge = (ROOT / "src/websocket_bridge.cpp").read_bytes().replace(b"\r\n", b"\n")
    original = subprocess.check_output(["git", "-c", "safe.directory=" + ROOT.as_posix(), "show", "5b3cea7:src/websocket_bridge.cpp"], cwd=ROOT)
    old = b"    if(result.revision_after!=result.revision_before) {\n        {Guard guard(self.lock_);++self.metrics_.controller_events;self.metrics_.last_controller_revision=result.revision_after;}\n"
    new = b"    const bool changed=result.revision_after!=result.revision_before;\n    if(changed) {\n        {Guard guard(self.lock_);++self.metrics_.controller_events;self.metrics_.last_controller_revision=result.revision_after;}\n    }\n    // Even an unchanged successful load supersedes older browser gestures.\n    // Publish the existing bulk shape/sequence; never fabricate a revision.\n    const bool loaded=command.type==CommandType::load_preset&&result.domain==DmxController::Result::ok&&\n        (result.status==CommandStatus::applied||result.status==CommandStatus::no_change||result.status==CommandStatus::transport_error);\n    if(changed||loaded) {\n"
    expected = original.replace(old, new)
    expected = expected.replace(
        b"            for(unsigned i=0;i<8;++i)presets_[i]=uint8_t(controller_.presetState(uint8_t(i)));\n            presets_ready_=true;",
        b"            presets_ready_=true;\n            for(unsigned i=0;i<8;++i) {\n                presets_[i]=uint8_t(controller_.presetState(uint8_t(i)));\n                if(presets_[i]==uint8_t(DmxPresetStorage::SlotState::error))presets_ready_=false;\n            }")
    expected = expected.replace(
        b"        Event e=state(Kind::snapshot,r.id);\n        Guard guard(lock_);",
        b"        uint8_t previous_presets[8];memcpy(previous_presets,presets_,8);\n        Event e=state(Kind::snapshot,r.id);\n        if(memcmp(previous_presets,e.presets,8)!=0) {\n            // A read after a persistence error may discover metadata changes\n            // without a domain revision. Notify peers before the newer snapshot.\n            e.kind=Kind::universe;e.id=0;broadcast(e);\n            e.kind=Kind::snapshot;e.id=r.id;e.sequence=++sequence_;\n        }\n        Guard guard(lock_);")
    expected = expected.replace(b"    if(changed||loaded) {", b"    const bool storage_refresh=result.status==CommandStatus::storage_error&&\n        (command.type==CommandType::load_preset||command.type==CommandType::save_preset||\n         command.type==CommandType::clear_preset||command.type==CommandType::copy_preset);\n    if(changed||loaded||storage_refresh) {")
    expected = expected.replace(b"    e.blackout=controller_.blackoutActive();e.sender=sender_;\n", b"    e.blackout=controller_.blackoutActive();e.sender=sender_;\n    e.loaded=controller_.loadedPreset();e.modified=controller_.loadedPresetModified();\n")
    assert original.count(old) == 1 and bridge == expected, "Unexpected Stage6B.4 Bridge change"
    print("PASS exact Stage6A firmware/HTTP/WS/domain/DMX/USB/canvas/partition/package scope; only UI composer/pipeline and named WS/AP diagnostics/sub-ms I/O guard and exact preset observation/publication exceptions")


if __name__ == "__main__":
    check()
