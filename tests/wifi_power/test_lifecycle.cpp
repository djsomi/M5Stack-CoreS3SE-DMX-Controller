// Exact production lifecycle/request/preference code, SDK/NVS fakes only.
// Does not establish radio behavior, SDK scheduling, RF or electrical timing.
#include "network_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
using network_service::Phase;using network_service::Status;using network_service::Memory;
using esp_err_t=int;
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=257,ESP_ERR_INVALID_STATE=259,
 ESP_ERR_TIMEOUT=263,ESP_ERR_WIFI_NOT_STARTED=10,ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED=11,
 WIFI_STORAGE_RAM=0,WIFI_MODE_AP=2,WIFI_IF_AP=0,WIFI_AUTH_WPA2_PSK=3,WIFI_CIPHER_TYPE_CCMP=4,
 ESP_NETIF_OP_SET=0,ESP_NETIF_OP_GET=1,ESP_NETIF_ROUTER_SOLICITATION_ADDRESS=32,
 ESP_NETIF_DHCP_STARTED=1,WIFI_EVENT=1,ESP_EVENT_ANY_ID=-1,HTTP_GET=0,PT_U8=1};
#define ESP_IP4TOADDR(a,b,c,d) (uint32_t(a)|(uint32_t(b)<<8)|(uint32_t(c)<<16)|(uint32_t(d)<<24))
#define IP4_ADDR(p,a,b,c,d) ((p)->addr=ESP_IP4TOADDR(a,b,c,d))
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_NETIF_DEFAULT_WIFI_AP() esp_netif_config_t{}
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
struct esp_netif_t{bool up=false;};
struct esp_netif_config_t{};
using esp_event_handler_instance_t=void*;
using esp_netif_dhcp_status_t=int;
struct esp_netif_ip_info_t {struct{uint32_t addr=0;}ip,gw,netmask;};
struct wifi_config_t{struct{uint8_t ssid[32]{},password[64]{};uint8_t ssid_len=0,channel=0,max_connection=0;int authmode=0,pairwise_cipher=0;}ap;};
struct wifi_init_config_t{unsigned nvs_enable=1,static_rx_buf_num=1,dynamic_rx_buf_num=1,static_tx_buf_num=1,cache_tx_buf_num=1,tx_buf_type=1,dynamic_tx_buf_num=1,mgmt_sbuf_num=1,ampdu_rx_enable=1,rx_ba_win=1,rx_mgmt_buf_type=1,rx_mgmt_buf_num=1;};
struct httpd_config_t{int core_id,task_priority,stack_size,max_open_sockets,max_uri_handlers,max_resp_headers,backlog_conn,recv_wait_timeout,send_wait_timeout;bool lru_purge_enable;int open_fn,close_fn,uri_match_fn;};
struct httpd_uri_t{const char* uri;int method,handler;bool is_websocket,handle_ws_control_frames;};
int opened=1,closed=2,websocket=3,serve=4,httpd_uri_match_wildcard=5;
httpd_config_t HTTPD_DEFAULT_CONFIG(){return {};}
Status shared{};int mux=0;void* worker_task=(void*)1;void* application_owner=(void*)2;void* caller=application_owner;
void* xTaskGetCurrentTaskHandle(){return caller;}
bool lifecycle_fault=false,canvas_available=true,netif_initialized=false,default_events_ready=false;
unsigned netif_calls=0,event_loop_calls=0;
uint32_t power_request=1;
constexpr unsigned max_stations=4,max_sessions=6;
softap_policy::Credentials credentials{};
uint8_t ap_mac[6]={1,2,3,4,5,6};
bool sweep_pending=false;int64_t sweep_queued_at=0;
struct{bool stopping=false;int server_stop_rc=0,last_work_rc=0;unsigned work_queued=0,work_failed=0;}ws_diag;
struct{void clear(){}}stations;
void publishStations(){shared.clients=0;}
void* server=nullptr;
esp_netif_t net;wifi_config_t configured{};esp_netif_ip_info_t ip{};
bool radio=false,initialized=false,handler=false,server_alive=false;
int64_t now=0;unsigned calls=0,fail_call=0,cycles=0,closes=0;
bool bad_stop=false,bad_deinit=false,bad_http_stop=false,bad_unregister=false;
bool pending_callback=false,stale_callback=false;
std::vector<std::string> events;
std::function<void()> during_delay;
int operation(){return ++calls==fail_call?ESP_FAIL:ESP_OK;}
int64_t esp_timer_get_time(){return now;}
struct Stop{};
void vTaskDelay(int ms){now+=ms*1000;if(during_delay)during_delay();}
void phase(Phase p,int e=0){shared.phase=p;shared.error=e;}
Status status(){return shared;}
Memory memory(){return {0,200000,190000,150000,140000,8000000};}
bool safe(const Memory&){return true;}
void record(unsigned,unsigned){shared.worker_stack=8192;}
void emitFirstFailure(){}
int sampleStations(){return 0;}
bool wifiChange(esp_netif_t*){return true;}
template<class F>int traced(const char*,F f,int=0){return f();}
void stationEvent(){}
int esp_netif_init(){++netif_calls;return operation();}
int esp_event_loop_create_default(){++event_loop_calls;return ESP_ERR_INVALID_STATE;} // Existing event loop is reused.
esp_netif_t* esp_netif_new(esp_netif_config_t*){if(operation())return nullptr;net.up=false;events.push_back("netif.new");return &net;}
int esp_netif_attach_wifi_ap(esp_netif_t*){return operation();}
int esp_wifi_set_default_wifi_ap_handlers(){return operation();}
int esp_wifi_init(wifi_init_config_t* c){assert(!c->nvs_enable);int rc=operation();if(!rc)initialized=true;return rc;}
int esp_wifi_set_storage(int mode){assert(mode==WIFI_STORAGE_RAM);return operation();}
int esp_wifi_set_mode(int mode){assert(mode==WIFI_MODE_AP);return operation();}
int esp_wifi_get_mac(int iface,uint8_t* mac){assert(iface==WIFI_IF_AP);memcpy(mac,ap_mac,6);return operation();}
int esp_wifi_set_config(int iface,wifi_config_t* ap){assert(iface==WIFI_IF_AP);configured=*ap;return operation();}
int esp_netif_dhcps_stop(esp_netif_t*){return operation();}
int esp_netif_dhcps_option(esp_netif_t*,int op,int id,void* value,unsigned size){assert(id==32&&size==1);if(op==ESP_NETIF_OP_GET)*(uint8_t*)value=0;else assert(*(uint8_t*)value==0);return operation();}
int esp_netif_set_ip_info(esp_netif_t*,esp_netif_ip_info_t* value){ip=*value;return operation();}
int esp_event_handler_instance_register(int,int,void(*)(),void*,void** out){int rc=operation();if(!rc){handler=true;*out=(void*)3;}return rc;}
int esp_wifi_start(){int rc=operation();if(!rc){radio=net.up=true;events.push_back("radio.start");}return rc;}
bool esp_netif_is_netif_up(esp_netif_t* n){return n->up;}
int esp_netif_dhcps_get_status(esp_netif_t*,int* dhcp){*dhcp=ESP_NETIF_DHCP_STARTED;return operation();}
int esp_netif_dhcps_start(esp_netif_t*){return operation();}
int esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t* out){*out=ip;return operation();}
int esp_wifi_get_config(int,wifi_config_t* out){*out=configured;return operation();}
int httpd_start(void** out,httpd_config_t* c){assert(c->core_id==0&&c->max_open_sockets==6&&!c->lru_purge_enable);int rc=operation();if(!rc){*out=(void*)4;server_alive=true;events.push_back("http.start");}return rc;}
int httpd_register_uri_handler(void*,httpd_uri_t*){return operation();}
void sweep(void*){assert(server_alive);sweep_pending=false;}
int httpd_queue_work(void*,void(*)(void*),void*){pending_callback=true;return operation();}
int httpd_stop(void*){events.push_back("http.stop");if(bad_http_stop)return ESP_FAIL;assert(server_alive);if(pending_callback){sweep(nullptr);pending_callback=false;}server_alive=false;++closes;return 0;}
int esp_wifi_stop(){events.push_back("radio.stop");if(bad_stop)return ESP_FAIL;radio=net.up=false;return 0;}
int esp_event_handler_instance_unregister(int,int,void*){if(bad_unregister)return ESP_FAIL;handler=false;return 0;}
void esp_netif_destroy_default_wifi(esp_netif_t*){assert(!server_alive&&!handler&&!radio);events.push_back("netif.destroy");}
int esp_wifi_deinit(){events.push_back("radio.deinit");if(bad_deinit)return ESP_FAIL;initialized=false;return 0;}
struct{bool busy=false;bool active()const{return busy;}}configuration_authority;
struct Preferences{
 static std::map<std::string,unsigned> values;
 static bool open_ok,write_ok,read_ok;static unsigned writes;
 bool begin(const char* ns,bool){assert(!strcmp(ns,"dmxwifi"));return open_ok;}
 bool isKey(const char* key){return values.count(key);}
 int getType(const char* key){return values.count(key)?(values[key]>1?2:PT_U8):0;}
 unsigned getUChar(const char* key,unsigned fallback){return read_ok&&values.count(key)?values[key]:fallback;}
 unsigned putUChar(const char* key,unsigned value){assert(!strcmp(key,"enabled"));++writes;if(!write_ok)return 0;values[key]=value;return 1;}
 void end(){}
};
std::map<std::string,unsigned> Preferences::values;bool Preferences::open_ok=true,Preferences::write_ok=true,Preferences::read_ok=true;unsigned Preferences::writes=0;
#include "lifecycle_under_test.h"
void reset(){shared={};shared.enabled=true;shared.phase=Phase::ready;server=nullptr;radio=initialized=handler=server_alive=false;lifecycle_fault=false;canvas_available=true;netif_initialized=default_events_ready=false;netif_calls=event_loop_calls=0;power_request=1;now=calls=fail_call=cycles=closes=0;events.clear();bad_stop=bad_deinit=bad_http_stop=bad_unregister=false;pending_callback=stale_callback=sweep_pending=false;during_delay=nullptr;caller=application_owner;configuration_authority.busy=false;Preferences::values.clear();Preferences::open_ok=Preferences::write_ok=Preferences::read_ok=true;Preferences::writes=0;strcpy(credentials.ssid,"Saved Custom AP");strcpy(credentials.pass,"SavedPassword");}
void stopAtReady(){during_delay=[](){if(shared.phase==Phase::ready){assert(shared.radio_on&&radio&&server_alive);shared.enabled=false;shared.phase=Phase::stopping;}};}
void preference(){Preferences storage;
#include "preference_under_test.h"
}
int main(){
 reset();preference();assert(shared.enabled&&shared.power_error==0&&Preferences::writes==0);
 Preferences::values["enabled"]=0;preference();assert(!shared.enabled);
 Preferences::values["enabled"]=1;preference();assert(shared.enabled);
 Preferences::values["enabled"]=2;preference();assert(!shared.enabled&&shared.power_error);
 reset();stopAtReady();assert(runNetwork());assert(!radio&&!initialized&&!server_alive&&!shared.radio_on&&shared.phase==Phase::disabled&&!sweep_pending);assert(closes==1);
 const unsigned operations=calls;
 assert(events[2]=="http.start");auto http=std::find(events.begin(),events.end(),"http.stop"),stop=std::find(events.begin(),events.end(),"radio.stop");assert(http<stop);
 for(unsigned fail=1;fail<=operations;++fail){reset();fail_call=fail;stopAtReady();assert(runNetwork());assert(!radio&&!server_alive&&!initialized);assert(!shared.radio_on);}
 reset();stopAtReady();assert(runNetwork());const auto ssid=configured.ap.ssid;assert(!strcmp((const char*)ssid,"Saved Custom AP"));assert(configured.ap.channel==6&&configured.ap.max_connection==4&&configured.ap.authmode==WIFI_AUTH_WPA2_PSK);
 assert(requestEnabled(true));assert(Preferences::values["enabled"]==1&&shared.phase==Phase::starting);assert(runNetwork());assert(closes==2&&netif_calls==1&&event_loop_calls==1); // Same owner, fresh SDK/server cycle.
 reset();shared.radio_on=true;assert(requestEnabled(false));assert(!shared.enabled&&shared.radio_on&&shared.phase==Phase::stopping);preference();assert(!shared.enabled); // Persisted OFF, no credential writes.
 reset();configuration_authority.busy=true;assert(!requestEnabled(false)&&Preferences::writes==0);configuration_authority.busy=false;caller=nullptr;assert(!requestEnabled(false));caller=application_owner;
 for(auto p:{Phase::prepared,Phase::starting,Phase::stopping}){shared.phase=p;assert(!requestEnabled(false));}shared.phase=Phase::ready;
 Preferences::write_ok=false;assert(!requestEnabled(false)&&shared.enabled&&shared.power_error);Preferences::write_ok=true;assert(requestEnabled(false));const auto writes=Preferences::writes;shared.phase=Phase::disabled;assert(requestEnabled(false));assert(writes==Preferences::writes);
 reset();Preferences::read_ok=false;assert(!requestEnabled(false)&&shared.enabled&&shared.power_error);
 for(unsigned fault=0;fault<4;++fault){reset();stopAtReady();bad_stop=fault==0;bad_deinit=fault==1;bad_http_stop=fault==2;bad_unregister=fault==3;assert(!runNetwork());assert(lifecycle_fault&&shared.phase==Phase::failed);assert(!requestEnabled(true));}
 reset();canvas_available=false;assert(runNetwork());assert(shared.phase==Phase::failed&&!radio&&!shared.radio_on&&calls==0);
 reset();shared.enabled=false;during_delay=[](){if(++cycles==5)throw Stop{};};try{worker(nullptr);}catch(Stop&){}assert(calls==0&&shared.phase==Phase::disabled);
 reset();fail_call=1;during_delay=[](){if(++cycles==5)throw Stop{};};try{worker(nullptr);}catch(Stop&){}assert(calls==1&&shared.phase==Phase::failed); // No implicit retry.
 puts("PASS exact lifecycle/preference/request: default/persisted/corrupt state, OFF/ON, AP config, every startup failure, queued callback drain, teardown order/fail-closed, conflicts, NVS errors, owner admission and no automatic restart");
}
