"""Current-source scope/ownership checks, separate from historical reconstruction."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
BASE='3730ea421a5a10d6f2be4451df6328e18c1e3aa1'
def read(name):return (ROOT/name).read_text(encoding='utf-8')
def old(name):return subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),'show',BASE+':'+name],cwd=ROOT).decode('utf-8')
def function(text,signature):
    a=text.index(signature);b=text.index('{',a);depth=0
    for i in range(b,len(text)):
        if text[i]=='{':depth+=1
        if text[i]=='}':
            depth-=1
            if not depth:return text[a:i+1]
net=read('src/network_service.cpp');app=read('src/src.cpp');view=read('src/view_sender.h')
for path in ['src/dmx_controller.cpp','src/dmx_controller.h','src/command_dispatcher.cpp','src/command_dispatcher.h',
             'src/dmx_transport.cpp','src/dmx_transport.h','src/dmx_transport_esp_dmx.h','src/preset_store.h',
             'src/view_receiver.h','include/device_maintenance.h','include/maintenance_transport.h',
             'src/http_policy.h','src/http_policy.cpp','platformio.ini','partitions/cores3se_16mb_ota.csv',
             'scripts/web_asset_pipeline.py','scripts/web_ui_bundle.py','scripts/package_firmware.py']:
    assert read(path)==old(path),'Unrelated implementation change: '+path
settings=read('include/device_settings.h')
polish='    bool persistencePending() const {return pending_;}\n    // Owner-task maintenance admission cancels only ephemeral remote state; never writes NVS.\n    void cancelPreviewForMaintenance() {cancelPreview();}\n'
assert settings.replace(polish,'')==old('include/device_settings.h')
assert 'device_settings.persistencePending()||command_dispatcher.pending()' in app
for signature in ['void controllerPowerOff(void)','void controllerRestart(void)']:
    assert function(app,signature)==function(old('src/src.cpp'),signature)
for signature in ['    void performOriginalPowerOff(','    void sendDmxZeroBeforePowerAction(',
                  '    void handleSettingsPowerOffTap(','    void handleSettingsRestartTap(',
                  '    bool serviceMaintenanceUi(','    bool restoreMaintenancePage(','    void updateBatteryMonitor(']:
    assert function(view,signature)==function(old('src/view_sender.h'),signature),'Changed accepted function: '+signature
for signature in ['int io(','esp_err_t serve(','void stationEvent(']:
    assert function(net,signature)==function(old('src/network_service.cpp'),signature)
worker=function(net,'void worker(')
assert worker.replace('            if(!wifiChange(netif)){error=ESP_FAIL;break;}\n','')==function(old('src/network_service.cpp'),'void worker(')
prepare=function(net,'void prepare(')
assert prepare.index('wifi_credentials::recover')<prepare.index('const bool exists=storage.isKey("ap1")')
assert 'reset?wifi_credentials::eraseVerified(journal,"ap_try")' in prepare
assert view.count('M5Canvas fullscreen_canvas;')==1
popup=function(view,'    bool serviceWifiUi(')
assert 'config.approve(millis())' in popup and 'config.cancelPhysical()' in popup
assert 'tp.wasPressed()' in popup and 'wifi_released' in popup
assert '20,174,130,50' in popup and '170,174,130,50' in popup
assert 'state.password_change' in popup and '.pass' not in popup.replace('.password_change','')
assert 'delay(' not in popup and 'while(' not in popup
authority=read('include/wifi_configuration.h')
assert 'backend_.begin(' not in function(authority,'    Result propose(')
assert 'backend_.begin(candidate_)' in function(authority,'    bool approve(')
assert 'backend_.connected(owner_)' in function(authority,'    bool approve(')
assert 'elapsed>=60000' in authority and 'Origin::custom' in authority
assert 'WifiConfiguration::erase' in function(net,'esp_err_t websocket(')
assert 'wifi_job.epoch==epoch&&wifi_job.retire_requested' in function(net,'void wifiRetire(')
assert 'httpd_sess_trigger_close' not in function(net,'bool wifiChange(') # HTTP task owns sessions.
for path in ['include/wifi_configuration.h','include/wifi_credential_transaction.h','src/wifi_credential_store.h']:
    text=read(path)
    assert not any(word in text for word in ['dmxpreset','dmxmeta','dmxui','nvs_flash_erase','esp_restart','M5.','Serial.','printf('])
store=read('src/wifi_credential_store.h')
assert 'nvs_open("dmxwifi"' in store and 'nvs_commit' in store
wire=read('src/websocket_protocol.cpp')
encoder=function(wire,'size_t encode(').split('if(e.kind==Kind::wifi_snapshot',1)[1].split('if(e.kind==Kind::maintenance_snapshot',1)[0]
assert 'c.pass' not in encoder.replace('c.password_change','') and '\\"password\\":' not in encoder
print('PASS current Wi-Fi approval/worker boundaries, unchanged startup/HTTP/DMX/power/maintenance/NVS domains, single canvas, stale-work epoch and sanitized publication')

assert 'settings_wifi_reset_y = 394' in view and 'settings_content_h = 462' in view
settings_draw=function(view,'    void drawSettingsScreen(')
assert settings_draw.index('c.fillScreen(TFT_BLACK)')<settings_draw.index('char network_line[96]')
assert settings_draw.index('char network_line[96]')<settings_draw.index('pushFullscreenCanvas(')
assert 'WIFI ONLY' not in settings_draw and '/status FOR MEMORY HISTORY' not in settings_draw
assert 'settings_wifi_reset_y && content_y < settings_wifi_reset_y + 28' in view
