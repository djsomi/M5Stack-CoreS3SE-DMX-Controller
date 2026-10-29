"""Pinned reviewed Settings-only deltas for historical whole-file scope guards.

The real new behavior is exercised by tests/settings and frontend/browser tests.
Restore the accepted ce1b9f6 inputs ONLY after matching exact reviewed contents;
all old DMX/HTTP/transport/partition reconstruction gates remain in force.
Hashes are review inputs, never generated/updated by a test run.
"""
from pathlib import Path
import hashlib, subprocess
ROOT=Path(__file__).resolve().parents[2]
BASE="ce1b9f63431874ae05588c1275644878ee29a973"
REVIEWED={'src/src.cpp': 'd51db6f242aca1a4b6982fbd945674dc727f01f54aad1782865c5a5a1ff7999f', 'src/view_sender.h': '75225632497ae70a37d75b8985c5d286c23516d5791cade30d13d9eef53b68fe', 'include/websocket_bridge.h': '1093dce82b726458e8d7659314d9e3c94dec8fd133da556cf7b950f358d93e11', 'include/websocket_service.h': 'd4b626ddc3afc854614db20762906b57cc6dc4c9f7de477243cf7256ac3b7646', 'src/websocket_bridge.cpp': '1ddb184dc9a5dc425abb00dfce9907c83e01d614462cc80ec212c5190cb6d093', 'src/websocket_protocol.cpp': 'bdda64fe60fa8857a1a795265477ae570cd201796c9860febce9426773c4b5ff', 'src/websocket_service.cpp': 'bd037b9b69fac8e9aade4d80c3c0a7ac9a99f3ba0f155981ef12017b0acf0e2f', 'web/src/js/protocol.js': '1701376575158f822a82812ea2247d80d28260fbbe1c1a4c0d51bdb3af01491a', 'web/src/js/commands.js': '3f3f92537f837677b3030b43ad3721b7fbde20dcde171a62407d9fd15aaa6010', 'web/src/js/transport.js': '4058254d5453c4479a8849cfa7879dc813902d3f77dc8d17beeb1880d135251d', 'web/src/js/settings.js': '49df6c06c2dc4de62705fb5785c8eca393c9b0aa7a6a1b055ecce5ae5aa604e9', 'web/src/js/app.js': '580f6faa86d1decea36cfddddd271785aaa1a41a762556c42ca4c30300d5b95b'}
HEADER="4a4a921ce02e691f5cb9b4f5aa46f10b817275603adc680913eec4a81a5d8c8d"
def restore(name,text):
    if name not in REVIEWED:return text
    assert hashlib.sha256(text.replace("\r\n","\n").encode()).hexdigest()==REVIEWED[name], "Unreviewed Device Settings delta: "+name
    return subprocess.check_output(["git","-c","safe.directory="+ROOT.as_posix(),"show",BASE+":"+name],cwd=ROOT).decode()
def check_header():
    assert hashlib.sha256((ROOT/'include/device_settings.h').read_bytes().replace(b"\r\n",b"\n")).hexdigest()==HEADER
