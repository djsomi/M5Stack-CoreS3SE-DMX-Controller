"""Remove only reviewed read-only release identity additions for historical guards."""
from pathlib import Path
import subprocess
import hashlib
from fader_guard_support import prior as before_fader

ROOT = Path(__file__).resolve().parents[2]
BASE = "9fdf07ee02340003e2ac4efc3daf61ce8c787062"


def before_icons(name, text):
    if name == "scripts/web_asset_pipeline.py" and "ICONS = " in text:
        assert hashlib.sha256(text.encode()).hexdigest() == "669655d0068efe55226f97f5892f33ef3b29eac8c9e58d46adebe1e2c245202f", "Unreviewed PNG pipeline delta"
        text = subprocess.check_output(['git', '-c', 'safe.directory='+ROOT.as_posix(), 'show',
            '9ba9445f453db585fa86cea3c5c5e32248714c47:'+name], cwd=ROOT).decode('utf-8')
    # Exact inverse of the PNG route additions; keep historical whole-file guards.
    icon_changes = {
        "src/network_service.cpp": (
            ('    const size_t limit=writing?s->budget.txLimit():stage5::max_rx_bytes;\n',
             '    const size_t limit=writing?stage5::max_tx_bytes:stage5::max_rx_bytes;\n'),
            ('    connection->budget.icon_response=!strcmp(asset->content_type,"image/png");\n', ''),
            ('    if(!strcmp(asset->content_encoding,"gzip"))httpd_resp_set_hdr(r,"Content-Encoding","gzip");\n',
             '    httpd_resp_set_hdr(r,"Content-Encoding","gzip");\n')),
        "src/http_policy.h": (
            ('constexpr size_t max_icon_bytes = 32768;\n', ''),
            ('    bool icon_response = false;\n    size_t txLimit() const { return icon_response ? max_icon_bytes + 1024 : max_tx_bytes; }\n', '')),
        "src/http_policy.cpp": (
            ('    const bool icon=!strcmp(asset->content_type,"image/png");\n'
             '    if(!icon&&!acceptsGzip(encoding)) return {406,nullptr,"no-store"};\n'
             '    if(asset->size>(icon?max_icon_bytes:max_asset_bytes)) return {503,nullptr,"no-store"};\n'
             '    if(icon) return {200,asset,"no-cache"};\n',
             '    if(!acceptsGzip(encoding)) return {406,nullptr,"no-store"};\n'
             '    if(asset->size>max_asset_bytes) return {503,nullptr,"no-store"};\n'),
            ('    const size_t limit = writing ? txLimit() : max_rx_bytes;\n',
             '    const size_t limit = writing ? max_tx_bytes : max_rx_bytes;\n')),
    }
    for new, old in icon_changes.get(name, ()):
        if new in text:
            assert text.count(new) == 1, "Unexpected icon delta: " + name
            text = text.replace(new, old, 1)
    return text


def prior(name, text):
    text = before_fader(name, before_icons(name, text))
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
