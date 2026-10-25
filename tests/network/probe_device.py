"""Manual Stage 5C read-only HTTP acceptance. Run only after the user uploads/connects.
No Wi-Fi provisioning, serial, reset, firmware upload or controller command.
"""
import argparse
import gzip
import hashlib
import http.client
import json
from pathlib import Path
import socket
import time
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',required=True,help='confirmed numeric AP address from Settings')
p.add_argument('--output',type=Path,required=True,help='NEW output log')
a=p.parse_args()
if a.output.exists(): p.error('refusing to overwrite log')
manifest=json.loads((Path(__file__).resolve().parents[2]/'web/generated/manifest.json').read_text())
with a.output.open('x',encoding='utf-8') as log:
    def get(path,encoding='gzip',host=None):
        c=http.client.HTTPConnection(a.host,80,timeout=6)
        try:
            c.request('GET',path,headers={'Accept-Encoding':encoding,'Host':host or a.host})
            r=c.getresponse();body=r.read();headers=dict(r.getheaders())
            log.write(json.dumps({'path':path,'status':r.status,'headers':headers,'bytes':len(body)})+'\n');log.flush()
            return r.status,headers,body
        finally: c.close()
    for entry in manifest['assets']:
        code,h,b=get(entry['path'])
        assert code==200 and h.get('Content-Encoding')=='gzip'
        assert int(h['Content-Length'])==len(b)==entry['gzip_size']
        assert hashlib.sha256(b).hexdigest()==entry['sha256']
        assert len(gzip.decompress(b))==entry['original_size']
        assert h['Content-Type']==entry['content_type']
        assert ('no-store' if entry['path']=='/' else 'immutable') in h['Cache-Control']
    assert get('/unknown')[0]==404
    assert get('/',encoding='identity')[0]==406
    assert get('/',host='untrusted.example')[0]==403
    code,_,body=get('/status');assert code==200
    data=json.loads(body);assert data['stage']==5 and data['phase']=='READY'
    assert data['dhcp_router_option'] is False
    assert 1<=data['max_stations']<=8 and 0<=data['stations']<=data['max_stations']
    log.write('MEMORY '+json.dumps(data)+'\n');log.flush()
    # No-byte connections must also retire, not just drip-fed requests.
    idle=socket.create_connection((a.host,80),timeout=6)
    idle.settimeout(6);time.sleep(5)
    try:
        assert idle.recv(1)==b'', 'idle session was not closed'
    except ConnectionResetError:
        pass
    finally: idle.close()
    log.write('PASS assets/headers/negotiation/404/Host/status/idle timeout\n')
print('PASS read-only HTTP checks. Browser rendering, radio/memory/DMX and bad-password testing remain manual.')
