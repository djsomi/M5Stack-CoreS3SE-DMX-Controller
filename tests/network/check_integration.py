# Exact baseline reconstruction and Stage 5 ownership checks; no device access.
from pathlib import Path
import re
import subprocess
from canvas_guard_support import stage5a_view
from softap_guard_support import stage5b_view
root=Path(__file__).resolve().parents[2]
def base(name):
    return subprocess.check_output(['git','-c','safe.directory='+root.as_posix(),'show','f31411a:'+name],cwd=root).decode('utf-8')
app=(root/'src/src.cpp').read_text(encoding='utf-8')
view=(root/'src/view_sender.h').read_text(encoding='utf-8')
# Stage 5A adds nine capture-only setup calls; strip exactly these for baseline checks.
trace_steps=re.findall(r'    wifi_startup_trace::capture\("([^"]+)"[^\n]*\n',app)
assert trace_steps==['setup.enter','credentials.after','M5.begin.before','M5.begin.after','sender.cells.after','receiver.canvas.before','receiver.canvas.after','dmx.startup.before','dmx.startup.after'],trace_steps
normalized=re.sub(r'    wifi_startup_trace::capture\([^\n]+\n','',app)
for text in ['#include "network_service.h"\n',
'    network_service::prepare(); // Owner-only credential I/O, before ADC/radio/DMX.\n',
'    // Reserve the EXISTING UI buffers before any radio allocation. No page switch.\n    const bool sender_canvas_reserved = view_sender.reserveNetworkCanvas();\n    const bool receiver_canvas_reserved = view_receiver.reserveNetworkCanvas();\n    const bool network_canvas_reserved = sender_canvas_reserved && receiver_canvas_reserved;\n\n',
'    network_service::launch(network_canvas_reserved);\n',
'    network_service::ownerTick(esp_timer_get_time(), dmx_transport.health().errors,\n                               dmx_transport.health().fault_stopped);\n']:
    assert normalized.count(text)==1,text
    normalized=normalized.replace(text,'')
assert normalized==base('src/src.cpp'),'Unrelated application change'
normalized=stage5a_view(stage5b_view(view)).replace('#include "network_service.h"\n','')
normalized=re.sub(r'    bool reserveNetworkCanvas\(\) \{.*?\n    \}\n\n','',normalized,count=1,flags=re.S)
normalized=normalized.replace('    bool network_canvas_reserved = false;\n','')
normalized=normalized.replace('static constexpr int settings_content_h = 480;','static constexpr int settings_content_h = 300;')
normalized=normalized.replace('''        // Keep the existing UI buffers reserved across mode switches while Wi-Fi exists.
        if (!network_canvas_reserved) {
            for (auto& tmp : canvas) {
                tmp.deleteSprite();
            }
            fullscreen_canvas.deleteSprite();
        }''','''        for (auto& tmp : canvas) {
            tmp.deleteSprite();
        }
        fullscreen_canvas.deleteSprite();''')
block=re.search(r'        // Stage 5 local network information begin.*?        // Stage 5 local network information end.\n\n',normalized,re.S)
assert block and 'network_service::password()' in block.group() and 'network_service::status()' in block.group()
normalized=normalized.replace(block.group(),'')
assert normalized==base('src/view_sender.h'),'Unrelated UI change'
receiver=(root/'src/view_receiver.h').read_text(encoding='utf-8')
receiver=re.sub(r'    bool reserveNetworkCanvas\(\) \{.*?\n    \}\n','',receiver,count=1,flags=re.S)
receiver=receiver.replace('    bool network_canvas_reserved = false;\n','')
receiver=receiver.replace('        if (!network_canvas_reserved) canvas.deleteSprite();','        canvas.deleteSprite();')
assert receiver==base('src/view_receiver.h'),'Receiver behavior change'
for name in ['src/dmx_transport.cpp','src/dmx_transport.h','src/dmx_transport_esp_dmx.h','src/dmx_controller.cpp','src/command_dispatcher.cpp','src/preset_store.h','src/hardware_diagnostics.cpp','platformio.ini','include/dmx_hardware.h','patches/esp_dmx_v202/tx_core.h','patches/esp_dmx_v202/tx_service.h','scripts/esp_dmx_tx_patch.py','scripts/embed_web_assets.py','scripts/package_firmware.py','scripts/web_asset_pipeline.py','partitions/cores3se_16mb_ota.csv']:
    assert (root/name).read_text(encoding='utf-8')==base(name),name
net=(root/'src/network_service.cpp').read_text()
assert all(s not in net for s in ['M5.', 'DmxController', 'CommandDispatcher', 'DmxTransport', 'Serial.', 'HWCDC', 'WiFi.begin(', 'httpd_ws_'])
assert net.index('esp_wifi_set_config(WIFI_IF_AP,&ap)')<net.index('esp_wifi_start()')
assert 'WIFI_STORAGE_RAM' in net and 'WIFI_MODE_AP' in net and 'WIFI_AUTH_WPA2_PSK' in net and 'cfg.nvs_enable=0' in net
assert 'WIFI_MODE_STA' not in net and 'WIFI_MODE_APSTA' not in net
assert 'storage.begin("dmxwifi",false)' in net and 'storage.putBytes("ap1"' in net
assert net.index('storage.putBytes("ap1"')>net.index('void prepare()')
handler=net[net.index('esp_err_t serve('):net.index('void worker(')]
assert all(s not in handler for s in ['credentials','password','Preferences','storage.','M5.'])
assert 'httpd_sess_set_recv_override' in net and 'httpd_sess_set_send_override' in net
assert 'httpd_queue_work(server,sweep' in net and 'httpd_sess_trigger_close' in net
assert net.index('httpd_sess_update_lru_counter(server,s.fd)')<net.index('httpd_sess_trigger_close(server,s.fd)')
assert app.index('network_service::prepare()')<app.index('    M5.begin(cfg);')
assert app.index('view_sender.reserveNetworkCanvas()')<app.index('network_service::launch(')
assert app.index('view_receiver.reserveNetworkCanvas()')<app.index('network_service::launch(')
print('PASS exact Stage4B app/UI/Receiver reconstruction, protected driver/domain/USB/layout/assets, network ownership and startup/session guards')
