"""No hardware access. Compile authority/transaction/real Bridge and actual UI methods."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'dist/stage6b5c/host';OUT.mkdir(parents=True,exist_ok=True)
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'dist/host-toolchain/cache')
os.environ['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'dist/host-toolchain/local-cache')
compiler=ROOT/'dist/host-toolchain/ziglang/zig.exe'
sources=['src/softap_policy.cpp','src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp','tests/wifi/test_configuration.cpp']
subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude',*sources,'-o',str(OUT/'configuration.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'configuration.exe')],cwd=ROOT,check=True)

view=(ROOT/'src/view_sender.h').read_text(encoding='utf-8')
def function(text,signature):
    a=text.index(signature);b=text.index('{',a);depth=0
    for i in range(b,len(text)):
        if text[i]=='{':depth+=1
        if text[i]=='}':
            depth-=1
            if not depth:return text[a:i+1]+'\n'
(OUT/'wifi_render_methods.h').write_text(''.join(function(view,s) for s in ['    bool pointInRect(','    bool serviceWifiUi(','    bool restoreWifiPage(']),encoding='utf-8')
subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude','-I'+str(OUT),'tests/wifi/test_render.cpp','-o',str(OUT/'render.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'render.exe')],cwd=ROOT,check=True)

network=(ROOT/'src/network_service.cpp').read_text(encoding='utf-8')
(OUT/'radio_under_test.h').write_text(function(network,'bool wifiApplyAP('),encoding='utf-8')
subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude','-I'+str(OUT),'tests/wifi/test_radio.cpp','-o',str(OUT/'radio.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'radio.exe')],cwd=ROOT,check=True)

subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Itests/wifi/fake','-Isrc','-Iinclude','tests/wifi/test_store.cpp','-o',str(OUT/'store.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'store.exe')],cwd=ROOT,check=True)
(OUT/'callbacks_under_test.h').write_text(function(network,'void wifiRetire(')+'\n'+function(network,'void wifiProbe('),encoding='utf-8')
subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(OUT),'tests/wifi/test_callbacks.cpp','-o',str(OUT/'callbacks.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'callbacks.exe')],cwd=ROOT,check=True)
subprocess.run([sys.executable,'-B','tests/wifi/check_integration.py'],cwd=ROOT,check=True)

subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude',*sources[:-1],'tests/wifi/test_settings_maintenance.cpp','-o',str(OUT/'settings-maintenance.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'settings-maintenance.exe')],cwd=ROOT,check=True)
rows=view[view.index('        // Stage 5 local network information begin'):view.index('        // Stage 5 local network information end.')]
(OUT/'wifi_settings_rows.h').write_text(rows,encoding='utf-8')
(OUT/'wifi_settings_reset.h').write_text(function(view,'    bool handleWifiResetTouch('),encoding='utf-8')
subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude','-I'+str(OUT),'tests/wifi/test_settings_render.cpp','-o',str(OUT/'settings-render.exe')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'settings-render.exe')],cwd=ROOT,check=True)
