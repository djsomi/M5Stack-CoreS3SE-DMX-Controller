#include "websocket_diagnostics.h"
#include <stdio.h>
#include <stdarg.h>
namespace websocket_diagnostics {
namespace {
struct Writer {
    char* data;size_t cap,used=0;bool ok=true;
    Writer(char* out,size_t capacity):data(out),cap(capacity){}
    void add(const char* format,...) {
        if(!ok)return;
        va_list args;va_start(args,format);const int n=vsnprintf(data+used,cap-used,format,args);va_end(args);
        if(n<0||size_t(n)>=cap-used){ok=false;return;}used+=size_t(n);
    }
};
void closure(Writer& w,const Closure& c) {
    w.add("{\"ms\":%u,\"gen\":%u,\"fd\":%d,\"reason\":\"%s\",\"source\":\"%s\",\"rc\":%d,\"errno\":%d,\"updates_sent\":%u,\"queued\":%u,\"outstanding\":%u,\"last_sent_revision\":\"%llu\",\"trigger_revision\":\"%llu\",\"last_enqueued_revision\":\"%llu\"}",
        unsigned(c.ms),unsigned(c.generation),c.fd,name(c.reason),name(c.source),c.rc,c.socket_errno,
        unsigned(c.updates_sent),unsigned(c.queued),unsigned(c.outstanding),(unsigned long long)c.last_sent_revision,
        (unsigned long long)c.attempt_revision,(unsigned long long)c.last_enqueued_revision);
}
}
size_t encode(const Counters& c,const websocket_bridge::Metrics& b,const SessionInfo* sessions,size_t count,bool pending,uint32_t now,char* output,size_t capacity) {
    Writer w(output,capacity);
    w.add("{\"marker\":\"WS_DIAGNOSTICS\",\"ms\":%u,\"http_opened\":%u,\"http_closed\":%u,\"open_rejected\":%u,\"ws_opened\":%u,\"ws_closed\":%u,\"upgrade_rejected\":%u,\"bridge_opened\":%u,\"bridge_closed\":%u,\"outbox\":%u,\"outbox_high\":%u,\"outstanding\":%u,\"updates_enqueued\":%u,\"updates_taken\":%u,\"updates_sent\":%u,\"queue_full\":%u,\"send_failures\":%u,\"last_send_rc\":%d,\"close_frames_sent\":%u,\"native_unattributed_closes\":%u,\"lru_purge\":false,\"native_evictions\":null,\"wifi_disconnects\":%u,\"last_wifi_ms\":%u,\"wifi_reason_available\":false,\"work_pending\":%s,\"work_queued\":%u,\"work_started\":%u,\"work_completed\":%u,\"work_failed\":%u,\"last_work_rc\":%d,\"controller_events\":%u,\"publications\":%u,\"last_controller_revision\":\"%llu\",\"first\":",
        unsigned(now),unsigned(c.sockets_opened),unsigned(c.sockets_closed),unsigned(c.open_rejected),unsigned(c.ws_opened),unsigned(c.ws_closed),unsigned(c.upgrade_rejected),
        unsigned(b.opened),unsigned(b.closed),unsigned(b.outbox),unsigned(b.outbox_high),unsigned(b.outstanding),unsigned(b.updates_enqueued),unsigned(b.updates_taken),
        unsigned(c.updates_sent),unsigned(b.queue_full),unsigned(c.send_failures),c.last_send_rc,unsigned(c.close_frames_sent),unsigned(c.unattributed_closes),
        unsigned(c.wifi_disconnects),unsigned(c.last_wifi_ms),pending?"true":"false",unsigned(c.work_queued),unsigned(c.work_started),unsigned(c.work_completed),unsigned(c.work_failed),c.last_work_rc,
        unsigned(b.controller_events),unsigned(b.broadcasts),(unsigned long long)b.last_controller_revision);
    if(c.first_present){w.add("{\"marker\":\"WS_FIRST_FAILURE\",\"closure\":");closure(w,c.first);w.add("}");}else w.add("null");
    w.add(",\"last_close\":");closure(w,c.last);
    w.add(",\"bridge_first\":");
    if(b.first.present)w.add("{\"ms\":%u,\"gen\":%u,\"slot\":%u,\"stop\":%u,\"queued\":%u,\"outstanding\":%u,\"last_enqueued_revision\":\"%llu\",\"rejected_revision\":\"%llu\"}",
        unsigned(b.first.ms),unsigned(b.first.token.generation),unsigned(b.first.token.slot),unsigned(b.first.stop),unsigned(b.first.queued),unsigned(b.first.outstanding),
        (unsigned long long)b.first.last_enqueued_revision,(unsigned long long)b.first.rejected_revision);
    else w.add("null");
    w.add(",\"sessions\":[");bool comma=false;
    for(size_t i=0;i<count;++i) {
        const auto& s=sessions[i];if(s.fd<0)continue;
        w.add("%s{\"fd\":%d,\"ws\":%s,\"closing\":%s,\"gen\":%u,\"opened_ms\":%u,\"activity_ms\":%u,\"active\":%s,\"queued\":%u,\"outstanding\":%u,\"updates_sent\":%u,\"last_sent_revision\":\"%llu\"}",
            comma?",":"",s.fd,s.websocket?"true":"false",s.closing?"true":"false",unsigned(s.generation),unsigned(s.opened_ms),unsigned(s.activity_ms),
            s.bridge.active?"true":"false",unsigned(s.bridge.queued),unsigned(s.bridge.outstanding),unsigned(s.updates_sent),(unsigned long long)s.last_sent_revision);
        comma=true;
    }
    w.add("]}");return w.ok?w.used:0;
}
}
