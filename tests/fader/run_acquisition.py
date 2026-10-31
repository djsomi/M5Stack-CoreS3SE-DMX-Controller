"""Differential test of real vendor function bodies; no physical I2C or device."""
from pathlib import Path
import argparse,os,subprocess,sys,re,json
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
import touch_acquisition_trace as t
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'dist/touch-acquisition-20261030/acquisition-host')
out=p.parse_args().output;out.mkdir(parents=True,exist_ok=True)
vendor=ROOT/'.pio/libdeps/cores3se_unit';touch=vendor/'M5Unified/src/utility'
files={name:next((vendor/lib).rglob(name)) for name,(lib,_,_) in t.LOCKS.items()}
original={n:p.read_text(encoding='utf-8') for n,p in files.items()}
for name,text in original.items():
    assert t.instrument(name,text)==t.instrument(name,text.replace('\n','\r\n'))
    try:t.instrument(name,text+'\n')
    except ValueError:pass
    else:raise AssertionError('mutated source accepted')
class Normal:
    def subst(self,_):return 'cores3se_unit'
    def get(self,*args):return []
    def ParseFlags(self,_):return {}
    def AddBuildMiddleware(self,*args):raise AssertionError('production middleware must not be installed')
t.install(Normal(),ROOT)
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
compiler=ROOT/'dist/host-toolchain/ziglang/zig.exe'
signatures=[]
for mode in ['original','trace','opt-out']:
    d=out/mode;d.mkdir(exist_ok=True)
    texts=original if mode=='original' else {n:t.instrument(n,v) for n,v in original.items()}
    ft=t.function(texts['Touch_FT5x06.cpp'],'  size_t Touch_FT5x06::_read_data(')+'\n'+t.function(texts['Touch_FT5x06.cpp'],'  uint_fast8_t Touch_FT5x06::getTouchRaw(')
    start=texts['M5GFX.cpp'].index('  struct Touch_M5StackCoreS3 :')
    core=t.function(texts['M5GFX.cpp'][start:],'    uint_fast8_t getTouchRaw(')
    (d/'acquisition_ft_under_test.h').write_text(ft,encoding='utf-8');(d/'acquisition_core_under_test.h').write_text(core,encoding='utf-8')
    tc=texts['Touch_Class.cpp'].replace('#include "Touch_Class.hpp"','#include '+json.dumps((touch/'Touch_Class.hpp').as_posix()))
    (d/'Touch_Class.cpp').write_text(tc,encoding='utf-8')
    flags=['-DDMX_HW_DIAGNOSTICS=1','-DDMX_FADER_DIAGNOSTICS=1']
    flags+=['-DDMX_TOUCH_ACQUISITION_DIAGNOSTICS='+('1' if mode=='trace' else '0')]
    exe=d/'test.exe'
    subprocess.run([str(compiler),'c++','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-Iinclude','-Isrc','-Itests/fader/fake','-I'+str(touch),'-I'+str(d),'tests/fader/test_acquisition.cpp',str(d/'Touch_Class.cpp'),'src/touch_acquisition_diagnostics.cpp','-o',str(exe)],cwd=ROOT,check=True)
    result=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True);print(mode,result.stdout,result.stderr,flush=True)
    assert result.returncode==0,result.returncode
    signatures.append(re.search(r'SIGNATURE .*',result.stdout)[0])
assert len(set(signatures))==1,signatures
print('PASS identical original/trace/opt-out I2C+GPIO transaction transcript and outputs; repeat generation, LF/CRLF, source mutation rejection and production exclusion')
