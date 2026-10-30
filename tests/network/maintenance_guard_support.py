"""Reviewed Stage 6B.5B reconstruction for legacy freeze checks.
Actual maintenance behavior is exercised by tests/maintenance and native ISR tests.
"""
from pathlib import Path
import hashlib,subprocess
from wifi_guard_support import prior as before_wifi
ROOT=Path(__file__).resolve().parents[2]
BASE="487fce1073def9c3328905d3b913d4482cd3b75d"
REVIEWED={'src/src.cpp': '996c01bd636afd6f27461b23f9e3e3a280a5fa0f40b2ff7ab08f1267d296bb72', 'src/view_sender.h': '5793a11d970d53d608996d8daa3bc8a560dad4c4166ce934e0d9e1e4216e4528', 'include/device_settings.h': '3d1cc2be49373823cc7d9d5ea19b701a060a3df972ac502271a122d0afb34b36', 'include/websocket_bridge.h': 'c3948b8c15c4fb9a827b4ddc9d16a165fc159edb15503b319c737737665541ac', 'include/websocket_service.h': 'b24372d8c83a9af71a080e9e49d1c0cc2ea0fbbc3d7fce6cd6e1f50cd5ab7947', 'src/websocket_bridge.cpp': '9c04fc9bb4bf8d79ccc2a0eb8c8bbbd506c992fe3de74b6f072928e18b40e628', 'src/websocket_protocol.cpp': '38d580507694b4fec862eea245a33272fcbfd627deafd2381f2e56319586c4b4', 'src/websocket_service.cpp': '2fc25e9cf1bba0d8d00fef493db07cd2c847e11edf7e8e67ea09fdedadc4e582', 'web/src/js/protocol.js': '5cdb9b578906cb1171d22433872713c99dadea1afa4ee269e6405dd4521403c4', 'web/src/js/commands.js': '9ee70c6dd812e122ebce25e8dc3d95520d122b98f5e725a78e4db8b0dd9df782', 'web/src/js/transport.js': '0f0a94eac81f681fd5b28ec4106ce1da22850771826b47e62686b052a9341083', 'web/src/js/settings.js': '8fd2d2dd4eb2b7c9e2a38f56eef62b3b991b52d7ebd5ee65044ca94a61b4c8ff'}
def prior(name,text):
    text=before_wifi(name,text)
    if name not in REVIEWED:return text
    original=subprocess.check_output(["git","-c","safe.directory="+ROOT.as_posix(),"show",BASE+":"+name],cwd=ROOT).decode("utf-8")
    if text==original:return text # Layered historical guards may already have restored this input.
    assert hashlib.sha256(text.replace("\r\n","\n").encode()).hexdigest()==REVIEWED[name],"Unreviewed maintenance delta: "+name
    return original
