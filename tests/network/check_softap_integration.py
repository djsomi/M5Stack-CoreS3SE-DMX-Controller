"""Stage5C wiring/ownership checks; no hardware, passwords or NVS access."""
from pathlib import Path
from wifi_guard_support import prior as before_wifi
import argparse,re
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--dhcp-header',type=Path)
p.add_argument('--station-header',type=Path)
a=p.parse_args()
net=before_wifi('src/network_service.cpp',(root/'src/network_service.cpp').read_text(encoding='utf-8'))
view=(root/'src/view_sender.h').read_text(encoding='utf-8')
config=(root/'include/softap_config.h').read_text()
assert '#define DMX_WIFI_MAX_STATIONS 4' in config
assert 'ap.ap.max_connection=max_stations' in net and 'max_sessions = 6' in net
assert 'esp_read_mac(ap_mac,ESP_MAC_WIFI_SOFTAP)' in net and 'esp_wifi_get_mac(WIFI_IF_AP,actual_mac)' in net
assert 'WIFI_AUTH_WPA2_PSK' in net and 'WIFI_MODE_APSTA' not in net
assert net.index('esp_netif_dhcps_stop(netif)')<net.index('ESP_NETIF_OP_SET')<net.index('esp_wifi_start()')
assert net.index('esp_netif_dhcps_start(netif)')<net.index('ESP_NETIF_OP_GET')
assert 'router_readback!=0' in net and 'shared.router_option_disabled=true' in net
assert 'IP4_ADDR(&ip.ip,192,168,4,1)' in net and 'IP4_ADDR(&ip.netmask,255,255,255,0)' in net
for event in ['WIFI_EVENT_AP_STACONNECTED','WIFI_EVENT_AP_STADISCONNECTED','WIFI_EVENT_AP_STOP','WIFI_EVENT_AP_START']:
    assert event in net
callback=net.split('void stationEvent(',1)[1].split('Memory memory()',1)[0]
assert not any(x in callback for x in ['M5.','Preferences','credentials','esp_wifi_ap_get_sta_list','Serial.'])
assert 'generation==station_generation' in net and 'maintenance<1000000' in net
assert 'if(!check(sampleStations()))' in net
assert net.count('esp_wifi_ap_get_sta_list(&list)')==1
assert 'emitStations(true)' in net and 'AP_STATIONS' in net
assert 'shared.clients=stations.count()' in net and '++shared.ui_revision' in net
handler=net.split('esp_err_t serve(',1)[1].split('void worker(',1)[0]
assert not any(x in handler for x in ['password','credentials','requestFactoryReset','Preferences'])
prepare=net.split('void prepare()',1)[1].split('void launch(',1)[0]
assert 'decision==softap_policy::Decision::write_factory' in prepare
assert prepare.index('storage.putBytes("ap1"')<prepare.index('storage.remove("factory")')
assert 'storage.clear(' not in net and 'storage.remove("ap1")' not in net
assert set(re.findall(r'storage.begin\("([^"]+)"',net))=={'dmxwifi'}
reset=net.split('bool requestFactoryReset()',1)[1].split('const char* phaseName',1)[0]
assert 'xTaskGetCurrentTaskHandle()!=application_owner' in reset
assert 'storage.putUChar("factory",1)' in reset and 'putBytes' not in reset
assert 'WIFI Ready - Clients %lu/%lu' in view
assert 'network_service::uiRevision()' in view and 'status_y < settings_view_h && settings_wifi_notice_y + 10 - settings_scroll_y >= 0' in view
assert 'network_service::requestFactoryReset()' in view and '>= 2000' in view
assert 'RESTART TO APPLY' in view and 'RESET TAKES EFFECT AFTER REBOOT' in view and 'WIFI ONLY - PRESETS ARE KEPT' not in view
assert 'wifi_reset_completed = true;' in view and 'wifi_reset_canceled = true;' in view
if a.dhcp_header:
    target=a.dhcp_header.resolve();assert (root/'dist').resolve() in target.parents
    calls=re.findall(r'esp_netif_dhcps_option\(netif,ESP_NETIF_OP_(SET|GET),\s*ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,&(\w+),sizeof\(\2\)\)',net)
    assert calls==[('SET','offer_router'),('GET','router_readback')],calls
    def expr(op,var):
        return re.search(r'esp_netif_dhcps_option\(netif,ESP_NETIF_OP_'+op+r',\s*ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,&'+var+r',sizeof\('+var+r'\)\)',net).group()
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('int set_router_under_test(FakeNetif* netif) { uint8_t offer_router=0; return '+expr('SET','offer_router')+'; }\n'+
        'int get_router_under_test(FakeNetif* netif,uint8_t* output) { uint8_t router_readback=1; const auto rc='+expr('GET','router_readback')+'; *output=router_readback; return rc; }\n',encoding='utf-8')
if a.station_header:
    target=a.station_header.resolve();assert (root/'dist').resolve() in target.parents
    def function(signature):
        start=net.index(signature);opening=net.index('{',start);depth=0
        for i in range(opening,len(net)):
            if net[i]=='{':depth+=1
            if net[i]=='}':
                depth-=1
                if not depth:return net[start:i+1]+'\n'
        raise AssertionError(signature)
    state=net[net.index('#if DMX_HW_DIAGNOSTICS\nstruct StationDiagnostics {'):net.index('Memory memory()')]
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(state+'#if DMX_HW_DIAGNOSTICS\n'+function('void emitStations(')+'#endif\n'+function('esp_err_t sampleStations('),encoding='utf-8')
print('PASS DHCP-before-lease ordering/readback, real AP MAC source, safe credential/reset wiring, event-driven stations and owner-only visible Settings refresh; physical lease/cellular tests pending')
