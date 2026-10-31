"""Fault-injected THDIFF state machine + real USB adapter/service; no hardware."""
from pathlib import Path
import argparse, os, subprocess, sys
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'dist/thdiff-aba-20261030/host')
out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
sys.path.insert(0,str(ROOT/'scripts'));from touch_acquisition_trace import function
source=(ROOT/'.pio/libdeps/cores3se_unit/M5GFX/src/lgfx/v1/platforms/esp32/common.cpp').read_text(encoding='utf-8')
signatures=['    cpp::result<void, error_t> transactionWrite(', '    cpp::result<void, error_t> transactionWriteRead(', '    cpp::result<uint8_t, error_t> readRegister8(', '    cpp::result<void, error_t> writeRegister8(']
(out/'io_under_test.h').write_text('\n'.join(function(source,s) for s in signatures),encoding='utf-8')
diag=(ROOT/'src/hardware_diagnostics.cpp').read_text(encoding='utf-8');(out/'service_under_test.h').write_text(function(diag,'void service()'),encoding='utf-8')
compiler=ROOT/'dist/host-toolchain/ziglang/zig.exe'
common=['-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Itests/thdiff/fake','-Itests/touch_config/fake','-I'+str(out)]
for name,flags in [('investigation',['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','-DDMX_TOUCH_CONFIG_DIAGNOSTICS=1','-DDMX_THDIFF_EXPERIMENT=1','-DDMX_THDIFF_AUTHORIZED_INVESTIGATION=1']),('unit',[]),('base',['-DDMX_HARDWARE_BASE']),('standard-diagnostics',['-DDMX_HW_DIAGNOSTICS=1']),('ordinary-touch-diagnostics',['-DDMX_HW_DIAGNOSTICS=1','-DDMX_TOUCH_CONFIG_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1']),('opt-out',['-DDMX_HW_DIAGNOSTICS=1','-DDMX_THDIFF_EXPERIMENT=0'])]:
 exe=out/(name+'.exe')
 sources=['src/thdiff_experiment.cpp','tests/thdiff/test_experiment.cpp']
 # A disabled experiment must link without any I2C backend even when the
 # independent read-only snapshot opt-in is set; snapshot tests cover that module.
 if name=='investigation':sources.append('src/touch_configuration_snapshot.cpp')
 subprocess.run([str(compiler),'c++',*common,*flags,*sources,'-o',str(exe)],cwd=ROOT,check=True)
 for mode in (['ready','recovery','capture','touch','disconnect','expiry','exit','overflow','partial','badbyte','txerror','txzero','rxerror','rxunavailable','restore_fail'] if name=='investigation' else ['off']):subprocess.run([str(exe),mode],cwd=ROOT,check=True)
app=(ROOT/'src/src.cpp').read_text(encoding='utf-8')
assert app.count('thdiff::beforeExit();')==2
assert 'thdiff::beforeExit();' in function(app,'void controllerPowerOff(')
assert 'thdiff::beforeExit();' in function(app,'    void execute(DeviceMaintenance::Action action)')
assert diag.index('thdiff::poll();')<diag.index('if (physical_fader::touchActive()) return;')
assert diag.index('line_sent += written;')<diag.index('thdiff::lineSent();')
print('PASS managed-exit hooks and safety poll before touch/TX early returns; original DMX shutdown retained by exact historical guards')
