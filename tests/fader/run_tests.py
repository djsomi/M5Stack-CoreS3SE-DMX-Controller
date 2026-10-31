"""Compile actual recorder and installed M5 touch source against bounded host I/O."""
from pathlib import Path
import argparse,os,re,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/network'))
from fader_guard_support import prior
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler');p.add_argument('--zig',action='store_true')
p.add_argument('--output',type=Path,default=ROOT/'dist/fader-host')
p.add_argument('--legacy-view',type=Path,help='Optional exact pre-fix view: verify the regression fails on its real input slices')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
compiler=a.compiler or os.environ.get('CXX') or shutil.which('c++')
if not compiler:compiler=str(ROOT/'dist/host-toolchain/ziglang/zig.exe');a.zig=True
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
tool=[compiler]+(['c++'] if a.zig else [])
touch=ROOT/'.pio/libdeps/cores3se_unit/M5Unified/src/utility'
common=['-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Isrc','-Itests/fader/fake','-I'+str(touch),'-I'+str(a.output)]
view=(a.legacy_view or ROOT/'src/view_sender.h').read_text(encoding='utf-8')
def function(signature):
    first=view.index(signature);brace=view.index('{',first);depth=0
    for i in range(brace,len(view)):
        depth+=(view[i]=='{')-(view[i]=='}')
        if not depth:return view[first:i+1]+'\n'
    raise AssertionError(signature)
geometry=view[view.index('    static constexpr uint16_t scroll_width'):view.index('    static constexpr int precision_back_x')]
first=view.index('        if (main_touch_consume_release) {' if a.legacy_view else '        int main_fader_y=tp.y;')
pre=view[first:view.index('        if (tp.state) {',first)]
first=view.index('                    int new_value = target_value;')
body=view[first:view.index('\n                }\n            }\n        }\n',first)]
generated=geometry+function('    bool applyMainCommand(')
generated+='bool frame() {\nif(other_page)return true;bool modified=false;auto tp=M5.Touch.getDetail();\n'
generated+=pre+'\nif(tp.state && tp.base_x>=scroll_width && tp.base_y<240) {\n'+body+'\n}\n(void)modified;return true;\n}\n'
(a.output/'captured_main_under_test.h').write_text(generated,encoding='utf-8')
cases={
 'recorder':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','tests/fader/test_recorder.cpp'],
 'runtime':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','tests/fader/test_runtime.cpp','src/physical_fader_diagnostics.cpp',str(touch/'Touch_Class.cpp')],
 'disabled-unit':['-DDMX_HARDWARE_UNIT','tests/fader/test_disabled.cpp','src/physical_fader_diagnostics.cpp'],
 'disabled-base':['-DDMX_HARDWARE_BASE','tests/fader/test_disabled.cpp','src/physical_fader_diagnostics.cpp'],
 'standard-diagnostics':['-DDMX_HW_DIAGNOSTICS=1','tests/fader/test_disabled.cpp','src/physical_fader_diagnostics.cpp'],
 'opt-out':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=0','tests/fader/test_disabled.cpp','src/physical_fader_diagnostics.cpp'],
 'captured-main':['tests/fader/test_captured.cpp','src/dmx_controller.cpp','src/command_dispatcher.cpp',str(touch/'Touch_Class.cpp')],
 'captured-main-diagnostics':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','tests/fader/test_captured.cpp','src/dmx_controller.cpp','src/command_dispatcher.cpp','src/physical_fader_diagnostics.cpp',str(touch/'Touch_Class.cpp')],
 'history':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','tests/fader/test_history.cpp'],
 'states':['tests/fader/test_states.cpp',str(touch/'Touch_Class.cpp')],
 'diagnostic-owner':['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1','tests/fader/test_owner.cpp','src/physical_fader_diagnostics.cpp',str(touch/'Touch_Class.cpp')]}
if a.legacy_view:cases={'captured-main-before':['-DFADER_REGRESSION_BEFORE',*cases['captured-main']]}
for flags in cases.values():
    if 'src/physical_fader_diagnostics.cpp' in flags:flags.append('src/touch_acquisition_diagnostics.cpp')
for name,flags in cases.items():
    exe=a.output/(name+('.exe' if os.name=='nt' else ''))
    subprocess.run(tool+common+flags+['-o',str(exe)],cwd=ROOT,check=True)
    if a.legacy_view:
        result=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True)
        assert result.returncode==1 and 'm.target_value==121+2*dy' in result.stderr,result
        print('PASS legacy input fails the first 1-pixel response regression as expected:',result.stderr.strip())
        sys.exit(0)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
    print('PASS '+name,flush=True)
for name in ('src/src.cpp','src/view_sender.h'):
    prior(name,(ROOT/name).read_text(encoding='utf-8'))
app=(ROOT/'src/src.cpp').read_text(encoding='utf-8')
version=re.search(r'DMX_FIRMWARE_VERSION "([^"]+)"',(ROOT/'include/firmware_version.h').read_text())[1]
for profile in ('UNIT DMX','BASE DMX'):
    text=profile+' - V'+version
    assert '"'+profile+' - V" DMX_FIRMWARE_VERSION, 160, 78' in app
    assert len(text)*6<=300 # Existing fixed-width Font0, size 1; no geometry change.
    print('PASS boot caption:',text,'Font0 width',len(text)*6)
# Formatter capacity bound: deliberately pessimistic 32 chars for each %s and
# 10 digits for every integer, including fields with smaller declared widths.
source=(ROOT/'src/physical_fader_diagnostics.cpp').read_text()
for fmt in re.findall(r'snprintf\(line,sizeof\(line\),"([^"\n]+)"',source):
    worst=re.sub(r'%s','x'*32,fmt)
    worst=re.sub(r'%llu','9'*20,worst)
    worst=re.sub(r'%(?:lu|u|d)','9'*10,worst).replace('%c','T').replace('\\n','\n')
    assert len(worst)<512,('diagnostic line overflow',len(worst))
assert source.count('Serial.write(')==1 and 'Serial.begin' not in source
assert 'getTouchRaw(' not in source and 'Touch.update(' not in source
assert 'new ' not in source and 'malloc(' not in source
print('PASS exact source inverse, boot geometry, bounded formatter, no new USB init/sensor reads/heap; no physical timing or electrical claims')
