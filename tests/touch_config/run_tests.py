"""Read-only snapshot: actual module and installed I2C helper bodies, no device."""
from pathlib import Path
import argparse, json, os, re, subprocess, sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT/'dist/touch-config-snapshot-20261030/host')
args = parser.parse_args()
out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR', str(ROOT/'dist/host-toolchain/local-cache'))
compiler = ROOT/'dist/host-toolchain/ziglang/zig.exe'
sys.path.insert(0, str(ROOT/'scripts'))
from touch_acquisition_trace import function
vendor = ROOT/'.pio/libdeps/cores3se_unit/M5GFX'
assert json.loads((vendor/'library.json').read_text())['version'] == '0.2.28'
source = (vendor/'src/lgfx/v1/platforms/esp32/common.cpp').read_text(encoding='utf-8')
helpers = function(source, '    cpp::result<void, error_t> transactionWriteRead(')
helpers += '\n' + function(source, '    cpp::result<uint8_t, error_t> readRegister8(')
(out/'read_under_test.h').write_text(helpers, encoding='utf-8')
common = ['-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Itests/touch_config/fake','-I'+str(out)]
for name, flags in [('investigation',['-DDMX_HW_DIAGNOSTICS=1','-DDMX_TOUCH_CONFIG_DIAGNOSTICS=1']),('unit',[]),('base',['-DDMX_HARDWARE_BASE']),('standard-diagnostics',['-DDMX_HW_DIAGNOSTICS=1']),('opt-out',['-DDMX_HW_DIAGNOSTICS=1','-DDMX_TOUCH_CONFIG_DIAGNOSTICS=0'])]:
    exe=out/(name+'.exe')
    subprocess.run([str(compiler),'c++',*common,*flags,'src/touch_configuration_snapshot.cpp','tests/touch_config/test_snapshot.cpp','-o',str(exe)],cwd=ROOT,check=True)
    modes=['ready','board','touch','panel','driver','port','sda','scl','address','spi','frequency','identity','failure','stop_failure'] if name=='investigation' else ['off']
    for mode in modes:subprocess.run([str(exe),mode],cwd=ROOT,check=True)

app = (ROOT/'src/src.cpp').read_text(encoding='utf-8')
assert app.index('M5.begin(cfg);') < app.index('hardware_diagnostics::begin();') < app.index('network_service::launch(')
assert app.index('hardware_diagnostics::begin();') < app.index('void loop(')
diag = (ROOT/'src/hardware_diagnostics.cpp').read_text(encoding='utf-8')
assert diag.count('touch_configuration::captureStartup();') == 1
assert 'touch_configuration::captureStartup();' in function(diag,'void begin()')
assert 'touch_configuration::captureStartup();' not in function(diag,'void service()')
assert 'physical_fader::touchActive()' in function(diag,'void service()')
module = (ROOT/'src/touch_configuration_snapshot.cpp').read_text(encoding='utf-8')
assert re.findall(r'\{0x([0-9A-F]{2}), "[A-Z_]+"\}',module) == ['A3','A6','A8','80','85','86','87','88','89','A5','A1','A2']
assert 'lgfx::i2c::readRegister8(c.port, c.address, reg, c.frequency)' in module
for forbidden in ['writeRegister','initTouch(','getTouchRaw(','Touch.update(','setPins(','Serial.','new ','malloc(','NVS','Preferences','delay(']:
    assert forbidden not in module, forbidden
print('PASS exact installed read helper/STOP propagation, startup-only integration, fixed documented register allowlist and no write/reset/polling/output API')
