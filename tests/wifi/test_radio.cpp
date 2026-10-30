// Compile the exact worker radio adapter with SDK fakes. No RF/flash claims.
#include "wifi_configuration.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
enum {ESP_OK=0,ESP_ERR_WIFI_NOT_STARTED=1,ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED=2,
      WIFI_IF_AP=0,WIFI_AUTH_WPA2_PSK=3,WIFI_CIPHER_TYPE_CCMP=4,ESP_NETIF_OP_SET=0,ESP_NETIF_OP_GET=1,
      ESP_NETIF_ROUTER_SOLICITATION_ADDRESS=32,ESP_NETIF_DHCP_STOPPED=0,ESP_NETIF_DHCP_STARTED=1};
using esp_netif_dhcp_status_t=int;
struct esp_netif_t {bool up=true;int dhcp=1;uint8_t router=0;};
struct wifi_config_t {struct {uint8_t ssid[32]{},password[64]{};uint8_t ssid_len=0,channel=0,max_connection=0;int authmode=0,pairwise_cipher=0;} ap;};
struct esp_netif_ip_info_t {struct {uint32_t addr;} ip,gw,netmask;};
#define ESP_IP4TOADDR(a,b,c,d) (uint32_t(a)|(uint32_t(b)<<8)|(uint32_t(c)<<16)|(uint32_t(d)<<24))
#define pdMS_TO_TICKS(ms) (ms)
constexpr unsigned max_stations=4;
esp_netif_t net;wifi_config_t configured;int64_t now=0;unsigned step=0,fail=0;bool never_up=false,bad_readback=false,bad_router=false;
int operation(){return ++step==fail?-1:ESP_OK;}
int64_t esp_timer_get_time(){now+=1000;return now;}
void vTaskDelay(int ms){now+=ms*1000;assert(now<6000000);}
int esp_wifi_stop(){int rc=operation();if(rc==0){net.up=false;net.dhcp=ESP_NETIF_DHCP_STOPPED;}return rc;}
bool esp_netif_is_netif_up(esp_netif_t* n){return n->up;}
int esp_netif_dhcps_stop(esp_netif_t* n){int rc=operation();if(!rc)n->dhcp=0;return rc;}
int esp_netif_dhcps_option(esp_netif_t* n,int op,int id,void* p,unsigned size){assert(id==32&&size==1);int rc=operation();if(rc)return rc;if(op==ESP_NETIF_OP_SET){assert(n->dhcp==0);n->router=*static_cast<uint8_t*>(p);}else *static_cast<uint8_t*>(p)=bad_router?1:n->router;return 0;}
int esp_wifi_set_config(int interface,wifi_config_t* c){assert(interface==WIFI_IF_AP&&c->ap.channel==6&&c->ap.max_connection==4&&c->ap.authmode==WIFI_AUTH_WPA2_PSK&&c->ap.pairwise_cipher==WIFI_CIPHER_TYPE_CCMP);int rc=operation();if(!rc)configured=*c;return rc;}
int esp_wifi_start(){int rc=operation();if(!rc){net.up=!never_up;net.dhcp=1;}return rc;}
int esp_netif_dhcps_get_status(esp_netif_t* n,int* status){int rc=operation();*status=n->dhcp;return rc;}
int esp_netif_dhcps_start(esp_netif_t* n){int rc=operation();if(!rc)n->dhcp=1;return rc;}
int esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t* ip){ip->ip.addr=ip->gw.addr=ESP_IP4TOADDR(192,168,4,1);ip->netmask.addr=ESP_IP4TOADDR(255,255,255,0);return operation();}
int esp_wifi_get_config(int interface,wifi_config_t* c){assert(interface==WIFI_IF_AP);int rc=operation();*c=configured;if(bad_readback)c->ap.password[0]^=1;return rc;}
#include "radio_under_test.h"
int main(){
 softap_policy::Credentials c{};strcpy(c.ssid,"01234567890123456789012345678901");memset(c.pass,'X',63);c.pass[63]=0;
 assert(wifiApplyAP(&net,c));const auto operations=step;
 assert(configured.ap.ssid_len==32&&configured.ap.password[63]==0&&net.router==0);
 for(unsigned failure=1;failure<=operations;++failure){net=esp_netif_t{};step=0;now=0;fail=failure;assert(!wifiApplyAP(&net,c));}
 fail=0;step=0;now=0;net=esp_netif_t{};never_up=true;assert(!wifiApplyAP(&net,c));assert(now>=2500000);never_up=false;
 for(bool router:{false,true}){step=0;now=0;net=esp_netif_t{};bad_router=router;bad_readback=!router;assert(!wifiApplyAP(&net,c));}
 puts("PASS exact worker AP adapter: stop/set/start, channel/security/station preservation, 32/63 limits, DHCP router suppression, full credential/IP readback, every SDK failure and bounded netif timeout");
}
