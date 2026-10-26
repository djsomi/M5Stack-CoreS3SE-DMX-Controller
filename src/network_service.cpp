#include "network_service.h"
#include "hardware_diagnostics.h"
#include "http_policy.h"
#include "web_assets.h"
#include "softap_policy.h"
#include "websocket_service.h"
#include "websocket_wire.h"
#include "websocket_pump.h"
#include "websocket_diagnostics.h"
#include <Preferences.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <esp_netif.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace network_service {
namespace {
constexpr unsigned max_stations = DMX_WIFI_MAX_STATIONS, max_sessions = 6;
static_assert(max_sessions+3<=CONFIG_LWIP_MAX_SOCKETS,"HTTP listen/control socket reservation");
static_assert(max_stations<=CONFIG_LWIP_DHCPS_MAX_STATION_NUM,"AP limit exceeds DHCP lease capacity");
// Provisional fail-closed guardrails, NOT measured safe Wi-Fi budgets.
constexpr size_t minimum_internal = 32768, minimum_dma = 16384;
softap_policy::Credentials credentials{};
uint8_t ap_mac[6]{};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Status shared{};
using WsReason=websocket_diagnostics::Reason;
using WsSource=websocket_diagnostics::Source;
websocket_diagnostics::Counters ws_diag;
struct Sample { Memory memory; unsigned event, clients, requests; } samples[8]{};
unsigned sample_count = 0;
TaskHandle_t worker_task = nullptr;
TaskHandle_t application_owner = nullptr;
httpd_handle_t server = nullptr;
struct Session {
    int fd = -1; stage5::Budget budget;
    bool handled=false, closing=false, websocket=false;
    websocket_bridge::Token token;
    int64_t rx_deadline=0, tx_deadline=0, activity=0, ping=0;
    uint32_t frames=0;int64_t frame_window=0;
    websocket_bridge::WireGuard wire;
    WsReason close_reason=WsReason::none,io_reason=WsReason::none;
    int socket_errno=0;uint32_t io_operation=0;
    uint32_t updates_sent=0;
    uint64_t last_sent_revision=0,attempt_revision=0;
    uint32_t last_send_ms=0,send_us=0,send_max_us=0,queue_start=0,queue_end=0;
} sessions[max_sessions];
// HTTP task only; one serialized formatter, no per-client heap/frame allocation.
char ws_output[websocket_bridge::max_json];
int64_t request_window = 0;
unsigned window_requests = 0;
bool launched = false;
bool sweep_pending = false;
int64_t sweep_queued_at = 0; // mux protected; one outstanding HTTP work item.
softap_policy::Stations stations;
uint32_t station_generation=0;
// Detailed station tracing is absent from normal builds, including its state/work.
#if DMX_HW_DIAGNOSTICS
struct StationDiagnostics {
    enum Event : uint8_t { none,start,connect,disconnect,stop };
    Event event=none,lifecycle=none;
    uint32_t lifecycle_ms=0;
    uint32_t event_ms=0,event_count=0,connects=0,disconnects=0,starts=0,stops=0;
    uint32_t cycle_connects=0,cycle_disconnects=0,query_ms=0,before=0,after=0;
    int native_count=-1,query_rc=0;
    uint16_t event_id=0,ids[max_stations]{};
    bool event_has_id=false,queried=false,applied=false;
} station_diag;
uint32_t station_trace_revision=1,station_trace_logged=0;
int64_t station_trace_at=0;
void stationIncrement(uint32_t& value) {if(value!=UINT32_MAX)++value;}
#endif

// Caller holds mux. Wi-Fi associations are independent of HTTP socket sessions.
void publishStations() {
    if(shared.clients!=stations.count()) {shared.clients=stations.count();++shared.ui_revision;}
}
void stationEvent(void*,esp_event_base_t,int32_t event,void* data) {
    if(event!=WIFI_EVENT_AP_STACONNECTED && event!=WIFI_EVENT_AP_STADISCONNECTED && event!=WIFI_EVENT_AP_STOP && event!=WIFI_EVENT_AP_START) return;
    portENTER_CRITICAL(&mux);
#if DMX_HW_DIAGNOSTICS
    auto& d=station_diag;const auto ms=uint32_t(esp_timer_get_time()/1000);
    d.queried=false;d.applied=false; // Any previous query predates this event.
    if(event==WIFI_EVENT_AP_START) {
        d.lifecycle=StationDiagnostics::start;d.lifecycle_ms=ms;stationIncrement(d.starts);
        d.cycle_connects=d.cycle_disconnects=0;
    } else if(event==WIFI_EVENT_AP_STOP) {d.lifecycle=StationDiagnostics::stop;d.lifecycle_ms=ms;stationIncrement(d.stops);}
    else {
        d.event_ms=ms;d.event_has_id=false;
        const uint8_t* mac=nullptr;
        if(event==WIFI_EVENT_AP_STACONNECTED) {
            d.event=StationDiagnostics::connect;stationIncrement(d.connects);stationIncrement(d.cycle_connects);
            if(data)mac=static_cast<wifi_event_ap_staconnected_t*>(data)->mac;
        } else {
            d.event=StationDiagnostics::disconnect;stationIncrement(d.disconnects);stationIncrement(d.cycle_disconnects);
            if(data)mac=static_cast<wifi_event_ap_stadisconnected_t*>(data)->mac;
        }
        if(mac){d.event_id=uint16_t(unsigned(mac[4])*256+mac[5]);d.event_has_id=true;}
    }
    ++station_trace_revision;
#endif
    if(event==WIFI_EVENT_AP_STOP) stations.clear();
    else if(data && event==WIFI_EVENT_AP_STACONNECTED)
        stations.connect(static_cast<wifi_event_ap_staconnected_t*>(data)->mac);
    else if(data && event==WIFI_EVENT_AP_STADISCONNECTED) stations.disconnect(static_cast<wifi_event_ap_stadisconnected_t*>(data)->mac);
    if(event==WIFI_EVENT_AP_STADISCONNECTED){++ws_diag.wifi_disconnects;ws_diag.last_wifi_ms=uint32_t(esp_timer_get_time()/1000);}
    if(event!=WIFI_EVENT_AP_START){++station_generation;publishStations();}
#if DMX_HW_DIAGNOSTICS
    if(event==WIFI_EVENT_AP_STACONNECTED || event==WIFI_EVENT_AP_STADISCONNECTED)d.event_count=shared.clients;
#endif
    portEXIT_CRITICAL(&mux);
    // No M5, Controller, storage, printing or driver query in the callback.
}

Memory memory() {
    return {static_cast<uint32_t>(esp_timer_get_time()/1000),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))};
}
bool safe(const Memory& m) { return m.internal >= minimum_internal && m.dma >= minimum_dma; }
template<class Function> esp_err_t traced(const char* api,Function function,esp_err_t allowed=ESP_OK) {
    // Before/after for allocation phases; exact return checkpoint for every
    // intermediate API. Keep the temporary evidence buffer small.
    if(!strcmp(api,"esp_netif_init") || !strcmp(api,"esp_wifi_init") ||
       !strcmp(api,"esp_wifi_set_mode") || !strcmp(api,"esp_wifi_start") || !strcmp(api,"httpd_start"))
        wifi_startup_trace::capture(api,0,false,nullptr,0,true);
    const esp_err_t result=function();
    wifi_startup_trace::capture(api,result,result!=ESP_OK && result!=allowed);
    return result;
}
void phase(Phase p, int error=0) {
    if(p==Phase::failed) {
        wifi_startup_trace::capture("phase.FAILED",error,true);
        wifi_startup_trace::release();
    }
    portENTER_CRITICAL(&mux);
    if(shared.phase!=p || shared.error!=error) ++shared.ui_revision;
    shared.phase=p;shared.error=error;shared.max_clients=max_stations;
    portEXIT_CRITICAL(&mux);
}
void record(unsigned event, unsigned clients) {
    const Memory m=memory();
    const auto stack=uxTaskGetStackHighWaterMark(nullptr);
    portENTER_CRITICAL(&mux);
    (void)clients;shared.current=m;shared.worker_stack=stack;
    if(event==1) shared.before=m;
    if(event==2) shared.after=m;
    if(event!=7) { samples[sample_count%8]={m,event,shared.clients,shared.requests}; ++sample_count; }
    portEXIT_CRITICAL(&mux);
}
#if DMX_HW_DIAGNOSTICS
void emitStations(bool force=false) {
    const auto now=esp_timer_get_time();
    StationDiagnostics d;unsigned app;uint32_t revision;
    portENTER_CRITICAL(&mux);d=station_diag;app=shared.clients;revision=station_trace_revision;portEXIT_CRITICAL(&mux);
    if(!force && ((revision==station_trace_logged && now-station_trace_at<30000000) ||
        (station_trace_logged && now-station_trace_at<1000000)))return;
    station_trace_logged=revision;station_trace_at=now;
    char ids[max_stations*5+1]{};size_t used=0;
    if(d.queried && d.query_rc==ESP_OK && d.native_count>=0)
        for(unsigned i=0;i<std::min(unsigned(d.native_count),max_stations);++i)
            used+=size_t(snprintf(ids+used,sizeof(ids)-used,"%s%04X",i?",":"",unsigned(d.ids[i])));
    char last[5]="----";if(d.event_has_id)snprintf(last,sizeof(last),"%04X",unsigned(d.event_id));
    const char* events[]={"none","start","connect","disconnect","stop"};
    // Existing ESP-IDF USB console: <=1 change line/s, 30 s steady sample, final teardown.
    ESP_LOGW("apdiag","AP_STATIONS ms=%u app=%u drv_last=%d rc=%d query=%u use=%u qms=%u gen=%u/%u ev=%s ems=%u eapp=%u id=%s C=%u D=%u starts=%u stops=%u cycle=%u/%u ap=%s ams=%u ids=%s",
        unsigned(now/1000),app,d.native_count,d.query_rc,unsigned(d.queried),unsigned(d.applied),unsigned(d.query_ms),unsigned(d.before),unsigned(d.after),
        events[unsigned(d.event)],unsigned(d.event_ms),unsigned(d.event_count),last,unsigned(d.connects),unsigned(d.disconnects),unsigned(d.starts),unsigned(d.stops),
        unsigned(d.cycle_connects),unsigned(d.cycle_disconnects),events[unsigned(d.lifecycle)],unsigned(d.lifecycle_ms),ids);
}
#endif
esp_err_t sampleStations() {
    wifi_sta_list_t list{};uint32_t generation;
    portENTER_CRITICAL(&mux);generation=station_generation;portEXIT_CRITICAL(&mux);
    const auto rc=esp_wifi_ap_get_sta_list(&list); // Worker only, existing 1 Hz query.
#if DMX_HW_DIAGNOSTICS
    uint16_t ids[max_stations]{};
    if(rc==ESP_OK) {
        for(unsigned i=0;i<std::min(unsigned(list.num),max_stations);++i)
            ids[i]=uint16_t(unsigned(list.sta[i].mac[4])*256+list.sta[i].mac[5]);
        std::sort(ids,ids+std::min(unsigned(list.num),max_stations)); // Diagnostic order only.
    }
#endif
    portENTER_CRITICAL(&mux);
    const bool apply=rc==ESP_OK && unsigned(list.num)<=max_stations && generation==station_generation;
#if DMX_HW_DIAGNOSTICS
    auto& d=station_diag;
    const int count=rc==ESP_OK?int(list.num):-1;
    if(!d.queried || d.native_count!=count || d.query_rc!=rc || d.applied!=apply || memcmp(d.ids,ids,sizeof(ids)))++station_trace_revision;
    d.queried=true;d.query_ms=uint32_t(esp_timer_get_time()/1000);d.query_rc=rc;d.native_count=count;
    d.before=generation;d.after=station_generation;d.applied=apply;memcpy(d.ids,ids,sizeof(ids));
#endif
    // Preserve the tested source of current count and its stale-query guard.
    if(apply) {
        stations.clear();
        for(int i=0;i<list.num;++i)stations.connect(list.sta[i].mac);
        publishStations();
    }
    portEXIT_CRITICAL(&mux);
#if DMX_HW_DIAGNOSTICS
    emitStations();
#endif
    return rc!=ESP_OK?rc:(unsigned(list.num)>max_stations?ESP_FAIL:ESP_OK);
}
Session* session(int fd) {
    for(auto& s:sessions) if(s.fd==fd) return &s;
    return nullptr;
}
void recordClosure(Session& s,WsReason reason,WsSource source,int rc=0) {
    if(s.close_reason!=WsReason::none)return;
    s.close_reason=reason;
    if(!s.websocket)return;
    const auto info=websocket_service::clientInfo(s.token);
    websocket_diagnostics::Closure c;
    c.ms=uint32_t(esp_timer_get_time()/1000);c.generation=s.token.generation;c.fd=s.fd;
    c.reason=reason;c.source=source;c.rc=rc;c.socket_errno=s.socket_errno;
    c.updates_sent=s.updates_sent;c.last_sent_revision=s.last_sent_revision;c.attempt_revision=s.attempt_revision;
    c.queued=info.queued;c.outstanding=info.outstanding;c.last_enqueued_revision=info.last_enqueued_revision;
    c.send_us=s.send_us;c.io_operation=s.io_operation;
    const auto first=websocket_service::metrics().first;
    if(first.present&&first.token.generation==s.token.generation&&first.token.slot==s.token.slot&&info.stop==first.stop) {
        c.queued=first.queued;c.outstanding=first.outstanding;
        if(reason==WsReason::bridge_queue_full)c.attempt_revision=first.rejected_revision;
    }
    portENTER_CRITICAL(&mux);
    ws_diag.last=c;
    if(reason==WsReason::native_unattributed)++ws_diag.unattributed_closes;
    if(reason!=WsReason::peer_close&&!ws_diag.first_present){ws_diag.first=c;ws_diag.first_present=true;}
    portEXIT_CRITICAL(&mux);
}
int ioFailure(Session& s,WsReason reason,int error) {
    if(s.io_reason==WsReason::none){s.io_reason=reason;s.socket_errno=error;}
    return HTTPD_SOCK_ERR_FAIL;
}
int io(httpd_handle_t, int fd, char* data, size_t length, int flags, bool writing) {
    Session* s=session(fd);
    if(s && s->websocket) {
        // IDF's WS sender only checks for negative results, NOT short writes.
        // Likewise extended length/mask reads must be exact. Complete each
        // native call within one frame deadline or fail/close the connection.
        auto& deadline=writing?s->tx_deadline:s->rx_deadline;
        const bool first=!deadline;
        if(first)deadline=esp_timer_get_time()+100000;
        if(length>(writing?websocket_bridge::max_json:websocket_bridge::max_message))return ioFailure(*s,WsReason::wire_invalid,0);
        size_t done=0;
        while(done<length) {
            // lwIP truncates timeval to milliseconds; <1 ms becomes zero
            // (unlimited wait). Retire this exhausted budget instead.
            const auto left=deadline-esp_timer_get_time();if(left<1000){s->io_operation=1;return ioFailure(*s,WsReason::io_failure,ETIMEDOUT);}
            timeval timeout{0,static_cast<long>(left)};
            s->io_operation=2;
            if(setsockopt(fd,SOL_SOCKET,writing?SO_SNDTIMEO:SO_RCVTIMEO,&timeout,sizeof(timeout)))return ioFailure(*s,WsReason::io_failure,errno);
            s->io_operation=writing?3:4;
            const int n=writing?send(fd,data+done,length-done,flags):recv(fd,data+done,length-done,flags);
            if(n<=0)return ioFailure(*s,WsReason::io_failure,n<0?errno:0);
            done+=size_t(n);
        }
        if(!writing&&!s->wire.wsBytes(data,done))return ioFailure(*s,WsReason::wire_invalid,0);
        return int(done);
    }
    if(!s || !s->budget.allow(esp_timer_get_time(),0,writing)) return HTTPD_SOCK_ERR_FAIL;
    const size_t used=writing?s->budget.sent:s->budget.received;
    const size_t limit=writing?stage5::max_tx_bytes:stage5::max_rx_bytes;
    const size_t amount=std::min(size_t(256),std::min(length,limit-used));
    if(!amount) return HTTPD_SOCK_ERR_FAIL;
    const int64_t remaining=stage5::session_us-(esp_timer_get_time()-s->budget.opened);
    if(remaining<=0) return HTTPD_SOCK_ERR_FAIL;
    const int64_t wait=std::min(int64_t(1000000),remaining);
    timeval timeout{static_cast<long>(wait/1000000),static_cast<long>(wait%1000000)};
    if(setsockopt(fd,SOL_SOCKET,writing?SO_SNDTIMEO:SO_RCVTIMEO,&timeout,sizeof(timeout))) return HTTPD_SOCK_ERR_FAIL;
    const int n=writing?send(fd,data,amount,flags):recv(fd,data,amount,flags);
    if(n>0) {
        if(!writing)s->wire.httpBytes(data,size_t(n));
        if(!s->budget.allow(esp_timer_get_time(),static_cast<size_t>(n),writing)) return HTTPD_SOCK_ERR_FAIL;
        return n;
    }
    if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return HTTPD_SOCK_ERR_TIMEOUT;
    return !writing && n==0 ? 0 : HTTPD_SOCK_ERR_FAIL;
}
int receive(httpd_handle_t h,int fd,char* p,size_t n,int f) { return io(h,fd,p,n,f,false); }
int transmit(httpd_handle_t h,int fd,const char* p,size_t n,int f) { return io(h,fd,const_cast<char*>(p),n,f,true); }
esp_err_t opened(httpd_handle_t h,int fd) {
    for(auto& s:sessions) if(s.fd==-1) {
        s=Session{};s.fd=fd;s.budget.opened=esp_timer_get_time();
        if(httpd_sess_set_recv_override(h,fd,receive)!=ESP_OK || httpd_sess_set_send_override(h,fd,transmit)!=ESP_OK) {
            s.fd=-1; return ESP_FAIL;
        }
        portENTER_CRITICAL(&mux);++ws_diag.sockets_opened;portEXIT_CRITICAL(&mux);
        return ESP_OK;
    }
    portENTER_CRITICAL(&mux);++ws_diag.open_rejected;portEXIT_CRITICAL(&mux);
    return ESP_FAIL;
}
void closed(httpd_handle_t,int fd) {
    if(auto* s=session(fd)){
        if(s->websocket) {
            portENTER_CRITICAL(&mux);const bool stopping=ws_diag.stopping;const int rc=ws_diag.server_stop_rc;portEXIT_CRITICAL(&mux);
            recordClosure(*s,stopping?WsReason::server_stop:(s->io_reason!=WsReason::none?s->io_reason:WsReason::native_unattributed),WsSource::native_callback,rc);
            websocket_service::close(s->token);
        }
        portENTER_CRITICAL(&mux);++ws_diag.sockets_closed;if(s->websocket)++ws_diag.ws_closed;portEXIT_CRITICAL(&mux);
        *s=Session{};
    }
    close(fd);
}
void closeSession(Session& s,WsReason reason,WsSource source,int rc=0) {
    if(s.closing)return;
    recordClosure(s,reason,source,rc);
    if(s.websocket)websocket_service::close(s.token);
    httpd_sess_update_lru_counter(server,s.fd);
    if(httpd_sess_trigger_close(server,s.fd)==ESP_OK)s.closing=true;
    else shutdown(s.fd,SHUT_RDWR);
}
bool wsSend(Session& s,httpd_ws_type_t type,uint8_t* payload,size_t length) {
    httpd_ws_frame_t frame{};frame.type=type;frame.payload=payload;frame.len=length;
    s.queue_start=websocket_service::clientInfo(s.token).queued;
    const auto send_start=esp_timer_get_time();
    s.io_reason=WsReason::none;s.socket_errno=0;s.io_operation=0;s.tx_deadline=esp_timer_get_time()+100000;
    const auto rc=httpd_ws_send_frame_async(server,s.fd,&frame);
    s.send_us=uint32_t(esp_timer_get_time()-send_start);
    s.send_max_us=std::max(s.send_max_us,s.send_us);
    s.queue_end=websocket_service::clientInfo(s.token).queued;
    if(rc==ESP_OK)s.last_send_ms=uint32_t(esp_timer_get_time()/1000);
    s.tx_deadline=0;
    portENTER_CRITICAL(&mux);ws_diag.last_send_rc=rc;if(rc!=ESP_OK)++ws_diag.send_failures;
    ws_diag.send_max_us=std::max(ws_diag.send_max_us,s.send_us);
    if(rc!=ESP_OK&&s.socket_errno==ETIMEDOUT)++ws_diag.send_timeouts;
    if(rc==ESP_OK&&type==HTTPD_WS_TYPE_CLOSE)++ws_diag.close_frames_sent;
    portEXIT_CRITICAL(&mux);
    if(rc!=ESP_OK){recordClosure(s,WsReason::send_failed,WsSource::send,rc);websocket_service::sendFailed(s.token);closeSession(s,WsReason::send_failed,WsSource::send,rc);return false;}
    return true;
}
void sweep(void*) {
    const auto now=esp_timer_get_time();
    portENTER_CRITICAL(&mux);++ws_diag.work_started;
    ws_diag.work_delay_max_us=std::max(ws_diag.work_delay_max_us,uint32_t(now-sweep_queued_at));
    portEXIT_CRITICAL(&mux);
    bool busy=false;
    for(auto& s:sessions) if(s.fd>=0 && !s.closing) {
        if(s.websocket) {
            const auto info=websocket_service::clientInfo(s.token);
            if(!info.active) {
                const auto reason=info.stop==websocket_bridge::Stop::queue_full?WsReason::bridge_queue_full:
                    (info.stop==websocket_bridge::Stop::rate_limit?WsReason::bridge_rate:WsReason::bridge_gone);
                closeSession(s,reason,WsSource::pump);continue;
            }
            const auto fd_info=httpd_ws_get_fd_info(server,s.fd);
            if(fd_info!=HTTPD_WS_CLIENT_WEBSOCKET){closeSession(s,WsReason::socket_invalid,WsSource::pump,int(fd_info));continue;}
            if(now-s.activity>=60000000){closeSession(s,WsReason::idle,WsSource::pump);continue;}
            if(now-s.ping>=20000000){s.ping=now;wsSend(s,HTTPD_WS_TYPE_PING,nullptr,0);}
            continue;
        }
        if(now-s.budget.opened<stage5::session_us)continue;
        // IDF 4.4.7 skips queued closes with zero LRU (new, never-read socket).
        // Refresh it in the HTTP task before queuing this same live session.
        httpd_sess_update_lru_counter(server,s.fd);
        if(httpd_sess_trigger_close(server,s.fd)==ESP_OK) s.closing=true;
        else shutdown(s.fd,SHUT_RDWR); // Wake select; native session cleanup owns close.
    }
    static unsigned first=0;
    websocket_bridge::drain(max_sessions,first,[&busy](unsigned slot) {
        auto& s=sessions[slot];
        if(s.fd<0||s.closing||!s.websocket||!websocket_service::alive(s.token))return false;
        websocket_bridge::Event event;if(!websocket_service::take(s.token,event))return false;
        busy=true;
        s.attempt_revision=event.revision;
        const size_t length=websocket_bridge::encode(event,ws_output,sizeof(ws_output));
        if(!length){recordClosure(s,WsReason::encode_failed,WsSource::pump);websocket_service::sendFailed(s.token);closeSession(s,WsReason::encode_failed,WsSource::pump);return true;}
        if(wsSend(s,HTTPD_WS_TYPE_TEXT,reinterpret_cast<uint8_t*>(ws_output),length)&&
           (event.kind==websocket_bridge::Kind::channel||event.kind==websocket_bridge::Kind::universe||event.kind==websocket_bridge::Kind::mode)) {
            ++s.updates_sent;s.last_sent_revision=event.revision;
            portENTER_CRITICAL(&mux);++ws_diag.updates_sent;portEXIT_CRITICAL(&mux);
        }
        return true;
    },[]{return esp_timer_get_time();});
    const auto duration=uint32_t(esp_timer_get_time()-now);
    portENTER_CRITICAL(&mux);sweep_pending=false;++ws_diag.work_completed;
    if(busy)++ws_diag.work_busy;
    ws_diag.work_last_us=duration;ws_diag.work_max_us=std::max(ws_diag.work_max_us,duration);portEXIT_CRITICAL(&mux);
}
esp_err_t finish(httpd_req_t* r,esp_err_t result) {
    Session* s=session(httpd_req_to_sockfd(r));
    if(result!=ESP_OK || !s) return ESP_FAIL;
    s->closing=true;
    return httpd_sess_trigger_close(r->handle,s->fd); // Supported server-task close, no success-as-error log.
}
bool header(httpd_req_t* r,const char* key,char* value,size_t capacity,bool required=false) {
    const size_t length=httpd_req_get_hdr_value_len(r,key);
    if(!length) {value[0]=0;return !required;}
    return length<capacity && httpd_req_get_hdr_value_str(r,key,value,capacity)==ESP_OK;
}
esp_err_t handlerFailure(Session& s,WsReason reason,int rc=ESP_FAIL) {
    if(!s.websocket){portENTER_CRITICAL(&mux);++ws_diag.upgrade_rejected;portEXIT_CRITICAL(&mux);}
    recordClosure(s,reason,s.io_reason==WsReason::none?WsSource::handler:WsSource::io,rc);
    return ESP_FAIL; // Native server owns cleanup; do not queue a second close.
}
esp_err_t websocket(httpd_req_t* r) {
    auto* s=session(httpd_req_to_sockfd(r));if(!s||s->closing)return ESP_FAIL;
    if(r->method==HTTP_GET) {
        char host[65],origin[65];
        const auto now=esp_timer_get_time();
        if(now-request_window>=1000000){request_window=now;window_requests=0;}
        // IDF emits 101 BEFORE this callback. Reject before registering any
        // bridge client or processing data; returning failure closes the socket.
        if(s->handled||s->wire.trailing()||strcmp(r->uri,"/ws")||r->content_len||++window_requests>12||
           httpd_ws_get_fd_info(r->handle,s->fd)!=HTTPD_WS_CLIENT_WEBSOCKET||
           !header(r,"Host",host,sizeof(host),true)||!stage5::allowedHost(host)||
           !header(r,"Origin",origin,sizeof(origin))||!stage5::allowedOrigin(origin))return handlerFailure(*s,WsReason::upgrade_rejected);
        s->token=websocket_service::open(uint64_t(now/1000));
        if(!s->token.generation)return handlerFailure(*s,WsReason::upgrade_rejected);
        s->handled=s->websocket=true;s->activity=s->ping=now;
        portENTER_CRITICAL(&mux);++ws_diag.ws_opened;portEXIT_CRITICAL(&mux);
        return ESP_OK;
    }
    if(!s->websocket||!websocket_service::alive(s->token))return handlerFailure(*s,WsReason::bridge_gone);
    const auto now=esp_timer_get_time();
    if(now-s->frame_window>=1000000){s->frame_window=now;s->frames=0;}
    if(++s->frames>24)return handlerFailure(*s,WsReason::frame_rate); // Includes control frames, bounded CPU.
    httpd_ws_frame_t frame{};
    const auto received=httpd_ws_recv_frame(r,&frame,0);
    if(received!=ESP_OK)return handlerFailure(*s,s->io_reason!=WsReason::none?s->io_reason:WsReason::receive_failed,received);
    if(!websocket_bridge::validFrame(unsigned(frame.type),frame.final,frame.len))return handlerFailure(*s,WsReason::frame_invalid);
    uint8_t payload[websocket_bridge::max_message];
    frame.payload=payload;
    if(frame.len){const auto payload_rc=httpd_ws_recv_frame(r,&frame,sizeof(payload));
        if(payload_rc!=ESP_OK)return handlerFailure(*s,s->io_reason!=WsReason::none?s->io_reason:WsReason::receive_failed,payload_rc);}
    s->rx_deadline=0;s->activity=now;
    if(frame.type==HTTPD_WS_TYPE_CLOSE){if(wsSend(*s,HTTPD_WS_TYPE_CLOSE,nullptr,0))recordClosure(*s,WsReason::peer_close,WsSource::handler);return ESP_FAIL;}
    if(frame.type==HTTPD_WS_TYPE_PING)return wsSend(*s,HTTPD_WS_TYPE_PONG,payload,frame.len)?ESP_OK:ESP_FAIL;
    if(frame.type==HTTPD_WS_TYPE_TEXT)websocket_service::receive(s->token,reinterpret_cast<char*>(payload),frame.len,uint64_t(now/1000));
    return ESP_OK;
}
size_t diagnosticJson(char* output,size_t capacity) {
    websocket_diagnostics::Counters counters;bool pending;
    portENTER_CRITICAL(&mux);counters=ws_diag;pending=sweep_pending;portEXIT_CRITICAL(&mux);
    websocket_diagnostics::SessionInfo infos[max_sessions];
    for(unsigned i=0;i<max_sessions;++i) {
        const auto& s=sessions[i];auto& info=infos[i];info.fd=s.fd;info.websocket=s.websocket;info.closing=s.closing;
        info.generation=s.token.generation;info.opened_ms=uint32_t(s.budget.opened/1000);info.activity_ms=uint32_t(s.activity/1000);
        info.updates_sent=s.updates_sent;info.last_sent_revision=s.last_sent_revision;
        info.last_send_ms=s.last_send_ms;info.send_us=s.send_us;info.send_max_us=s.send_max_us;
        info.queue_start=s.queue_start;info.queue_end=s.queue_end;
        if(s.websocket)info.bridge=websocket_service::clientInfo(s.token);
    }
    return websocket_diagnostics::encode(counters,websocket_service::metrics(),infos,max_sessions,pending,uint32_t(esp_timer_get_time()/1000),output,capacity);
}
void emitFirstFailure() {
    websocket_diagnostics::Closure c;bool emit=false;
    portENTER_CRITICAL(&mux);
    if(ws_diag.first_present&&!ws_diag.first_logged){c=ws_diag.first;ws_diag.first_logged=true;emit=true;}
    portEXIT_CRITICAL(&mux);
    if(emit)ESP_LOGW("wsdiag","WS_FIRST_FAILURE ms=%u gen=%u fd=%d reason=%s source=%s rc=%d errno=%d last_ok_rev=%llu trigger_rev=%llu updates_sent=%u queued=%u outstanding=%u",
        unsigned(c.ms),unsigned(c.generation),c.fd,websocket_diagnostics::name(c.reason),websocket_diagnostics::name(c.source),c.rc,c.socket_errno,
        (unsigned long long)c.last_sent_revision,(unsigned long long)c.attempt_revision,unsigned(c.updates_sent),unsigned(c.queued),unsigned(c.outstanding));
}
void security(httpd_req_t* r,const char* cache="no-store") {
    httpd_resp_set_hdr(r,"Connection","close");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; connect-src 'self' ws://192.168.4.1; frame-ancestors 'none'; base-uri 'none'");
    httpd_resp_set_hdr(r,"Referrer-Policy","no-referrer");
    httpd_resp_set_hdr(r,"Cache-Control",cache);
}
esp_err_t reject(httpd_req_t* r,const char* code) {
    portENTER_CRITICAL(&mux);++shared.rejected;
    const unsigned status=unsigned(code[0]-'0')*100+unsigned(code[1]-'0')*10+unsigned(code[2]-'0');
    websocket_diagnostics::rejection(ws_diag,uint32_t(esp_timer_get_time()/1000),status,r->uri);
    portEXIT_CRITICAL(&mux);
    security(r);httpd_resp_set_status(r,code);httpd_resp_set_type(r,"text/plain; charset=utf-8");
    return finish(r,httpd_resp_send(r,code,strlen(code)));
}
esp_err_t serve(httpd_req_t* r) {
    auto* connection=session(httpd_req_to_sockfd(r));
    if(!connection || connection->handled) return ESP_FAIL;
    connection->handled=true;
    const auto hwm=uxTaskGetStackHighWaterMark(nullptr);
    portENTER_CRITICAL(&mux);++shared.requests;shared.http_stack=hwm;portEXIT_CRITICAL(&mux);
    char host[65],origin[65],encoding[stage5::max_header_value+1];
    if(!header(r,"Host",host,sizeof(host),true) || !stage5::allowedHost(host) ||
       !header(r,"Origin",origin,sizeof(origin)) || !stage5::allowedOrigin(origin)) return reject(r,"403 Forbidden");
    if(r->content_len) return reject(r,"413 Payload Too Large");
    const auto now=esp_timer_get_time();
    if(now-request_window>=1000000) {request_window=now;window_requests=0;}
    if(++window_requests>12) return reject(r,"429 Too Many Requests");
    if(!strcmp(r->uri,"/status")||!strcmp(r->uri,"/ws-diagnostics")) {
        security(r);
        const auto s=status();
        static char json[4096]; // HTTP task only; keep the existing 8 KiB task stack.
        if(!strcmp(r->uri,"/ws-diagnostics")) {
            const size_t length=diagnosticJson(json,sizeof(json));
            if(!length)return reject(r,"503 Service Unavailable");
            httpd_resp_set_type(r,"application/json");return finish(r,httpd_resp_send(r,json,length));
        }
        int n=snprintf(json,sizeof(json),
            "{\"stage\":5,\"phase\":\"%s\",\"ip\":\"%s\",\"stations\":%lu,\"max_stations\":%lu,\"dhcp_router_option\":%s,\"requests\":%lu,\"rejected\":%lu,\"max_owner_gap_us\":%lu,\"tx_errors\":%lu,\"tx_stopped\":%s,\"worker_stack_hwm\":%lu,\"http_stack_hwm\":%lu,\"samples\":[",
            phaseName(s.phase),s.ip,(unsigned long)s.clients,(unsigned long)s.max_clients,s.router_option_disabled?"false":"true",
            (unsigned long)s.requests,(unsigned long)s.rejected,
            (unsigned long)s.max_loop_gap_us,(unsigned long)s.tx_errors,s.tx_stopped?"true":"false",
            (unsigned long)s.worker_stack,(unsigned long)s.http_stack);
        // Baseline/after-AP samples persist even after the event ring wraps.
        const Memory fixed[]={s.before,s.after,s.current};
        for(unsigned i=0;i<3;++i) {
            const auto& m=fixed[i];
            n+=snprintf(json+n,sizeof(json)-n,
                "%s{\"event\":%u,\"ms\":%lu,\"internal\":%lu,\"minimum\":%lu,\"largest\":%lu,\"dma\":%lu,\"psram\":%lu}",
                i?",":"",90+i,(unsigned long)m.at_ms,(unsigned long)m.internal,(unsigned long)m.minimum,
                (unsigned long)m.largest,(unsigned long)m.dma,(unsigned long)m.psram);
        }
        unsigned count;
        portENTER_CRITICAL(&mux);count=sample_count;portEXIT_CRITICAL(&mux);
        const unsigned begin=count>8?count-8:0;
        for(unsigned i=begin;i<count;++i) {
            Sample sample;
            portENTER_CRITICAL(&mux);sample=samples[i%8];portEXIT_CRITICAL(&mux);
            const auto& m=sample.memory;
            if(n<0 || size_t(n)>=sizeof(json)) return reject(r,"503 Service Unavailable");
            n+=snprintf(json+n,sizeof(json)-n,
                "%s{\"event\":%u,\"ms\":%lu,\"clients\":%u,\"requests\":%u,\"internal\":%lu,\"minimum\":%lu,\"largest\":%lu,\"dma\":%lu,\"psram\":%lu}",
                ",",sample.event,(unsigned long)m.at_ms,sample.clients,sample.requests,
                (unsigned long)m.internal,(unsigned long)m.minimum,(unsigned long)m.largest,(unsigned long)m.dma,(unsigned long)m.psram);
        }
        if(n<0 || size_t(n)+3>=sizeof(json)) return reject(r,"503 Service Unavailable");
        const auto w=websocket_service::metrics();
        n+=snprintf(json+n,sizeof(json)-n,
            "],\"websocket\":{\"clients\":%lu,\"max_clients\":4,\"http_sockets\":6,\"inbox\":%lu,\"inbox_high\":%lu,\"outbox_high\":%lu,\"accepted\":%lu,\"rejected\":%lu,\"applied\":%lu,\"broadcasts\":%lu,\"send_failures\":%lu,\"slow_closes\":%lu,\"static_bridge_bytes\":%u},\"dma_free\":%u}",
            (unsigned long)w.clients,(unsigned long)w.inbox,(unsigned long)w.inbox_high,(unsigned long)w.outbox_high,
            (unsigned long)w.accepted,(unsigned long)w.rejected,(unsigned long)w.applied,(unsigned long)w.broadcasts,
            (unsigned long)w.send_failures,(unsigned long)w.slow_closes,unsigned(sizeof(websocket_bridge::Bridge)),
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)));
        if(n<0||size_t(n)>=sizeof(json))return reject(r,"503 Service Unavailable");
        httpd_resp_set_type(r,"application/json");return finish(r,httpd_resp_send(r,json,n));
    }
    if(!header(r,"Accept-Encoding",encoding,sizeof(encoding))) return reject(r,"431 Request Header Fields Too Large");
    const auto response=stage5::assetResponse(r->uri,encoding);
    if(response.status==404) return reject(r,"404 Not Found");
    if(response.status==406) return reject(r,"406 Not Acceptable");
    if(response.status!=200) return reject(r,"503 Service Unavailable");
    const auto* asset=response.asset;
    security(r,response.cache);
    httpd_resp_set_type(r,asset->content_type);
    httpd_resp_set_hdr(r,"Content-Encoding","gzip");
    httpd_resp_set_hdr(r,"Vary","Accept-Encoding");
    // Library emits exact Content-Length; fixed const flash bytes, no inflation/copy.
    return finish(r,httpd_resp_send(r,reinterpret_cast<const char*>(asset->data),asset->size));
}

void worker(void*) {
    esp_netif_t* netif=nullptr;bool initialized=false,started=false;
    esp_event_handler_instance_t station_handler=nullptr;
    esp_err_t error=ESP_OK;
    phase(Phase::starting);
    auto check=[&](esp_err_t e){if(e!=ESP_OK) error=e;return e==ESP_OK;};
    do {
        if(!safe(memory())) {wifi_startup_trace::capture("guard.worker.entry",ESP_ERR_NO_MEM,true);error=ESP_ERR_NO_MEM;break;}
        if(!check(traced("esp_netif_init",[]{return esp_netif_init();}))) break;
        const auto event_error=traced("esp_event_loop_create_default",[]{return esp_event_loop_create_default();},ESP_ERR_INVALID_STATE);
        if(event_error!=ESP_OK && event_error!=ESP_ERR_INVALID_STATE) {error=event_error;break;}
        esp_netif_config_t netcfg=ESP_NETIF_DEFAULT_WIFI_AP();
        wifi_startup_trace::capture("esp_netif_new",0,false,nullptr,0,true);
        netif=esp_netif_new(&netcfg);
        wifi_startup_trace::capture("esp_netif_new",netif?ESP_OK:ESP_ERR_NO_MEM,!netif);
        if(!netif) {error=ESP_ERR_NO_MEM;break;}
        if(!check(traced("esp_netif_attach_wifi_ap",[&]{return esp_netif_attach_wifi_ap(netif);}))) break;
        if(!check(traced("esp_wifi_set_default_wifi_ap_handlers",[]{return esp_wifi_set_default_wifi_ap_handlers();}))) break;
        wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();cfg.nvs_enable=0;
        wifi_startup_trace::capture("cfg.srx_drx_stx_cache",0,false,nullptr,uint32_t(cfg.static_rx_buf_num)|(uint32_t(cfg.dynamic_rx_buf_num)<<8)|(uint32_t(cfg.static_tx_buf_num)<<16)|(uint32_t(cfg.cache_tx_buf_num)<<24));
        wifi_startup_trace::capture("cfg.txType_dtx_mgmt_nvs",0,false,nullptr,uint32_t(cfg.tx_buf_type)|(uint32_t(cfg.dynamic_tx_buf_num)<<8)|(uint32_t(cfg.mgmt_sbuf_num)<<16)|(uint32_t(cfg.nvs_enable)<<24));
        wifi_startup_trace::capture("cfg.ampduRx_ba_mgmtType_count",0,false,nullptr,uint32_t(cfg.ampdu_rx_enable)|(uint32_t(cfg.rx_ba_win)<<8)|(uint32_t(cfg.rx_mgmt_buf_type)<<16)|(uint32_t(cfg.rx_mgmt_buf_num)<<24));
        if(!check(traced("esp_wifi_init",[&]{return esp_wifi_init(&cfg);}))) break;initialized=true;
        if(!check(traced("esp_wifi_set_storage",[]{return esp_wifi_set_storage(WIFI_STORAGE_RAM);}))) break;
        if(!check(traced("esp_wifi_set_mode",[]{return esp_wifi_set_mode(WIFI_MODE_AP);}))) break;
        uint8_t actual_mac[6];
        if(!check(traced("esp_wifi_get_mac.AP",[&]{return esp_wifi_get_mac(WIFI_IF_AP,actual_mac);})) || memcmp(actual_mac,ap_mac,6)) {
            wifi_startup_trace::capture("verify.AP_MAC",ESP_FAIL,true);error=ESP_FAIL;break;
        }
        wifi_config_t ap{};
        memcpy(ap.ap.ssid,credentials.ssid,strlen(credentials.ssid));
        ap.ap.ssid_len=strlen(credentials.ssid);
        memcpy(ap.ap.password,credentials.pass,strlen(credentials.pass));
        ap.ap.channel=6;ap.ap.max_connection=max_stations;ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
        ap.ap.pairwise_cipher=WIFI_CIPHER_TYPE_CCMP;
        // Configure protected AP before start; there is never a temporary open AP.
        if(!check(traced("esp_wifi_set_config",[&]{return esp_wifi_set_config(WIFI_IF_AP,&ap);}))) break;
        const auto stop=traced("esp_netif_dhcps_stop",[&]{return esp_netif_dhcps_stop(netif);},ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED);
        if(stop!=ESP_OK && stop!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {error=stop;break;}
        // Set while DHCP is STOPPED, before AP-start handlers can distribute leases.
        uint8_t offer_router=0;
        if(!check(traced("dhcps.router.SET",[&]{return esp_netif_dhcps_option(netif,ESP_NETIF_OP_SET,
            ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,&offer_router,sizeof(offer_router));}))) break;
        esp_netif_ip_info_t ip{};
        IP4_ADDR(&ip.ip,192,168,4,1);IP4_ADDR(&ip.gw,192,168,4,1);IP4_ADDR(&ip.netmask,255,255,255,0);
        if(!check(traced("esp_netif_set_ip_info",[&]{return esp_netif_set_ip_info(netif,&ip);}))) break;
        if(!check(traced("station.events.register",[&]{return esp_event_handler_instance_register(
            WIFI_EVENT,ESP_EVENT_ANY_ID,stationEvent,nullptr,&station_handler);}))) break;
        if(!check(traced("esp_wifi_start",[]{return esp_wifi_start();}))) break;started=true;
        // Default AP event handlers bring the interface up asynchronously.
        unsigned wait=0;
        while(!esp_netif_is_netif_up(netif) && wait++<50) vTaskDelay(pdMS_TO_TICKS(50));
        wifi_startup_trace::capture("netif.up.wait",esp_netif_is_netif_up(netif)?ESP_OK:ESP_ERR_TIMEOUT,!esp_netif_is_netif_up(netif));
        if(!esp_netif_is_netif_up(netif)) {error=ESP_ERR_TIMEOUT;break;}
        esp_netif_dhcp_status_t dhcp;
        if(!check(traced("esp_netif_dhcps_get_status",[&]{return esp_netif_dhcps_get_status(netif,&dhcp);}))) break;
        if(dhcp!=ESP_NETIF_DHCP_STARTED && !check(traced("esp_netif_dhcps_start",[&]{return esp_netif_dhcps_start(netif);}))) break;
        if(!check(traced("esp_netif_dhcps_get_status.verify",[&]{return esp_netif_dhcps_get_status(netif,&dhcp);})) || dhcp!=ESP_NETIF_DHCP_STARTED) {wifi_startup_trace::capture("verify.dhcp.started",ESP_FAIL,true);error=ESP_FAIL;break;}
        // IDF 4.4.7 GET rejects STOPPED servers: read back only after start.
        uint8_t router_readback=1;
        if(!check(traced("dhcps.router.GET",[&]{return esp_netif_dhcps_option(netif,ESP_NETIF_OP_GET,
            ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,&router_readback,sizeof(router_readback));})) || router_readback!=0) {
            wifi_startup_trace::capture("verify.router.disabled",ESP_FAIL,true);error=ESP_FAIL;break;
        }
        portENTER_CRITICAL(&mux);shared.router_option_disabled=true;portEXIT_CRITICAL(&mux);
        esp_netif_ip_info_t actual{};wifi_config_t actual_ap{};
        if(!check(traced("esp_netif_get_ip_info",[&]{return esp_netif_get_ip_info(netif,&actual);})) || actual.ip.addr!=ip.ip.addr ||
           !check(traced("esp_wifi_get_config",[&]{return esp_wifi_get_config(WIFI_IF_AP,&actual_ap);})) ||
           actual_ap.ap.authmode!=WIFI_AUTH_WPA2_PSK || actual_ap.ap.max_connection!=max_stations) {wifi_startup_trace::capture("verify.ip_auth",ESP_FAIL,true);error=ESP_FAIL;break;}
        portENTER_CRITICAL(&mux);memcpy(shared.ip,"192.168.4.1",12);portEXIT_CRITICAL(&mux);
        record(2,0);
        if(!safe(memory())) {wifi_startup_trace::capture("guard.after_ap",ESP_ERR_NO_MEM,true);error=ESP_ERR_NO_MEM;break;}
        httpd_config_t http=HTTPD_DEFAULT_CONFIG();
        http.core_id=0;http.task_priority=3;http.stack_size=8192;http.max_open_sockets=max_sessions;
        http.max_uri_handlers=2;http.max_resp_headers=8;http.backlog_conn=2;
        http.recv_wait_timeout=1;http.send_wait_timeout=1;http.lru_purge_enable=false;
        http.open_fn=opened;http.close_fn=closed;http.uri_match_fn=httpd_uri_match_wildcard;
        wifi_startup_trace::capture("http.stack_sockets",0,false,nullptr,uint32_t(http.stack_size)|(uint32_t(http.max_open_sockets)<<16));
        if(!check(traced("httpd_start",[&]{return httpd_start(&server,&http);}))) break;
        httpd_uri_t ws{};ws.uri="/ws";ws.method=HTTP_GET;ws.handler=websocket;
        ws.is_websocket=true;ws.handle_ws_control_frames=true;
        if(!check(httpd_register_uri_handler(server,&ws)))break;
        httpd_uri_t route{};route.uri="/*";route.method=HTTP_GET;route.handler=serve;
        if(!check(traced("httpd_register_uri_handler",[&]{return httpd_register_uri_handler(server,&route);}))) break;
        if(!safe(memory())) {wifi_startup_trace::capture("guard.after_http",ESP_ERR_NO_MEM,true);error=ESP_ERR_NO_MEM;break;}
        phase(Phase::ready);record(3,0);
        wifi_startup_trace::capture("startup.READY");wifi_startup_trace::release();
        uint32_t previous_requests=0;unsigned previous_clients=0;int64_t maintenance=0;
        for(;;) {
            vTaskDelay(pdMS_TO_TICKS(20)); // Bounded HTTP-task pump; never queues work from the Controller task.
            bool queue_sweep=false;
            portENTER_CRITICAL(&mux);
            if(!sweep_pending) {sweep_pending=true;sweep_queued_at=esp_timer_get_time();queue_sweep=true;}
            portEXIT_CRITICAL(&mux);
            if(queue_sweep) {
                const auto rc=httpd_queue_work(server,sweep,nullptr);
                portENTER_CRITICAL(&mux);ws_diag.last_work_rc=rc;
                if(rc==ESP_OK)++ws_diag.work_queued;else{++ws_diag.work_failed;sweep_pending=false;}
                portEXIT_CRITICAL(&mux);
                if(rc!=ESP_OK){error=rc;break;}
            }
            if(esp_timer_get_time()-maintenance<1000000)continue;
            maintenance=esp_timer_get_time(); // Station reconciliation and heap guards remain 1 Hz.
            emitFirstFailure(); // Worker only, once; never prints from Controller/ISR/callback.
            if(!check(sampleStations())) {wifi_startup_trace::capture("esp_wifi_ap_get_sta_list",error,true);break;}
            auto s=status();
            record(s.clients!=previous_clients?(s.clients?4:5):(s.requests!=previous_requests?6:7),s.clients);
            previous_clients=s.clients;previous_requests=s.requests;
            if(!safe(memory())) {wifi_startup_trace::capture("guard.runtime.heap",ESP_ERR_NO_MEM,true);error=ESP_ERR_NO_MEM;break;}
            if(s.tx_stopped || s.tx_errors) {wifi_startup_trace::capture("guard.transport",ESP_ERR_INVALID_STATE,true);error=ESP_ERR_INVALID_STATE;break;}
            if((s.http_stack && s.http_stack<1024) || s.worker_stack<1024) {wifi_startup_trace::capture("guard.stack",ESP_ERR_NO_MEM,true);error=ESP_ERR_NO_MEM;break;}
        }
    } while(false);
    // Only worker may block on teardown. Never reset/reinitialize the DMX driver.
    portENTER_CRITICAL(&mux);ws_diag.stopping=true;ws_diag.server_stop_rc=error;portEXIT_CRITICAL(&mux);
    if(server) {traced("httpd_stop",[]{return httpd_stop(server);});server=nullptr;}
    emitFirstFailure();
    if(started) traced("esp_wifi_stop",[]{return esp_wifi_stop();});
    if(station_handler) traced("station.events.unregister",[&]{return esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,station_handler);});
    worker_task=nullptr;
    portENTER_CRITICAL(&mux);stations.clear();publishStations();
#if DMX_HW_DIAGNOSTICS
    station_diag.queried=false;station_diag.applied=false;++station_trace_revision;
#endif
    portEXIT_CRITICAL(&mux);
#if DMX_HW_DIAGNOSTICS
    emitStations(true); // One final offline/error sample; no client connection required.
#endif
    if(netif) esp_netif_destroy_default_wifi(netif);
    wifi_startup_trace::capture("netif.destroy.after");
    if(initialized) traced("esp_wifi_deinit",[]{return esp_wifi_deinit();});
    record(8,0);phase(Phase::failed,error==ESP_OK?ESP_FAIL:error);
    vTaskDelete(nullptr);
}
}
void prepare() {
    application_owner=xTaskGetCurrentTaskHandle();
    // Actual SoftAP-derived address, verified against WIFI_IF_AP after Wi-Fi init.
    if(esp_read_mac(ap_mac,ESP_MAC_WIFI_SOFTAP)!=ESP_OK) {phase(Phase::failed,ESP_FAIL);return;}
    Preferences storage;
    if(!storage.begin("dmxwifi",false)) {phase(Phase::failed,ESP_FAIL);return;}
    bool reset=false,valid=true;
    if(storage.isKey("factory")) {
        valid=storage.getType("factory")==PT_U8 && storage.getUChar("factory",0)==1;
        reset=valid;
    }
    const bool exists=storage.isKey("ap1");
    const size_t length=exists?storage.getBytesLength("ap1"):0;
    uint8_t blob[sizeof(softap_policy::Credentials)]{};
    if(valid && exists && !reset)
        valid=length<=sizeof(blob) && length>0 && storage.getBytes("ap1",blob,length)==length;
    const auto decision=valid?softap_policy::select(blob,length,exists,reset,ap_mac,credentials):softap_policy::Decision::invalid;
    valid=decision!=softap_policy::Decision::invalid;
    if(decision==softap_policy::Decision::write_factory) {
        valid=storage.putBytes("ap1",&credentials,sizeof(credentials))==sizeof(credentials);
        softap_policy::Credentials verified{};
        valid=valid && storage.getBytesLength("ap1")==sizeof(verified) &&
            storage.getBytes("ap1",&verified,sizeof(verified))==sizeof(verified) && !memcmp(&verified,&credentials,sizeof(verified));
        // Only acknowledge the explicit reset after the atomic blob write/readback.
        if(valid && reset) valid=storage.remove("factory");
    }
    storage.end();
    if(!valid) {memset(&credentials,0,sizeof(credentials));phase(Phase::failed,ESP_FAIL);return;}
    phase(Phase::prepared);
}
void launch(bool canvas_reserved) {
    if(launched || status().phase!=Phase::prepared) return;
    launched=true;
    if(!canvas_reserved) {wifi_startup_trace::capture("guard.canvas",ESP_ERR_NO_MEM,true);phase(Phase::failed,ESP_ERR_NO_MEM);return;}
    const auto m=memory();
    portENTER_CRITICAL(&mux);shared.before=m;shared.current=m;samples[0]={m,1,0,0};sample_count=1;portEXIT_CRITICAL(&mux);
    const uint32_t failed_guards=(m.internal<81920?1U:0U)|(m.largest<49152?2U:0U)|(m.dma<32768?4U:0U);
    wifi_startup_trace::capture("guard.preflight",failed_guards?ESP_ERR_NO_MEM:ESP_OK,failed_guards!=0,nullptr,failed_guards);
    if(m.internal<81920 || m.largest<49152 || m.dma<32768) {phase(Phase::failed,ESP_ERR_NO_MEM);return;}
    wifi_startup_trace::capture("xTaskCreatePinnedToCore",0,false,nullptr,0,true);
    const auto task_result=xTaskCreatePinnedToCore(worker,"net-worker",8192,nullptr,1,&worker_task,0);
    wifi_startup_trace::capture("xTaskCreatePinnedToCore",task_result==pdPASS?ESP_OK:ESP_ERR_NO_MEM,task_result!=pdPASS,nullptr,uint32_t(task_result));
    if(task_result!=pdPASS) phase(Phase::failed,ESP_ERR_NO_MEM);
}
void ownerTick(int64_t now,uint32_t errors,bool stopped) {
    static int64_t previous=0;
    const uint32_t gap=previous && now>=previous?static_cast<uint32_t>(std::min(int64_t(UINT32_MAX),now-previous)):0;
    previous=now;
    portENTER_CRITICAL(&mux);
    shared.max_loop_gap_us=std::max(shared.max_loop_gap_us,gap);shared.tx_errors=errors;shared.tx_stopped=stopped;
    portEXIT_CRITICAL(&mux);
    wifi_startup_trace::service();
}
Status status() {portENTER_CRITICAL(&mux);const auto s=shared;portEXIT_CRITICAL(&mux);return s;}
const char* ssid() {return credentials.ssid;}
const char* password() {return credentials.pass;}
const char* credentialMode() {
    return credentials.origin==softap_policy::Origin::legacy?"LEGACY":
        (credentials.origin==softap_policy::Origin::factory?"FACTORY":"CUSTOM");
}
uint32_t uiRevision() {portENTER_CRITICAL(&mux);const auto r=shared.ui_revision;portEXIT_CRITICAL(&mux);return r;}
bool requestFactoryReset() {
    if(xTaskGetCurrentTaskHandle()!=application_owner) return false;
    if(status().reset_pending) return true;
    Preferences storage;
    if(!storage.begin("dmxwifi",false)) return false;
    const bool saved=storage.putUChar("factory",1)==1 && storage.getUChar("factory",0)==1;
    storage.end();
    if(saved) {
        portENTER_CRITICAL(&mux);shared.reset_pending=true;++shared.ui_revision;portEXIT_CRITICAL(&mux);
    }
    return saved; // No credential mutation/restart until the user explicitly restarts.
}

const char* phaseName(Phase p) {
    switch(p) {case Phase::disabled:return "OFF";case Phase::prepared:return "PREPARED";case Phase::starting:return "STARTING";case Phase::ready:return "READY";default:return "FAILED";}
}
}
