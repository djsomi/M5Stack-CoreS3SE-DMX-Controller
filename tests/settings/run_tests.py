"""Compile real Settings/Bridge and extracted hardware adapter against host fakes."""
from pathlib import Path
import argparse, os, shutil, subprocess, sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'dist/device-settings-20261029'
subprocess.run([sys.executable,'-B',str(ROOT/'tests/settings/check_adapter.py')],cwd=ROOT,check=True)
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler');parser.add_argument('--zig',action='store_true')
args=parser.parse_args()
compiler=args.compiler or os.environ.get('CXX') or shutil.which('c++')
if not compiler:
    compiler=str(ROOT/'dist/host-toolchain/ziglang/zig.exe');args.zig=True
command=[str(compiler)]+(['c++'] if args.zig else [])
cases={'settings':['src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp','tests/settings/test_settings.cpp'],
       'settings-preview':['src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp','tests/settings/test_preview.cpp'],
       'settings-adapter':['tests/settings/test_adapter.cpp']}
for name,sources in cases.items():
    exe=OUT/(name+'.exe')
    subprocess.run(command+['-std=c++11','-Wall','-Wextra','-Werror','-Isrc','-Iinclude','-I'+str(OUT),*sources,'-o',str(exe)],cwd=ROOT,check=True)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
