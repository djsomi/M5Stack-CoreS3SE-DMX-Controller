"""Exact Stage 6B.5C reconstruction for historical freeze checks.
New approval/journal/radio/UI behavior is exercised on current production sources
by tests/wifi. Pins are explicit review inputs, not updated by test execution.
"""
from pathlib import Path
import hashlib,subprocess
ROOT=Path(__file__).resolve().parents[2]
BASE="3730ea421a5a10d6f2be4451df6328e18c1e3aa1"
REVIEWED={'src/network_service.cpp': '00c726a063ed8d1711477f3336f1bd598fe11edc3c0eed1faf874f8ab35910fb', 'src/src.cpp': '65b2967ce54c5f59bcc497876f85925793417a8edc00c36fed55375a38c40882', 'src/view_sender.h': '78e62df176198f5630c8947a836a427b87bdabea19f786a0960a81dcbc5f755e', 'src/websocket_bridge.cpp': '63c02961d138af1692d7f445ef6a172dc6db28c0f53750a40a261ec1f157c77a', 'src/websocket_protocol.cpp': '79180479a471c6d8c81deb6e019ab1766c2eaf99aee3b5d6865582f3f53933a7', 'src/websocket_service.cpp': '95c9953476476800be63f035a8f925f9f77fe05d92b554ec9f6ee06eb8d0d9ff', 'include/network_service.h': '500e600ca4d378f73282d2cf08fedeea719319af447d2770ac0922389310340a', 'include/websocket_bridge.h': '583b092395bbb2ee41aec3d8e5314a4bca65d37b36e861d9b4d381d67adc570e', 'include/websocket_service.h': '38477b7893659b0eff43e70d47754740525d688e19b8d1d66cf3a265ccbf3d2c', 'include/websocket_diagnostics.h': 'e01a358b2e54697bdbf284d8f03c90b6c15e2ff28727873b31a9b1246ddb4da1', 'web/src/js/protocol.js': 'd16fd89a2809c4c3f8481ef7f8cee43cad034ed9ab16a030e38d0b38ff5e5234', 'web/src/js/commands.js': '944d65d4cc306ebbfa2738b3f01602b83bc79e9de0f8a549075e1b3f10c4b2ac', 'web/src/js/transport.js': '3a408954200cae676c21322dba97f4e54cfa9cd625935bd7141791e953943615', 'web/src/js/settings.js': '96a5b45ddaa48be304e1e63132f10794d83d44835ca1b1ac86ba093a70df058d', 'include/device_settings.h': '3249e47f0c84bdc33147d14dbd802305af17ffebac7187410ae5622a079fb647', 'web/src/js/app.js': '45d632d1fb71a06262a24ddcbfbf3a592253f81d713d471602d1f5a1c1fa6409'}
NEW={'include/wifi_configuration.h': 'bd297f5a94c97d3a88642db047242e46797f060fae29c2044643d07c0d0d2431', 'include/wifi_credential_transaction.h': 'f1ecefd6a563eb015bfdaeecff4793894611128fb08abe91803b7bf5c77d71dd', 'include/wifi_wire.h': 'e337a28da9f197c42a95ac7ca2f1f4fd75d11ecb88bff3b03fa5f7038c5bce72', 'src/wifi_credential_store.h': '08f308740856a96531553ed39a22ab820b287910ce215c3f86e1038b206097b7'}
def prior(name,text):
    if name not in REVIEWED:return text
    original=subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),'show',BASE+':'+name],cwd=ROOT).decode('utf-8')
    if text==original:return text
    assert hashlib.sha256(text.replace('\r\n','\n').encode()).hexdigest()==REVIEWED[name],'Unreviewed Wi-Fi delta: '+name
    return original

def check_new():
    for name,expected in NEW.items():
        assert hashlib.sha256((ROOT/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,'Unreviewed Wi-Fi input: '+name
