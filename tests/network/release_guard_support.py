"""Remove only reviewed read-only release identity additions for historical guards."""
from pathlib import Path
import subprocess
from fader_guard_support import prior as before_fader

ROOT = Path(__file__).resolve().parents[2]
BASE = "9fdf07ee02340003e2ac4efc3daf61ce8c787062"


def prior(name, text):
    text = before_fader(name, text)
    if name == "src/hardware_diagnostics.cpp":
        text = text.replace('#include "touch_configuration_snapshot.h"\n', '', 1)
        text = text.replace('    touch_configuration::captureStartup();\n', '', 1)
        text = text.replace('''    } else if (touch_configuration::pending()) {
        length = static_cast<int>(touch_configuration::formatNext(line, sizeof(line)));
''', '', 1)
        text = text.replace(' && !touch_configuration::pending()', '', 1)
    if name in ("scripts/embed_web_assets.py", "scripts/web_asset_pipeline.py"):
        if name == "scripts/embed_web_assets.py":
            text = text.replace('from touch_acquisition_trace import install as install_touch_trace\ninstall_touch_trace(env, root)\n', '', 1)
            marker = "from build_provenance import identity\n"
            if marker not in text:
                return text
            text = text.replace(marker, "", 1)
            text = text.replace('provenance = identity(root, profile, source_digest)\nemit_cpp(root, generated, profile, source_digest, provenance)', 'emit_cpp(root, generated, profile, source_digest)', 1)
            text = text.replace('''    if identity(root, profile, source_digest) != provenance:
        raise RuntimeError("Git/version identity changed during build; rebuild")
    application = (build / "firmware.bin").read_bytes()
    for key in ("version", "source_commit", "build_id"):
        if provenance[key].encode() not in application:
            raise RuntimeError("missing linked firmware provenance: " + key)
    receipt["provenance"] = provenance
''', '', 1)
        else:
            marker = "def emit_cpp(root, destination, profile, source_digest, provenance=None):"
            if marker not in text:
                return text
            text = text.replace(marker, "def emit_cpp(root, destination, profile, source_digest):", 1)
            text = text.replace('''    if provenance is not None:
        lines += ['#include "firmware_version.h"', 'namespace {',
                  'const firmware_build::Info build_info = {%s,%s,%s,%s,%s,%s};' % (
                      json.dumps(provenance["version"]), json.dumps(provenance["source_commit"]),
                      json.dumps(profile), json.dumps(source_digest), json.dumps(provenance["build_id"]),
                      "true" if provenance["source_modified"] else "false"),
                  '}', 'const firmware_build::Info& firmware_build::info() { return build_info; }', '']
''', '', 1)
        old = subprocess.check_output(['git', '-c', 'safe.directory='+ROOT.as_posix(), 'show', BASE+':'+name], cwd=ROOT).decode('utf-8')
        assert text == old, "Unreviewed release identity hook delta: " + name
        return text
    if name not in ("src/network_service.cpp", "src/hardware_diagnostics.cpp"):
        return text
    include = '#include "firmware_version.h"\n'
    if include not in text:
        return text
    assert text.count(include) == 1
    text = text.replace(include, "", 1)
    if name == "src/network_service.cpp":
        text = text.replace('        const auto& identity=firmware_build::info();\n', '', 1)
        text = text.replace(r'{\"version\":\"%s\",\"source_commit\":\"%s\",\"source_modified\":%s,\"build_id\":\"%s\",\"build\":', r'{\"build\":', 1)
        text = text.replace('            identity.version,identity.commit,identity.modified?"true":"false",identity.build_id,\n', '', 1)
    else:
        text = text.replace('[S0] build=%s commit=%s modified=%u environment=%s inputs=%s date=%s_%s', '[S0] build=v0.2.0-base/32c0571+stage0 date=%s_%s', 1)
        text = text.replace('            firmware_build::info().build_id, firmware_build::info().commit,\n            firmware_build::info().modified ? 1U : 0U, firmware_build::info().environment,\n            firmware_build::info().source_sha256,\n', '', 1)
    old = subprocess.check_output(['git', '-c', 'safe.directory='+ROOT.as_posix(), 'show', BASE+':'+name], cwd=ROOT).decode('utf-8')
    assert text == old, "Unreviewed release identity delta: " + name
    return text
