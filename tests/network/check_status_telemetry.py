"""Exact /status changes and reviewed HTTP deadline guard; formatted header contract."""
from pathlib import Path
import ast
import json
import re
import subprocess

root = Path(__file__).resolve().parents[2]
original = subprocess.check_output(["git", "-c", "safe.directory=" + root.as_posix(), "show", "5b3cea7:src/network_service.cpp"], cwd=root).decode("utf-8")
current = (root / "src/network_service.cpp").read_text(encoding="utf-8")
old = '        int n=snprintf(json,sizeof(json),\n            "{\\"stage\\":5,'
new = '        const auto& bundle=web_assets::bundle();\n        int n=snprintf(json,sizeof(json),\n            "{\\"build\\":\\"%s\\",\\"profile\\":\\"%s\\",\\"layout\\":\\"%s\\",\\"ms\\":%lu,\\"stage\\":5,'
args = '            phaseName(s.phase),s.ip,(unsigned long)s.clients'
new_args = '            bundle.source_sha256,bundle.profile,bundle.layout,(unsigned long)(uint32_t(now/1000)),\n' + args
assert original.count(old) == original.count(args) == 1
expected = original.replace(old,new).replace(args,new_args)
expected = '#include "status_ssid.h"\n' + expected
query = """        wifi_config_t active{};
        char ssid_json[67];
        const bool have_ssid=esp_wifi_get_config(WIFI_IF_AP,&active)==ESP_OK &&
            status_telemetry::ssidJson(active.ap.ssid,active.ap.ssid_len?active.ap.ssid_len:
                strnlen(reinterpret_cast<const char*>(active.ap.ssid),sizeof(active.ap.ssid)),ssid_json);
"""
expected = expected.replace('        const auto& bundle=web_assets::bundle();\n', '        const auto& bundle=web_assets::bundle();\n' + query)
expected = expected.replace(r'\"layout\":\"%s\",\"ms\":', r'\"layout\":\"%s\",\"ssid\":%s,\"ms\":')
expected = expected.replace('bundle.layout,(unsigned long)', 'bundle.layout,have_ssid?ssid_json:"null",(unsigned long)')
# Permit only the reviewed ordinary-HTTP guard; preserve the complete WS branch.
deadline_old = '    if(remaining<=0) return HTTPD_SOCK_ERR_FAIL;\n'
deadline_new = ('    // lwIP truncates timeval to milliseconds; zero would mean unlimited wait.\n'
                '    if(remaining<1000) return HTTPD_SOCK_ERR_FAIL;\n')
assert expected.count(deadline_old) == 1
expected = expected.replace(deadline_old, deadline_new)
assert current == expected, "Unrelated network behavior changed"
assert 'esp_wifi_get_config(WIFI_IF_AP,&active)==ESP_OK &&' in current
assert 'active.ap.password' not in current and 'esp_wifi_set_config' not in query

# Interpret the actual literal format, not a separately maintained response schema.
block = current.split('int n=snprintf(json,sizeof(json),',1)[1].split('bundle.source_sha256',1)[0]
header = ''.join(ast.literal_eval(literal) for literal in re.findall(r'"(?:\\.|[^"\\])*"',block))
values = ('a'*64, 'cores3se_unit', 'cores3se-16m-ota6-v1', json.dumps('Actual AP \"quoted\"'), 123456, 'READY', '192.168.4.1', 3, 4, 'false', 100, 2, 456, 0, 'false', 4096, 2048)
formatted = header.replace('%lu','%d') % values
response = json.loads(formatted + ']}')
missing_values=list(values);missing_values[3]='null'
assert json.loads((header.replace('%lu','%d') % tuple(missing_values)) + ']}')['ssid'] is None
assert response['build'] == 'a'*64 and response['profile'] == 'cores3se_unit'
assert response['ssid'] == 'Actual AP \"quoted\"'
assert response['ms'] == 123456 and response['stations'] == 3 and not response['dhcp_router_option']
assert response['worker_stack_hwm'] == 4096 and response['tx_stopped'] is False
assert 'const Memory fixed[]={s.before,s.after,s.current};' in current
assert '90+i' in current and 'static char json[4096]' in current
assert 'max_sessions = 6' in current and 'if(++window_requests>12)' in current
assert 'httpd_resp_set_hdr(r,"Connection","close")' in current
assert 'heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)' in current
print('PASS exact native /status additions and ordinary-HTTP sub-ms guard, actual metadata schema, event92 memory and unchanged WS/radio/task/socket/byte/session limits')

# Use every actual /status format string with maximum-width numeric values.
branch = current.split('const auto& bundle=web_assets::bundle();',1)[1].split('if(!header(r,"Accept-Encoding"',1)[0]
formats = [ast.literal_eval(x) for x in re.findall(r'(?:int n=|n\+=)snprintf\(json[^,]*,sizeof\(json\)[^,]*,\s*("(?:\\.|[^"\\])*"),',branch)]
assert len(formats) == 4
maximum = 4294967295
header_values = ('a'*64,'cores3se_unit_diagnostics','cores3se-16m-ota6-v1',json.dumps('\"'*32),maximum,'PREPARED','255.255.255.255',maximum,maximum,'false',maximum,maximum,maximum,maximum,'false',maximum,maximum)
def expand(fmt, values):
    return fmt.replace('%lu','%d').replace('%u','%d') % values
payload = expand(formats[0],header_values)
for i in range(3):
    count = len(re.findall(r'%(?:lu|u|s|d)',formats[1]))
    payload += expand(formats[1],('' if i==0 else ',',90+i,*([maximum]*(count-2))))
for i in range(8):
    count = len(re.findall(r'%(?:lu|u|s|d)',formats[2]))
    payload += expand(formats[2],(',',*([maximum]*(count-1))))
count = len(re.findall(r'%(?:lu|u|s|d)',formats[3]))
payload += expand(formats[3],tuple([maximum]*count))
assert len(payload.encode('ascii')) < 4096
parsed = json.loads(payload)
assert len(parsed['samples']) == 11 and parsed['samples'][2]['event'] == 92
assert parsed['ssid'] == '\"'*32
assert parsed['build'] == 'a'*64 and 'password' not in parsed
print('PASS actual maximum-width /status response: %d / 4096 bytes' % len(payload))
