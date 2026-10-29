"""Extract actual hardware/NVS adapter for host mocks; writes only ignored dist."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/'src/src.cpp').read_text(encoding='utf-8')
def function(signature):
    start=source.index(signature);opening=source.index('{',start);depth=0
    for i in range(opening,len(source)):
        if source[i]=='{':depth+=1
        if source[i]=='}':
            depth-=1
            if not depth:return source[start:i+1]+'\n'
    raise AssertionError(signature)
out=ROOT/'dist/device-settings-20261029';out.mkdir(exist_ok=True)
adapter=source[source.index('class ApplicationSettingsBackend'):source.index('static ApplicationSettingsBackend')]
(out/'settings_adapter.h').write_text(function('static uint8_t brightnessFromLevel(')+function('static uint8_t speakerVolumeFromLevel(')+adapter,encoding='utf-8')
view=(ROOT/'src/view_sender.h').read_text(encoding='utf-8')
assert 'if (!tp.state) persistDeviceSettings();' in view
assert 'settings_device_revision' in view
assert 'if(!display_sleeping)M5.Display.setBrightness' in adapter
assert 'if(!battery_warning_volume_override)' in adapter and '.tone(' not in adapter
assert 'putUChar(name(key),value)==1&&prefs.getUChar(name(key),255)==value' in adapter
assert source.index('dmx_transport.poll(')<source.index('websocket_service::ownerPoll(')
print('PASS extracted actual NVS/hardware adapter, release persistence and unchanged owner scheduling boundary')
