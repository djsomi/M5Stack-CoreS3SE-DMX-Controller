#include "softap_policy.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <string>
using namespace softap_policy;

// Native DHCP-call fragments are extracted unchanged by check_softap_integration.py.
enum {ESP_NETIF_OP_SET,ESP_NETIF_OP_GET,ESP_NETIF_ROUTER_SOLICITATION_ADDRESS=32};
struct FakeNetif {bool started=false;uint8_t router=1;int forced_error=0;};
int esp_netif_dhcps_option(FakeNetif* n,int op,int id,void* value,uint32_t size) {
    assert(id==ESP_NETIF_ROUTER_SOLICITATION_ADDRESS && size==1 && value);
    if(n->forced_error) return n->forced_error;
    if(op==ESP_NETIF_OP_SET) {if(n->started) return 101; n->router=*static_cast<uint8_t*>(value);}
    else {if(!n->started) return 102; *static_cast<uint8_t*>(value)=n->router;}
    return 0;
}
#include "dhcp_under_test.h"
int main() {
    uint8_t mac[]={0x02,0x11,0x22,0x33,0x72,0x9c};
    Credentials out{};
    factory(mac,out);
    assert(!strcmp(out.ssid,"CoreS3SE-DMX-729C") && !strcmp(out.pass,"espdmx-729c"));
    assert(validPassword(out.pass,sizeof(out.pass)) && out.origin==Origin::factory && out.version==2);
    Credentials again{};factory(mac,again);assert(!memcmp(&again,&out,sizeof(out)));
    mac[0]=0x06;factory(mac,again);assert(!strcmp(again.pass,out.pass)); // Only last two AP bytes.
    mac[4]=0;mac[5]=1;factory(mac,again);assert(!strcmp(again.ssid,"CoreS3SE-DMX-0001"));
    mac[4]=0x72;mac[5]=0x9c;
    assert(select(nullptr,0,false,false,mac,again)==Decision::write_factory);
    assert(select(&out,sizeof(out),true,false,mac,again)==Decision::keep);
    assert(select(nullptr,0,true,false,mac,again)==Decision::invalid);
    assert(select(nullptr,999,true,true,mac,again)==Decision::write_factory); // Explicit local recovery.
    LegacyCredentials old{};old.version=1;
    strcpy(old.ssid,"CoreS3SE-DMX-02112233729C");strcpy(old.pass,"ABCDEFGHJKLMNPQRSTUVWXYZ");
    const auto unchanged=old;
    assert(select(&old,sizeof(old),true,false,mac,again)==Decision::keep);
    assert(again.origin==Origin::legacy && !strcmp(again.pass,old.pass));
    assert(!memcmp(&old,&unchanged,sizeof(old))); // Existing ambiguous factory/custom blob untouched.
    strcpy(old.ssid,"User Custom SSID");strcpy(old.pass,"Custom-Key-24-Character!");
    assert(select(&old,sizeof(old),true,false,mac,again)==Decision::keep);
    assert(again.origin==Origin::legacy && !strcmp(again.ssid,old.ssid));
    assert(select(&old,sizeof(old),true,true,mac,again)==Decision::write_factory);
    Credentials custom{};custom.version=2;custom.origin=Origin::custom;
    strcpy(custom.ssid,"Strong-user-setting");memset(custom.pass,'x',63);custom.pass[63]=0;
    assert(select(&custom,sizeof(custom),true,false,mac,again)==Decision::keep);
    assert(again.origin==Origin::custom && strlen(again.pass)==63);
    for(const char* p:{"","short","1234567","invalid\npassword"}) assert(!validPassword(p,strlen(p)+1));
    assert(validPassword("12345678",9));
    char unterminated[64];memset(unterminated,'x',64);assert(!validPassword(unterminated,64));
    out.origin=Origin::factory;out.pass[0]='X';assert(select(&out,sizeof(out),true,false,mac,again)==Decision::invalid);
    custom.version=99;assert(select(&custom,sizeof(custom),true,false,mac,again)==Decision::invalid);
    Stations stations;
    uint8_t clients[DMX_WIFI_MAX_STATIONS+1][6]{};
    for(unsigned i=0;i<DMX_WIFI_MAX_STATIONS+1;++i) {clients[i][0]=2;clients[i][5]=uint8_t(i+1);}
    for(unsigned i=0;i<DMX_WIFI_MAX_STATIONS;++i) {assert(stations.connect(clients[i]));assert(!stations.connect(clients[i]));}
    assert(stations.count()==DMX_WIFI_MAX_STATIONS && !stations.connect(clients[DMX_WIFI_MAX_STATIONS]));
    assert(!stations.disconnect(clients[DMX_WIFI_MAX_STATIONS]));
    assert(stations.disconnect(clients[0]) && !stations.disconnect(clients[0]));
    assert(stations.connect(clients[DMX_WIFI_MAX_STATIONS]));
    stations.clear();assert(stations.count()==0 && !stations.disconnect(clients[0]));
    FakeNetif netif;
    assert(set_router_under_test(&netif)==0 && netif.router==0); // Before leases/AP start.
    uint8_t readback=1;assert(get_router_under_test(&netif,&readback)==102);
    netif.started=true;
    assert(get_router_under_test(&netif,&readback)==0 && readback==0);
    assert(set_router_under_test(&netif)==101); // Cannot silently set after lease start.
    netif.forced_error=257;assert(get_router_under_test(&netif,&readback)==257);
    puts("PASS deterministic AP suffix, WPA2 validation, preserved legacy/custom records, explicit factory reset, bounded station sets; actual DHCP-call arguments/error simulation (real leases/iOS remain hardware tests)");
}
