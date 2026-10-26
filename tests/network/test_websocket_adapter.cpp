// Compiles the ACTUAL application endpoint and I/O source extracted by the
// integration check. Native IDF APIs/sockets/time are mocked; no hardware claims.
#include "websocket_bridge.h"
#include "websocket_wire.h"
#include "websocket_diagnostics.h"
#include <algorithm>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
using esp_err_t=int;using httpd_handle_t=void*;
constexpr int ESP_OK=0,ESP_FAIL=-1,HTTP_GET=1,HTTPD_WS_CLIENT_WEBSOCKET=2;
constexpr int HTTPD_SOCK_ERR_FAIL=-2,HTTPD_SOCK_ERR_TIMEOUT=-3;
constexpr int SOL_SOCKET=1,SO_SNDTIMEO=2,SO_RCVTIMEO=3;
using WsReason=websocket_diagnostics::Reason;using WsSource=websocket_diagnostics::Source;
websocket_diagnostics::Counters ws_diag;int mux=0;
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
enum httpd_ws_type_t {HTTPD_WS_TYPE_TEXT=1,HTTPD_WS_TYPE_CLOSE=8,HTTPD_WS_TYPE_PING=9,HTTPD_WS_TYPE_PONG=10};
struct httpd_ws_frame_t {bool final=true;bool fragmented=false;httpd_ws_type_t type=HTTPD_WS_TYPE_TEXT;uint8_t* payload=nullptr;size_t len=0;};
struct httpd_req_t {int method=HTTP_GET;const char* uri="/ws";size_t content_len=0;httpd_handle_t handle=nullptr;};
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%d: %s\n",__LINE__,#c);exit(1);}}while(0)
int64_t clock_us=1000000,request_window=0;
unsigned window_requests=0;int bytes=0,chunk=3;bool blocked=false,frame_error=false;bool delayed_partial=false,frame_socket_read=false,recv_eof=false;int recv_error=0;
std::string input="{}",host="192.168.4.1",origin="http://192.168.4.1";
httpd_ws_frame_t input_frame;
const char wire_input[]={char(0x81),char(0x82),'a','b','c','d','x','y'};size_t wire_offset=0;
int64_t esp_timer_get_time(){return clock_us;}
int setsockopt(int,int,int,const void*,size_t){return 0;}
int send(int,const char*,size_t n,int){clock_us+=100;if(blocked){clock_us+=100000;errno=ETIMEDOUT;return -1;}const int sent=int(std::min(n,size_t(chunk)));bytes+=sent;if(delayed_partial){clock_us+=518230;delayed_partial=false;}return sent;}
int recv(int,char* p,size_t n,int){clock_us+=100;if(recv_eof)return 0;if(recv_error){errno=recv_error;return -1;}if(blocked)return -1;size_t got=std::min(n,std::min(size_t(chunk),sizeof(wire_input)-wire_offset));if(!got)return -1;memcpy(p,wire_input+wire_offset,got);wire_offset+=got;return int(got);}
namespace stage5 {
constexpr size_t max_tx_bytes=20480,max_rx_bytes=2048;constexpr int64_t session_us=3000000;
struct Budget{size_t sent=0,received=0;int64_t opened=0;bool allow(int64_t,size_t,bool){return true;}};
bool allowedHost(const char* p){return !strcmp(p,"192.168.4.1");}
bool allowedOrigin(const char* p){return !*p||!strcmp(p,"http://192.168.4.1");}
}
struct Session {
    int fd=5;stage5::Budget budget;bool handled=false,closing=false,websocket=false;
    websocket_bridge::Token token;int64_t rx_deadline=0,tx_deadline=0,activity=0,ping=0;
    uint32_t frames=0;int64_t frame_window=0;
    websocket_bridge::WireGuard wire;
    WsReason close_reason=WsReason::none,io_reason=WsReason::none;
    int socket_errno=0;uint32_t io_operation=0;uint32_t updates_sent=0;uint64_t last_sent_revision=0,attempt_revision=0;
    uint32_t last_send_ms=0,send_us=0,send_max_us=0,queue_start=0,queue_end=0;
} connection;
Session* session(int fd){return fd==connection.fd?&connection:nullptr;}
httpd_handle_t server=nullptr;
void recordClosure(Session&,WsReason,WsSource,int);
void closeSession(Session& s,WsReason reason,WsSource source,int rc=0){recordClosure(s,reason,source,rc);s.closing=true;}
int httpd_req_to_sockfd(httpd_req_t*){return connection.fd;}
int httpd_ws_get_fd_info(httpd_handle_t,int){return HTTPD_WS_CLIENT_WEBSOCKET;}
size_t httpd_req_get_hdr_value_len(httpd_req_t*,const char* k){return !strcmp(k,"Host")?host.size():origin.size();}
int httpd_req_get_hdr_value_str(httpd_req_t*,const char* k,char* p,size_t cap){const auto& s=!strcmp(k,"Host")?host:origin;if(s.size()>=cap)return ESP_FAIL;memcpy(p,s.c_str(),s.size()+1);return ESP_OK;}
int io(httpd_handle_t,int,char*,size_t,int,bool);
int httpd_ws_recv_frame(httpd_req_t*,httpd_ws_frame_t* f,size_t max){if(frame_socket_read){char byte;if(io(nullptr,connection.fd,&byte,1,0,false)<0)return ESP_FAIL;}if(frame_error)return ESP_FAIL;if(!max){*f=input_frame;f->len=input.size();}else memcpy(f->payload,input.data(),input.size());return ESP_OK;}
int io(httpd_handle_t,int,char*,size_t,int,bool);
int httpd_ws_send_frame_async(httpd_handle_t,int fd,httpd_ws_frame_t* f){char header[4]{};if(io(nullptr,fd,header,sizeof(header),0,true)<0)return ESP_FAIL;return !f->len||io(nullptr,fd,reinterpret_cast<char*>(f->payload),f->len,0,true)>=0?ESP_OK:ESP_FAIL;}
namespace websocket_service {
unsigned opened=0,closed=0,received=0,failed=0;bool capacity=true;
websocket_bridge::Token open(uint64_t){websocket_bridge::Token t;if(capacity)t.generation=++opened;return t;}
bool alive(websocket_bridge::Token t){return t.generation!=0;}
void receive(websocket_bridge::Token,const char*,size_t,uint64_t){++received;}
void sendFailed(websocket_bridge::Token){++failed;}
websocket_bridge::ClientInfo clientInfo(websocket_bridge::Token){return {};}
websocket_bridge::Metrics metrics(){return {};}
}
#include "ws_adapter_under_test.h"
void reset(){connection=Session{};clock_us+=2000000;host="192.168.4.1";origin="http://192.168.4.1";blocked=false;frame_error=false;delayed_partial=false;frame_socket_read=recv_eof=false;recv_error=0;input="{}";input_frame=httpd_ws_frame_t{};websocket_service::capacity=true;}
void handshake(){httpd_req_t req;CHECK(websocket(&req)==ESP_OK&&connection.websocket);}
int main(){
    httpd_req_t req;
    reset();origin="http://evil.example";CHECK(websocket(&req)==ESP_FAIL&&!connection.websocket);
    reset();host="evil.example";CHECK(websocket(&req)==ESP_FAIL);
    reset();websocket_service::capacity=false;CHECK(websocket(&req)==ESP_FAIL);
    reset();req.uri="/ws?extra=1";CHECK(websocket(&req)==ESP_FAIL);req.uri="/ws";
    reset();handshake();CHECK(websocket_service::opened==1);CHECK(websocket(&req)==ESP_FAIL); // repeated upgrade.
    req.method=0;CHECK(websocket(&req)==ESP_OK&&websocket_service::received==1);
    input_frame.final=false;CHECK(websocket(&req)==ESP_FAIL);input_frame.final=true;
    input_frame.type=static_cast<httpd_ws_type_t>(2);CHECK(websocket(&req)==ESP_FAIL);
    input_frame.type=HTTPD_WS_TYPE_TEXT;input.assign(257,'x');CHECK(websocket(&req)==ESP_FAIL);
    input="{}";frame_error=true;CHECK(websocket(&req)==ESP_FAIL);frame_error=false;
    input_frame.type=HTTPD_WS_TYPE_PING;CHECK(websocket(&req)==ESP_OK);
    input_frame.type=HTTPD_WS_TYPE_PONG;CHECK(websocket(&req)==ESP_OK);
    input_frame.type=HTTPD_WS_TYPE_CLOSE;CHECK(websocket(&req)==ESP_FAIL);
    reset();handshake();char packet[2300]{};bytes=0;chunk=3;
    CHECK(wsSend(connection,HTTPD_WS_TYPE_TEXT,reinterpret_cast<uint8_t*>(packet),sizeof(packet)));CHECK(bytes==2304); // actual short-write loop.
    ws_diag=websocket_diagnostics::Counters{};connection.close_reason=WsReason::none;
    connection.last_sent_revision=268;connection.attempt_revision=269;connection.updates_sent=7;
    blocked=true;CHECK(!wsSend(connection,HTTPD_WS_TYPE_TEXT,reinterpret_cast<uint8_t*>(packet),sizeof(packet))&&connection.closing&&websocket_service::failed==1);
    CHECK(ws_diag.last_send_rc==ESP_FAIL&&ws_diag.send_failures==1&&ws_diag.first_present);
    CHECK(ws_diag.send_timeouts==1&&ws_diag.send_max_us>=100000&&ws_diag.first.send_us>=100000);
    CHECK(ws_diag.first.reason==WsReason::send_failed&&ws_diag.first.last_sent_revision==268&&ws_diag.first.attempt_revision==269&&ws_diag.first.updates_sent==7);
    connection.close_reason=WsReason::none;recordClosure(connection,WsReason::idle,WsSource::pump);
    CHECK(ws_diag.first.reason==WsReason::send_failed&&ws_diag.last.reason==WsReason::idle); // immutable first failure.
    reset();handshake();ws_diag=websocket_diagnostics::Counters{};chunk=1;delayed_partial=true;
    CHECK(!wsSend(connection,HTTPD_WS_TYPE_TEXT,reinterpret_cast<uint8_t*>(packet),sizeof(packet)));
    CHECK(connection.closing&&ws_diag.first.io_operation==1&&ws_diag.first.socket_errno==ETIMEDOUT);
    CHECK(ws_diag.first.send_us==518330&&ws_diag.send_timeouts==1);
    // A positive partial send may return after the nominal deadline. No further write is allowed.
    reset();handshake();bytes=0;connection.tx_deadline=clock_us+999;
    CHECK(io(nullptr,5,packet,1,0,true)<0&&bytes==0&&connection.io_operation==1);
    reset();handshake();connection.rx_deadline=clock_us-1;CHECK(io(nullptr,5,packet,2,0,false)<0);
    connection.rx_deadline=0;CHECK(io(nullptr,5,packet,8,0,false)==8); // split mask/length bytes.
    // IDF maps first-header TCP EOF to a synthetic CLOSE before invoking the handler.
    // Its subsequent frame read can EOF again: the actual wrapper preserves io_failure/errno0.
    reset();handshake();ws_diag=websocket_diagnostics::Counters{};req.method=0;
    recv_eof=true;errno=ECONNRESET;CHECK(io(nullptr,connection.fd,packet,1,0,false)<0);
    input_frame.type=HTTPD_WS_TYPE_CLOSE;frame_socket_read=true;
    CHECK(websocket(&req)==ESP_FAIL&&ws_diag.first_present);
    CHECK(ws_diag.first.reason==WsReason::io_failure&&ws_diag.first.source==WsSource::io);
    CHECK(ws_diag.first.io_operation==4&&ws_diag.first.rc==ESP_FAIL&&ws_diag.first.socket_errno==0);
    CHECK(ws_diag.send_failures==0&&ws_diag.close_frames_sent==0);
    // Actual negative receive errors retain errno, never get rewritten as an ordinary peer CLOSE.
    for(int error:{ECONNRESET,ETIMEDOUT}) {
        reset();handshake();ws_diag=websocket_diagnostics::Counters{};frame_socket_read=true;recv_error=error;
        CHECK(websocket(&req)==ESP_FAIL&&ws_diag.first.socket_errno==error);
        CHECK(ws_diag.first.io_operation==4&&ws_diag.first.reason==WsReason::io_failure&&ws_diag.send_failures==0);
    }
    // An explicit complete WebSocket CLOSE uses the separate existing peer_close path.
    reset();handshake();ws_diag=websocket_diagnostics::Counters{};input_frame.type=HTTPD_WS_TYPE_CLOSE;input="\x03\xE8";
    CHECK(websocket(&req)==ESP_FAIL&&ws_diag.last.reason==WsReason::peer_close);
    CHECK(!ws_diag.first_present&&ws_diag.close_frames_sent==1&&ws_diag.send_failures==0);
    reset();req.method=HTTP_GET;connection.wire.httpBytes("GET /ws HTTP/1.1\r\n\r\nx",21);CHECK(websocket(&req)==ESP_FAIL);
    puts("PASS actual native endpoint/I/O with API mocks: upgrade policy/capacity/control/binary/fragment/oversize/incomplete/deadline/short-write behavior");
}
