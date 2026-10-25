#pragma once
#include <stdint.h>
#include "websocket_bridge.h"
namespace websocket_diagnostics {
enum class Reason : uint8_t { none,peer_close,native_unattributed,io_failure,wire_invalid,
    frame_invalid,receive_failed,frame_rate,upgrade_rejected,bridge_queue_full,bridge_rate,
    bridge_gone,socket_invalid,idle,send_failed,encode_failed,server_stop };
enum class Source : uint8_t { none,handler,io,send,pump,native_callback,worker };
inline const char* name(Reason r) {
    const char* names[]={"none","peer_close","native_unattributed","io_failure","wire_invalid",
        "frame_invalid","receive_failed","frame_rate","upgrade_rejected","bridge_queue_full","bridge_rate",
        "bridge_gone","socket_invalid","idle","send_failed","encode_failed","server_stop"};
    return names[unsigned(r)];
}
inline const char* name(Source s) {
    const char* names[]={"none","handler","io","send","pump","native_callback","worker"};return names[unsigned(s)];
}
struct Closure {
    uint32_t ms=0,generation=0,updates_sent=0,queued=0,outstanding=0;
    int fd=-1,rc=0,socket_errno=0;
    Reason reason=Reason::none;Source source=Source::none;
    uint64_t last_sent_revision=0,attempt_revision=0,last_enqueued_revision=0;
};
struct Counters {
    uint32_t sockets_opened=0,sockets_closed=0,open_rejected=0,ws_opened=0,ws_closed=0,upgrade_rejected=0;
    uint32_t updates_sent=0,send_failures=0,close_frames_sent=0,unattributed_closes=0;
    uint32_t wifi_disconnects=0,last_wifi_ms=0,work_queued=0,work_started=0,work_completed=0,work_failed=0;
    int last_send_rc=0,last_work_rc=0,server_stop_rc=0;
    bool stopping=false,first_present=false,first_logged=false;
    Closure first,last;
};
struct SessionInfo {
    int fd=-1;bool websocket=false,closing=false;
    uint32_t generation=0,opened_ms=0,activity_ms=0,updates_sent=0;
    uint64_t last_sent_revision=0;
    websocket_bridge::ClientInfo bridge;
};
size_t encode(const Counters& counters,const websocket_bridge::Metrics& bridge,
    const SessionInfo* sessions,size_t count,bool pending,uint32_t now_ms,char* output,size_t capacity);
}
