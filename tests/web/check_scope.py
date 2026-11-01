"""Stage6B scope gate; domain/driver inputs frozen, named network diagnostic/I/O exceptions."""
from pathlib import Path
import hashlib
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"network"))
from preset_loaded_guard_support import prior, check_new_header
from device_settings_guard_support import restore, check_header
from wifi_guard_support import check_new
from release_guard_support import prior as before_release
from preset_archive_guard_support import check_new as check_archive

ROOT = Path(__file__).resolve().parents[2]
BASE = "294b8b8ea21d139555a7222f16b5eeff00d16091"
PREFIXES = ("src", "include", "patches", "partitions", "scripts")
ALLOWED = {"include/captured_fader_touch.h", "include/physical_fader_diagnostics.h", "src/physical_fader_diagnostics.cpp", "include/firmware_version.h", "scripts/build_provenance.py", "scripts/package_release_candidate.py", "scripts/embed_web_assets.py", "scripts/web_asset_pipeline.py", "scripts/web_ui_bundle.py", "src/network_service.cpp", "include/status_ssid.h",
    "src/websocket_diagnostics.cpp", "include/websocket_diagnostics.h", "src/websocket_bridge.cpp", "include/preset_double_tap.h", "include/device_settings.h", "include/device_maintenance.h", "include/maintenance_transport.h", "include/wifi_configuration.h", "include/wifi_credential_transaction.h", "include/wifi_wire.h", "src/wifi_credential_store.h"}


def check():
    # Publication-only offline tooling: never imported by firmware/build hooks.
    ALLOWED.add('scripts/distribution_notices.py')
    ALLOWED.update({'scripts/frozen_source.py', 'scripts/publication_materials.py',
                    'scripts/publication_release.py', 'scripts/validate_relink.py',
                    'scripts/source_material_inventory.py'})
    ALLOWED.add('scripts/assemble_stable_release.py')  # Offline; no firmware/build-hook import.
    check_archive()
    ALLOWED.update({'include/preset_archive.h','include/preset_archive_store.h'})
    ALLOWED.update({'include/touch_acquisition_diagnostics.h','src/touch_acquisition_diagnostics.cpp','scripts/touch_acquisition_trace.py'})
    ALLOWED.update({'include/touch_configuration_snapshot.h','src/touch_configuration_snapshot.cpp'})
    ALLOWED.update({'include/thdiff_experiment.h','src/thdiff_experiment.cpp'})
    # Preserve the accepted split; allow only the reviewed action presentation block.
    for path in (ROOT / "web/src/js").glob("*.js"):
        if path.name == "archives.js":
            continue  # New isolated factory, exercised by file/transport/browser tests.
        original = subprocess.check_output(["git", "-c", "safe.directory=" + ROOT.as_posix(),
            "show", "4e6fbbeada642e5dc8345421a1d11eb9acc6baa5:" + path.relative_to(ROOT).as_posix()], cwd=ROOT)
        if path.name == "presets.js":
            old = b'''      node("preset-load").disabled = !available || occupied !== 1 || !!confirmation;
      node("preset-save").disabled = !available || ![0,1,2].includes(occupied) || !!confirmation;
      node("preset-clear").disabled = !available || occupied !== 1 || !!confirmation;
'''
            new = b'''      const unavailable = !ready || !state.current().sender;
      node("preset-load").disabled = unavailable || occupied !== 1 || !!confirmation;
      node("preset-save").disabled = unavailable || ![0,1,2].includes(occupied) || !!confirmation;
      node("preset-clear").disabled = unavailable || occupied !== 1 || !!confirmation;
      // Temporary admission blocking preserves appearance and keyboard focus.
      for (const id of ["preset-load", "preset-save", "preset-clear"]) {
        const button = node(id);
        button.setAttribute("aria-disabled", String(button.disabled || !available));
      }
'''
            assert original.count(old) == 1
            original = original.replace(old, new)
        assert restore(path.relative_to(ROOT).as_posix(),path.read_text(encoding="utf-8")).encode() == original, "Unexpected application factory change: " + path.name
    check_new()
    check_new_header()
    check_header()
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
        data = prior(name,before_release(name,(ROOT / name).read_text(encoding="utf-8"))).encode("utf-8")
        oid = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        assert oid == metadata.split()[2].decode(), "Protected Stage6A file changed: " + name
    for folder in PREFIXES:
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                name = path.relative_to(ROOT).as_posix()
                assert name in known or name in ALLOWED, "Unexpected firmware/build input: " + name
    bridge = restore("src/websocket_bridge.cpp",(ROOT / "src/websocket_bridge.cpp").read_text(encoding="utf-8")).encode()
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
