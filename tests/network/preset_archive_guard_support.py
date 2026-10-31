"""Exact inverse of this reviewed archive feature for historical freeze guards.

Current functionality is exercised by preset_archive host and frontend/browser
tests. Older whole-file scope tests retain their original baselines, not a
blanket exception for changed Controller/storage/network inputs.
"""
from pathlib import Path
import hashlib,json
DELTAS=json.loads(Path(__file__).with_name('preset_archive_delta.json').read_text(encoding='utf-8'))
MARKERS={'include/controller_types.h':'replace_presets','include/websocket_bridge.h':'archive_result','include/websocket_service.h':'prepareArchive',
 'src/command_dispatcher.cpp':'replace_presets','src/dmx_controller.cpp':'captureArchive','src/dmx_controller.h':'preset_archive',
 'src/preset_store.h':'PresetArchiveKv','src/src.cpp':'prepareArchive','src/websocket_bridge.cpp':'archiveRequest',
 'src/websocket_protocol.cpp':'presets.result','src/websocket_service.cpp':'prepareArchive','scripts/web_ui_bundle.py':'"archives"',
 'web/src/js/commands.js':'completeArchive','web/src/js/protocol.js':'presets.result','web/src/js/transport.js':'sendArchive','web/src/js/app.js':'archiveView',
 'src/view_sender.h':'Result::storage_full','web/src/js/actions.js':'result.domain === 14','web/src/js/control.js':'function operationDelay(limit'}
def prior(name,text):
    if name not in DELTAS:return text
    change=DELTAS[name];digest=hashlib.sha256(text.encode()).hexdigest()
    if digest==change['before']:return text
    if digest!=change['after']:
        assert MARKERS[name] not in text,'Unreviewed preset archive delta: '+name
        return text
    for after,before in change['patches']:
        assert text.count(after)==1,name
        text=text.replace(after,before,1)
    assert hashlib.sha256(text.encode()).hexdigest()==change['before'],name
    return text

def check_new():
    root=Path(__file__).resolve().parents[2]
    for name,digest in DELTAS['_new'].items():
        assert hashlib.sha256((root/name).read_text(encoding='utf-8').encode()).hexdigest()==digest,'Unreviewed archive input: '+name
