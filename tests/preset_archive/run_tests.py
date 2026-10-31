from pathlib import Path
import argparse,os,subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);out=p.parse_args().output.resolve()
assert (ROOT/'dist').resolve() in out.parents;out.mkdir(parents=True,exist_ok=True)
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
cc=[str(ROOT/'dist/host-toolchain/ziglang/zig.exe'),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Isrc']
sources=['tests/preset_archive/test_core.cpp','src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp','src/websocket_protocol.cpp']
for name in ['core','bridge','adapter','snapshot']:
 sources[0]='tests/preset_archive/test_'+name+'.cpp'
 exe=out/(name+'.exe');subprocess.run(cc+['-DEXPECT_BULK_SNAPSHOT','-Itests/preset_archive/fake']+sources+['-o',str(exe)],cwd=ROOT,check=True);subprocess.run([str(exe)],cwd=ROOT,check=True)
