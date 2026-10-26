// Actual network_service event/reconciliation/logging code with native API mocks.
#include "softap_policy.h"
#ifndef DMX_HW_DIAGNOSTICS
#define DMX_HW_DIAGNOSTICS 0
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%d: %s\n",__LINE__,#x);exit(1);}}while(0)
using esp_err_t=int;using esp_event_base_t=const char*;
enum {ESP_OK=0,ESP_FAIL=-1,WIFI_EVENT_AP_START=1,WIFI_EVENT_AP_STACONNECTED=2,
    WIFI_EVENT_AP_STADISCONNECTED=3,WIFI_EVENT_AP_STOP=4};
constexpr unsigned max_stations=DMX_WIFI_MAX_STATIONS;
struct wifi_event_ap_staconnected_t {uint8_t mac[6];};
struct wifi_event_ap_stadisconnected_t {uint8_t mac[6];};
struct wifi_sta_info_t {uint8_t mac[6]{};};
struct wifi_sta_list_t {wifi_sta_info_t sta[10];int num=0;};
softap_policy::Stations stations;
uint32_t station_generation=0;
struct {unsigned clients=0;uint32_t ui_revision=0;} shared;
struct {uint32_t wifi_disconnects=0,last_wifi_ms=0;} ws_diag;
int mux=0;
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
int64_t clock_us=1000000;
unsigned clock_reads=0;
int64_t esp_timer_get_time(){++clock_reads;return clock_us;}
wifi_sta_list_t native_list;int native_rc=0;unsigned queries=0;
std::function<void()> during_query;
int esp_wifi_ap_get_sta_list(wifi_sta_list_t* list) {
    ++queries;*list=native_list;
    if(during_query){auto action=during_query;during_query=nullptr;action();}
    return native_rc;
}
std::vector<std::string> logs;
void captureLog(const char*,const char* format,...) {
    char line[1024];va_list args;va_start(args,format);const int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
    CHECK(n>0&&unsigned(n)<sizeof(line));logs.emplace_back(line);
}
#define ESP_LOGW(tag,format,...) captureLog(tag,format,__VA_ARGS__)
#include "station_under_test.h"
void event(int type,unsigned tag=0) {
    wifi_event_ap_staconnected_t data{};data.mac[0]=2;data.mac[1]=0x11;
    data.mac[2]=0x22;data.mac[3]=0x33;data.mac[4]=uint8_t(tag>>8);data.mac[5]=uint8_t(tag);
    stationEvent(nullptr,nullptr,type,type==WIFI_EVENT_AP_START||type==WIFI_EVENT_AP_STOP?nullptr:&data);
}
void driver(unsigned count,unsigned tag=0x4455) {
    native_list=wifi_sta_list_t{};native_list.num=int(count);
    for(unsigned i=0;i<std::min(count,10U);++i) {
        const uint8_t mac[]={2,0x11,0x22,0x33,uint8_t(tag>>8),uint8_t(tag+i)};
        memcpy(native_list.sta[i].mac,mac,6);
    }
}
void reset() {
    stations.clear();shared.clients=0;shared.ui_revision=0;
#if DMX_HW_DIAGNOSTICS
    station_diag=StationDiagnostics{};station_trace_revision=1;station_trace_logged=0;station_trace_at=0;
#endif
    ws_diag.wifi_disconnects=ws_diag.last_wifi_ms=0;station_generation=0;
    clock_reads=0;native_rc=0;queries=0;clock_us=1000000;
    driver(0);during_query=nullptr;logs.clear();
}
int main() {
#if DMX_HW_DIAGNOSTICS
    reset();CHECK(shared.clients==0&&!station_diag.queried);
    event(WIFI_EVENT_AP_START);CHECK(queries==0&&station_diag.starts==1&&station_generation==0);
    CHECK(sampleStations()==ESP_OK&&shared.clients==0&&station_diag.native_count==0&&station_diag.applied);
    const auto boot_logs=logs.size();clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&logs.size()==boot_logs);
    clock_us+=30000000;CHECK(sampleStations()==ESP_OK&&logs.size()==boot_logs+1); // Fresh native sample without HTTP/events.
    event(WIFI_EVENT_AP_STACONNECTED,0x4455);event(WIFI_EVENT_AP_STACONNECTED,0x4455);
    CHECK(shared.clients==1&&stations.count()==1&&station_diag.connects==2&&station_diag.cycle_connects==2);
    driver(1);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&shared.clients==1&&station_diag.applied);
    CHECK(logs.back().find("ids=4455")!=std::string::npos);
    CHECK(logs.back().find("02:11:22:33")==std::string::npos&&logs.back().find("02112233")==std::string::npos);
    event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);
    CHECK(shared.clients==0&&station_diag.event_count==0&&station_diag.disconnects==2&&ws_diag.wifi_disconnects==2);
    driver(0);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&shared.clients==0&&station_diag.native_count==0);
    // The reported 1 -> 0 -> 1 is possible if the NATIVE list still contains a station.
    driver(1);clock_us+=1000000;const auto connects=station_diag.connects;
    CHECK(sampleStations()==ESP_OK&&shared.clients==1&&station_diag.connects==connects);
    CHECK(station_diag.event==StationDiagnostics::disconnect&&station_diag.native_count==1);
    // Conversely, a delayed connect notification is corrected by a native zero list.
    driver(0);event(WIFI_EVENT_AP_STACONNECTED,0x4455);clock_us+=1000000;
    CHECK(sampleStations()==ESP_OK&&shared.clients==0&&station_diag.native_count==0);
    // A callback during a native query invalidates that sample's publication.
    event(WIFI_EVENT_AP_STACONNECTED,0x4455);driver(1);
    during_query=[]{event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);};clock_us+=1000000;
    CHECK(sampleStations()==ESP_OK&&shared.clients==0&&!station_diag.applied);
    CHECK(station_diag.native_count==1&&station_diag.before!=station_diag.after);
    driver(0);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&station_diag.applied&&shared.clients==0);
    // Unreported replacement station/second station updates the bounded set.
    driver(1,0xAABB);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&shared.clients==1&&station_diag.ids[0]==0xAABB);
    if(max_stations>=2){driver(2);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&shared.clients==2);}
    // Error is not interpreted as a zero-station success; existing worker policy receives it.
    const auto old=shared.clients;native_rc=257;clock_us+=1000000;
    CHECK(sampleStations()==257&&shared.clients==old&&!station_diag.applied&&station_diag.native_count==-1);
    CHECK(logs.back().find("rc=257")!=std::string::npos);
    native_rc=0;driver(max_stations+1);clock_us+=1000000;
    CHECK(sampleStations()==ESP_FAIL&&shared.clients==old&&!station_diag.applied);
    const auto lifetime_connects=station_diag.connects,lifetime_disconnects=station_diag.disconnects;
    event(WIFI_EVENT_AP_STOP);CHECK(shared.clients==0&&!station_diag.queried&&station_diag.stops==1);
    event(WIFI_EVENT_AP_START);CHECK(shared.clients==0&&station_diag.starts==2);
    CHECK(station_diag.cycle_connects==0&&station_diag.cycle_disconnects==0);
    CHECK(station_diag.connects==lifetime_connects&&station_diag.disconnects==lifetime_disconnects);
    CHECK(station_diag.event==StationDiagnostics::disconnect&&station_diag.event_has_id);
    driver(0);clock_us+=1000000;CHECK(sampleStations()==ESP_OK&&station_diag.native_count==0&&shared.clients==0);
    // Change logging is limited to 1 Hz even if events arrive rapidly; steady samples are 30 s apart.
    const auto before=logs.size();event(WIFI_EVENT_AP_STACONNECTED,0x4455);emitStations();
    event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);emitStations();CHECK(logs.size()==before);
    clock_us+=1000000;emitStations();CHECK(logs.size()==before+1);emitStations();CHECK(logs.size()==before+1);
    emitStations(true);CHECK(logs.size()==before+2); // One final teardown/error record.
    if(max_stations>=3) {
        // Distinguish three nonzero count-history samples from three association events.
        reset();event(WIFI_EVENT_AP_START);clock_us=14254000;
        event(WIFI_EVENT_AP_STACONNECTED,0x4455);driver(1);CHECK(sampleStations()==ESP_OK&&shared.clients==1);
        clock_us=24356000;event(WIFI_EVENT_AP_STACONNECTED,0x4456);
        event(WIFI_EVENT_AP_STACONNECTED,0x4456); // Duplicate is a raw event, not a third station.
        driver(2);CHECK(sampleStations()==ESP_OK&&shared.clients==2&&shared.ui_revision==2);
        const auto events=station_diag.connects;clock_us=31431000;driver(3);
        CHECK(sampleStations()==ESP_OK&&shared.clients==3&&station_diag.native_count==3);
        CHECK(station_diag.connects==events&&station_diag.disconnects==0&&station_diag.event_count==2);
        CHECK(station_diag.event_ms==24356&&station_diag.query_ms==31431&&station_diag.applied);
    }
    station_diag.connects=UINT32_MAX;event(WIFI_EVENT_AP_STACONNECTED,0x4455);CHECK(station_diag.connects==UINT32_MAX);
    reset();CHECK(shared.clients==0&&station_diag.connects==0&&station_diag.disconnects==0&&ws_diag.wifi_disconnects==0);
    printf("PASS actual station callback/query/logging: boot/connect/disconnect/duplicates/delays/native reconciliation/query race/restart/error/redaction/rate; max=%u diagnostics=%zu bytes\n",max_stations,sizeof(StationDiagnostics));
#else
    reset();event(WIFI_EVENT_AP_START);CHECK(queries==0&&shared.clients==0);
    CHECK(sampleStations()==ESP_OK&&shared.clients==0);
    event(WIFI_EVENT_AP_STACONNECTED,0x4455);event(WIFI_EVENT_AP_STACONNECTED,0x4455);CHECK(shared.clients==1);
    driver(1);CHECK(sampleStations()==ESP_OK&&shared.clients==1);
    event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);
    CHECK(shared.clients==0&&ws_diag.wifi_disconnects==2);
    driver(0);CHECK(sampleStations()==ESP_OK&&shared.clients==0);
    driver(1);CHECK(sampleStations()==ESP_OK&&shared.clients==1);
    during_query=[]{event(WIFI_EVENT_AP_STADISCONNECTED,0x4455);};CHECK(sampleStations()==ESP_OK&&shared.clients==0);
    driver(0);CHECK(sampleStations()==ESP_OK&&shared.clients==0);
    event(WIFI_EVENT_AP_STOP);event(WIFI_EVENT_AP_START);CHECK(shared.clients==0);
    const auto calls=queries,reads=clock_reads;
    for(unsigned i=0;i<500;++i){clock_us+=1000000;CHECK(sampleStations()==ESP_OK);}
    CHECK(queries==calls+500&&clock_reads==reads&&logs.empty()); // No tracing, formatting, timers or extra native queries.
    native_rc=257;CHECK(sampleStations()==257);CHECK(logs.empty());
    puts("PASS production station path: identical bounded count/reconciliation/events/errors, retained disconnect counter, zero detailed diagnostic work/logging");
#endif
}
