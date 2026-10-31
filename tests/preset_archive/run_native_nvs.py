"""Real pinned ESP-IDF NVS on a bounded NOR-flash host adapter; no hardware.

Supply the unmodified v4.4.7 SDK tree via --sdk-source. See native manifest.
--project-source permits the same test against the preserved pre-fix tree.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk-source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--project-source',type=Path,default=ROOT);args=p.parse_args()
out=args.output.resolve();assert (ROOT/'dist').resolve() in out.parents;out.mkdir(parents=True,exist_ok=True)
sdk=args.sdk_source.resolve();project=args.project_source.resolve();shim=ROOT/'tests/preset_archive/native'
manifest=json.loads((shim/'sdk-source-manifest.json').read_text())
for item in manifest['files']:
 data=(sdk/item['path']).read_bytes();oid=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest();assert oid==item['sha'],'Modified SDK input: '+item['path']
def function(text,signature):
 start=text.index(signature);opening=text.index('{',start);depth=0
 for i in range(opening,len(text)):
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if not depth:return text[start:i+1]+'\n'
 raise AssertionError(signature)
view=(project/'src/view_sender.h').read_text(encoding='utf-8')
(out/'physical_presets_under_test.h').write_text('\n'.join(function(view,s)for s in ['const char* presetError(','bool commandSucceeded(','const char* commandError(','bool clearPreset(','bool savePreset(']),encoding='utf-8')
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/cache'));os.environ.setdefault('ZIG_LOCAL_CACHE_DIR',str(ROOT/'dist/host-toolchain/local-cache'))
component=sdk/'components/nvs_flash';names=['nvs_types','nvs_api','nvs_page','nvs_pagemanager','nvs_storage','nvs_item_hash_list','nvs_handle_simple','nvs_partition_manager','nvs_partition','nvs_partition_lookup']
command=[str(ROOT/'dist/host-toolchain/ziglang/zig.exe'),'c++','-std=c++11','-DLINUX_TARGET','-include','cstring','-I'+str(shim),'-I'+str(component/'include'),'-I'+str(component/'private_include'),'-I'+str(component/'src'),'-I'+str(project/'src'),'-I'+str(project/'include'),'-I'+str(out)]
command += [str(component/'src'/(n+'.cpp'))for n in names]+[str(project/'src'/n)for n in ['dmx_controller.cpp','command_dispatcher.cpp','websocket_bridge.cpp','websocket_protocol.cpp']]+[str(ROOT/'tests/preset_archive/test_native_nvs.cpp'),'-o',str(out/'native-nvs.exe')]
subprocess.run(command,cwd=ROOT,check=True);subprocess.run([str(out/'native-nvs.exe')],cwd=ROOT,check=True)
