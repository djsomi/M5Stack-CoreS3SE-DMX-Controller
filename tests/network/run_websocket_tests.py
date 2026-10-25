"""Compile/run Stage6A host tests; writes only ignored dist/stage6a-host-tests.

Example: python -B tests/network/run_websocket_tests.py --compiler path/to/zig.exe --zig
Otherwise uses CXX/c++, or the existing ignored local Zig toolchain on Windows.
"""
from pathlib import Path
import argparse,os,shutil,subprocess
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler');p.add_argument('--zig',action='store_true')
a=p.parse_args()
out=root/'dist/stage6a-host-tests';out.mkdir(parents=True,exist_ok=True)
compiler=a.compiler or os.environ.get('CXX') or shutil.which('c++')
if not compiler:
    compiler=str(root/'dist/host-toolchain/ziglang/zig.exe');a.zig=True
if a.zig:
    os.environ['ZIG_GLOBAL_CACHE_DIR']=str(root/'dist/host-toolchain/cache')
    os.environ['ZIG_LOCAL_CACHE_DIR']=str(out/'zig-cache')
command=[compiler]+(['c++'] if a.zig else [])
subprocess.run([os.sys.executable,'-B','tests/network/check_websocket_integration.py',
                '--adapter-header',str(out/'ws_adapter_under_test.h')],cwd=root,check=True)
flags=['-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Isrc','-I'+str(out)]
if os.name!='nt':flags+=['-pthread']
cases={
    'bridge':['src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp','src/websocket_diagnostics.cpp','tests/network/test_websocket.cpp'],
    'adapter':['src/websocket_protocol.cpp','tests/network/test_websocket_adapter.cpp']}
for name,sources in cases.items():
    exe=out/(name+('.exe' if os.name=='nt' else ''))
    subprocess.run(command+flags+sources+['-o',str(exe)],cwd=root,check=True)
    subprocess.run([str(exe)],cwd=root,check=True)
