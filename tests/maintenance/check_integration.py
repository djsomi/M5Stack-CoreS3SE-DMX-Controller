"""Verify original Power Off and corrected owner/render lifecycle against accepted source."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
def read(n):return (root/n).read_text(encoding='utf-8')
def baseline(n):return subprocess.check_output(['git','-c','safe.directory='+root.as_posix(),'show','487fce1:'+n],cwd=root).decode('utf-8')
def function(text,signature):
    a=text.index(signature);b=text.index('{',a);depth=0
    for i in range(b,len(text)):
        if text[i]=='{':depth+=1
        if text[i]=='}':
            depth-=1
            if not depth:return text[a:i+1]
app=read('src/src.cpp');view=read('src/view_sender.h');bridge=read('src/websocket_bridge.cpp')
loop=app.split('void loop(void)',1)[1]
assert loop.index('dmx_transport.poll(')<loop.index('device_maintenance.poll(')<loop.index('websocket_service::ownerPoll(')<loop.index('command_dispatcher.process();')
assert 'scene_mode == mode_sender&&!device_maintenance.committed()' in loop
assert '!device_maintenance.active()&&!view_sender.maintenanceUiActive()' in loop
assert 'device_maintenance.active()||view_sender.maintenanceUiActive()' in loop
assert 'if (!device_maintenance.committed())view_sender.serviceDmxOutput();' in loop
assert function(app,'void controllerPowerOff(void) {')==function(baseline('src/src.cpp'),'void controllerPowerOff(void) {')
assert function(view,'    void sendDmxZeroBeforePowerAction(')==function(baseline('src/view_sender.h'),'    void sendDmxZeroBeforePowerAction(')
assert function(view,'    void performOriginalPowerOff(')=="    void performOriginalPowerOff() {\n        sendDmxZeroBeforePowerAction();\n        controllerPowerOff();\n    }"
assert 'view_sender.performOriginalPowerOff();' in app
assert 'performOriginalPowerOff();' in function(view,'    void handleSettingsPowerOffTap(')
assert function(view,'    void handleSettingsPowerOffTap(').replace('performOriginalPowerOff();','sendDmxZeroBeforePowerAction();\n            controllerPowerOff();')==function(baseline('src/view_sender.h'),'    void handleSettingsPowerOffTap(')
assert app.count('M5.Power.powerOff();')==1
backend=app.split('class ApplicationMaintenanceBackend',1)[1].split('void setup',1)[0]
assert not any(s in backend for s in ['isVBUS','getBatState','getType','DMX_HARDWARE','powerCapability'])
assert 'controllerCanPowerOff' not in app+view and 'UNAVAILABLE' not in function(view,'    void handleSettingsPowerOffTap(')
# Exact accepted startup power and battery monitoring, with no charging/power-path reconfiguration.
assert app.split('void setup(void) {',1)[1].split('    // Reserve the EXISTING',1)[0].split('    network_service::prepare()',1)[1]==baseline('src/src.cpp').split('void setup(void) {',1)[1].split('    // Reserve the EXISTING',1)[0].split('    network_service::prepare()',1)[1]
assert function(view,'    void updateBatteryMonitor(')==function(baseline('src/view_sender.h'),'    void updateBatteryMonitor(')
restore=function(view,'    bool restoreMaintenancePage(')
assert restore.index('M5.Speaker.isPlaying()')<restore.index('setActiveFullscreenPage(')
assert 'M5.Display.fillScreen(TFT_BLACK)' in restore and 'drawPresetOpenButton();' in restore
assert 'if (!touch_down) maintenance_ui_open=false' in restore
assert 'M5.Display.setClipRect(120,78,80,60)' in view and 'M5.Display.waitDisplay();M5.Display.clearClipRect();' in view
assert 'maintenance_->cancel(r.token.generation,r.maintenance_request,lock_.nowMs())' in bridge
assert 'maintenance_->committed()&&(!r.settings||r.settings_write)' in bridge
for n in ['src/dmx_controller.cpp','src/dmx_controller.h','src/command_dispatcher.cpp','src/command_dispatcher.h','src/dmx_transport.cpp','src/dmx_transport.h','src/dmx_transport_esp_dmx.h','src/preset_store.h','src/network_service.cpp','platformio.ini','partitions/cores3se_16mb_ota.csv']:
    assert read(n)==baseline(n),'Protected behavior changed: '+n
for n in ['include/device_maintenance.h','include/maintenance_transport.h']:
    text=read(n)
    assert not any(s in text for s in ['delay(', 'while (', 'Preferences', 'putUChar', 'dmx_write', 'dmx_tx_cancel','esp_restart'])
print('PASS exact original Power Off/helper and shared calls; no new power gate/config; unchanged startup, battery/DMX/NVS; five-second owner/render/clip/restore boundaries')
