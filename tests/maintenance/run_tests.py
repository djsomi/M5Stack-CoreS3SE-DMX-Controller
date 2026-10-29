"""Compile real maintenance authority/bridge and extracted original power path; no device access."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'dist/stage6b5-maintenance';OUT.mkdir(exist_ok=True)
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'dist/host-toolchain/cache')
os.environ['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'dist/host-toolchain/local-cache')
compiler=str(ROOT/'dist/host-toolchain/ziglang/zig.exe')
source=(ROOT/'src/src.cpp').read_text(encoding='utf-8')
def function(text,signature):
    start=text.index(signature);opening=text.index('{',start);depth=0
    for i in range(opening,len(text)):
        if text[i]=='{':depth+=1
        if text[i]=='}':
            depth-=1
            if not depth:return text[start:i+1]+'\n'
view=(ROOT/'src/view_sender.h').read_text(encoding='utf-8')
(OUT/'render_methods.h').write_text(function(view,'    bool pointInRect(')+function(view,'    bool serviceMaintenanceUi(')+function(view,'    bool restoreMaintenancePage('),encoding='utf-8')
(OUT/'power_original.h').write_text(function(source,'void controllerPowerOff(void) {')+
    function(view,'    void sendDmxZeroBeforePowerAction(')+function(view,'    void performOriginalPowerOff('),encoding='utf-8')
cases={
    'render':['tests/maintenance/test_render.cpp'],
    'maintenance':['src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp','tests/maintenance/test_maintenance.cpp'],
    'power-unit':['-DDMX_HARDWARE_UNIT','tests/maintenance/test_power.cpp'],
    'power-base':['-DDMX_HARDWARE_BASE','tests/maintenance/test_power.cpp']}
for name,files in cases.items():
    exe=OUT/(name+'.exe')
    subprocess.run([compiler,'c++','-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude','-I'+str(OUT),*files,'-o',str(exe)],cwd=ROOT,check=True)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
subprocess.run([sys.executable,'-B','tests/maintenance/check_integration.py'],cwd=ROOT,check=True)
