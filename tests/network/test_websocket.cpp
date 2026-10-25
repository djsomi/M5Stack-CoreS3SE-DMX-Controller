#include "websocket_bridge.h"
#include "websocket_wire.h"
#include "websocket_pump.h"
#include "websocket_diagnostics.h"
#include "../support/fake_preset_storage.h"
#include <mutex>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
using namespace websocket_bridge;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"%d: %s\n",__LINE__,#c);exit(1);}}while(0)
struct Mutex : Lock {std::mutex mutex;void enter() override{mutex.lock();}void leave()override{mutex.unlock();}};
struct CountingStorage : FakeStorage {unsigned inspections=0;SlotState inspect(uint8_t slot) override{++inspections;return FakeStorage::inspect(slot);}};
struct Rig {
    CountingStorage storage;DmxController controller{storage};CommandDispatcher dispatcher{controller};Mutex lock;
    Bridge bridge{controller,dispatcher,lock};uint64_t clock=1000;Event event;
    Rig(){dispatcher.setSenderActive(true);bridge.ownerPoll(true);}
    Token connect(){auto t=bridge.open(clock);CHECK(t.generation);bridge.ownerPoll(true);CHECK(bridge.take(t,event)&&event.kind==Kind::hello);CHECK(bridge.take(t,event)&&event.kind==Kind::snapshot);return t;}
    void send(Token t,const std::string& text){bridge.receive(t,text.data(),text.size(),clock);}
    void apply(){bridge.ownerPoll(true);dispatcher.process();clock+=51;}
    void drain(Token t){while(bridge.take(t,event)){};}
};
std::string channel(unsigned id,int ch,int value){return "{\"v\":1,\"id\":"+std::to_string(id)+",\"type\":\"channel.set\",\"channel\":"+std::to_string(ch)+",\"value\":"+std::to_string(value)+"}";}
std::string blackout(unsigned id,bool on){return "{\"v\":1,\"id\":"+std::to_string(id)+",\"type\":\"blackout.set\",\"active\":"+(on?"true":"false")+"}";}
void protocol() {
    const char* bad[]={"{}","[]","null","{\"v\":1,\"id\":1,\"type\":\"channel.set\",\"channel\":1,\"value\":1.5}",
        "{\"v\":1,\"v\":1,\"id\":1,\"type\":\"state.request\"}",
        "{\"v\":1,\"id\":4294967296,\"type\":\"state.request\"}",
        "{\"v\":1,\"id\":01,\"type\":\"state.request\"}",
        "{\"v\":1,\"id\":0,\"type\":\"state.request\"}",
        "{\"v\":1,\"id\":1,\"type\":\"state.request\",\"slot\":1}",
        "{\"v\":1,\"id\":1,\"type\":\"reset_sender\"}",
        "{\"v\":1,\"id\":1,\"type\":\"blackout.set\",\"active\":1}",
        "{\"v\":1,\"id\":1,\"type\":\"state.requ\\u0065st\"}"};
    for(auto text:bad){Request r;const char* why;CHECK(!decode(text,strlen(text),r,why));}
    for(auto text:{channel(1,0,1),channel(1,513,1),channel(1,1,-1),channel(1,1,256),channel(1,1,100)+"x",std::string(257,'x')}) {
        Request r;const char* why;CHECK(!decode(text.data(),text.size(),r,why));
    }
    Request r;const char* why;const char* version="{\"v\":2,\"id\":1,\"type\":\"state.request\"}";
    CHECK(!decode(version,strlen(version),r,why)&&!strcmp(why,"version"));
    for(unsigned ch:{1U,512U}){Request good;auto t=channel(1,int(ch),255);CHECK(decode(t.data(),t.size(),good,why));CHECK(good.command.payload.channel.channel==int(ch));}
    for(unsigned opcode=0;opcode<16;++opcode) {
        CHECK(!validFrame(opcode,false,1));
        CHECK(validFrame(opcode,true,1)==(opcode==1||opcode==9||opcode==10));
    }
    CHECK(!validFrame(1,true,257)&&!validFrame(9,true,126)&&!validFrame(1,true,0)&&validFrame(8,true,0)&&validFrame(8,true,2));
    Event e;e.kind=Kind::snapshot;e.revision=UINT64_MAX;memset(e.values,255,512);char json[max_json];
    CHECK(encode(e,json,sizeof(json))>2100);CHECK(strstr(json,"18446744073709551615"));CHECK(!encode(e,json,10));
}
void wireProtocol() {
    WireGuard http;http.httpBytes("GET /ws HTTP/1.1\r\n\r",19);http.httpBytes("\n",1);CHECK(!http.trailing());http.httpBytes("x",1);CHECK(http.trailing());
    const unsigned char valid[]={0x81,0xfe,1,0,'m','a','s','k'};
    WireGuard wire;for(auto c:valid){const char b=char(c);CHECK(wire.wsBytes(&b,1));}
    char data[256]{};CHECK(wire.wsBytes(data,256));
    const char ping[]={char(0x89),char(0x80),0,0,0,0};CHECK(wire.wsBytes(ping,sizeof(ping)));
    for(auto bytes: {std::string("\x01\x80",2),std::string("\x82\x80",2),std::string("\xc1\x80",2),
                    std::string("\x81\x01",2),std::string("\x81\xff",2),std::string("\x81\xfe\x01\x01",4),
                    std::string("\x81\xfe\x00\x01",4),std::string("\x89\xfe",2),std::string("\x88\x81",2)}) {
        WireGuard bad;CHECK(!bad.wsBytes(bytes.data(),bytes.size()));CHECK(!bad.wsBytes(ping,sizeof(ping)));
    }
}
void lifecycleAndOrder() {
    Rig r;Token clients[4];for(auto& c:clients)c=r.connect();CHECK(!r.bridge.open(1000).generation);
    r.send(clients[0],channel(1,512,255));CHECK(r.controller.channel(512)==0);CHECK(!r.bridge.take(clients[0],r.event));
    r.apply();const auto rev=r.controller.revision();
    for(auto c:clients){CHECK(r.bridge.take(c,r.event)&&r.event.kind==Kind::channel&&r.event.value==255&&r.event.revision==rev);}
    CHECK(r.bridge.take(clients[0],r.event)&&r.event.kind==Kind::ack&&r.event.id==1);
    CHECK(r.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,512,42)).status==CommandStatus::applied);
    for(auto c:clients){CHECK(r.bridge.take(c,r.event)&&r.event.value==42&&r.event.revision>rev);}
    // Disconnected generation cannot take, submit, close or receive old work.
    r.bridge.close(clients[0]);auto next=r.connect();CHECK(next.generation!=clients[0].generation);
    r.send(clients[0],channel(2,1,200));r.bridge.close(clients[0]);CHECK(r.bridge.alive(next));r.apply();CHECK(r.controller.channel(1)==0);
    r.send(next,channel(1,1,12));r.bridge.close(next);r.apply();CHECK(r.controller.channel(1)==0); // not submitted yet: cancelled.
}
void snapshotRaceAndSources() {
    Rig r;auto a=r.connect();auto b=r.bridge.open(1000);
    r.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,3,17)); // before initial snapshot.
    r.bridge.ownerPoll(true);Event e;CHECK(r.bridge.take(b,e)&&e.kind==Kind::hello);CHECK(r.bridge.take(b,e)&&e.values[2]==17);const auto revision=e.revision;
    r.send(a,channel(1,3,18));r.send(b,channel(1,3,19));r.apply();r.apply();CHECK(r.controller.channel(3)==19);
    uint64_t previous=revision;unsigned updates=0;
    while(r.bridge.take(b,e))if(e.kind==Kind::channel){CHECK(e.revision>previous);previous=e.revision;++updates;}
    CHECK(updates==2);
    r.drain(a);r.send(a,"{\"v\":1,\"id\":2,\"type\":\"state.request\"}");r.apply();
    CHECK(r.bridge.take(a,e)&&e.kind==Kind::snapshot&&e.values[2]==19&&e.revision==r.controller.revision());
}
struct Gate {bool admit=false;static bool admission(void* context,const ControllerCommand&){return static_cast<Gate*>(context)->admit;}};
void blackoutAndFailure() {
    Rig r;auto a=r.connect();Gate gate;r.dispatcher.setAdmission(Gate::admission,&gate);
    r.send(a,blackout(1,true));r.apply();CHECK(!r.controller.blackoutActive());CHECK(!r.bridge.take(a,r.event));
    r.send(a,channel(2,1,150));r.send(a,blackout(3,false));
    gate.admit=true;r.dispatcher.process();CHECK(r.controller.blackoutActive());r.drain(a);
    r.apply();CHECK(r.controller.channel(1)==150&&r.controller.outputPacket()[1]==0);r.drain(a);
    r.apply();CHECK(!r.controller.blackoutActive()&&r.controller.channel(1)==0&&r.controller.workingPacket()[0]==0);
    CHECK(r.bridge.take(a,r.event)&&r.event.kind==Kind::universe&&!r.event.blackout&&r.event.values[0]==0);
    r.drain(a);
    r.dispatcher.setSenderActive(false);r.send(a,channel(4,1,30));r.bridge.ownerPoll(false);
    CHECK(r.bridge.take(a,r.event)&&r.event.kind==Kind::mode);
    CHECK(r.bridge.take(a,r.event)&&r.event.kind==Kind::error&&r.event.result==CommandStatus::unsupported_mode);
    CHECK(r.controller.channel(1)==0);
}
void inFlightDisconnectAndLatest() {
    Rig r;auto a=r.connect();r.send(a,channel(1,1,99));r.bridge.ownerPoll(true);
    CHECK(r.dispatcher.pending()==1);r.bridge.close(a);
    auto b=r.bridge.open(1000);CHECK(b.generation&&b.generation!=a.generation);
    r.dispatcher.process();CHECK(r.controller.channel(1)==99); // Already admitted intent survives disconnect.
    r.bridge.ownerPoll(true);CHECK(r.bridge.take(b,r.event)&&r.event.kind==Kind::hello);
    CHECK(r.bridge.take(b,r.event)&&r.event.kind==Kind::snapshot&&r.event.values[0]==99);
    CHECK(!r.bridge.take(b,r.event)); // No stale ack from old generation.
    for(unsigned i=1;i<=100;++i){r.send(b,channel(i,512,int(i)));r.apply();r.drain(b);}
    CHECK(r.controller.channel(512)==100&&r.bridge.alive(b));
    r.send(b,blackout(101,true));r.apply();r.drain(b);
    r.send(b,channel(102,1,10));r.apply();r.drain(b);
    r.send(b,blackout(103,false));r.apply();r.drain(b);
    CHECK(r.controller.channel(1)==99); // Restore-entry snapshot discards blackout edits.
}
void presets() {
    Rig r;auto t=r.connect();CHECK(r.storage.inspections==8);
    auto second=r.connect();CHECK(r.storage.inspections==8);r.bridge.close(second);
    r.send(t,blackout(1,true));r.apply();r.drain(t);CHECK(r.storage.inspections==8);
    r.send(t,blackout(2,false));r.apply();r.drain(t);CHECK(r.storage.inspections==8);
    r.bridge.close(t);t=r.connect();
    r.send(t,channel(1,1,70));r.apply();r.drain(t);
    r.send(t,"{\"v\":1,\"id\":2,\"type\":\"preset.save\",\"slot\":1}");r.apply();r.drain(t);CHECK(r.storage.saves==1);
    r.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,1,0));r.drain(t);
    r.send(t,"{\"v\":1,\"id\":3,\"type\":\"preset.load\",\"slot\":1}");r.apply();r.drain(t);CHECK(r.controller.channel(1)==70);
    r.storage.fail_save=true;r.send(t,"{\"v\":1,\"id\":4,\"type\":\"preset.save\",\"slot\":2}");r.apply();
    CHECK(r.bridge.take(t,r.event)&&r.event.kind==Kind::error&&r.event.result==CommandStatus::storage_error);
}
void boundsAndCleanup() {
    Rig r;auto a=r.connect();auto slow=r.connect();
    for(unsigned i=1;i<=5;++i)r.send(a,channel(i,1,int(i)));
    CHECK(r.bridge.metrics().inbox==4&&r.bridge.metrics().rejected==1);r.drain(a);
    for(unsigned i=0;i<4;++i){r.apply();r.drain(a);}
    r.send(a,channel(5,1,55));CHECK(r.bridge.take(a,r.event)&&r.event.reason&&!strcmp(r.event.reason,"id_order"));
    for(unsigned i=6;i<20;++i){r.send(a,channel(i,1,int(i)));r.apply();r.drain(a);}
    CHECK(!r.bridge.alive(slow)&&r.bridge.metrics().slow_closes>=1);auto next=r.connect();r.bridge.sendFailed(next);CHECK(!r.bridge.alive(next));
    // Queue capacity across all four clients, plus explicit backpressure.
    Rig q;Token t[4];for(auto& c:t)c=q.connect();
    for(auto c:t)for(unsigned i=1;i<=4;++i)q.send(c,channel(i,1,1));
    CHECK(q.bridge.metrics().inbox==16);q.send(t[0],channel(5,1,2));CHECK(q.bridge.metrics().rejected==1);
    // Text flood closes deterministically, including invalid requests.
    Rig flood;auto f=flood.connect();for(unsigned i=0;i<21;++i){flood.send(f,"{}");flood.drain(f);}CHECK(!flood.bridge.alive(f));
}
void crossTaskStress() {
    Rig r;auto token=r.connect();
    std::thread network([&]{for(unsigned i=1;i<=1000;++i){r.bridge.receive(token,channel(i,4,int(i%256)).c_str(),channel(i,4,int(i%256)).size(),i*51);Event e;while(r.bridge.take(token,e)){};}});
    for(unsigned i=0;i<1000;++i){r.bridge.ownerPoll(true);r.dispatcher.process();}
    network.join();CHECK(r.controller.workingPacket()[0]==0);r.bridge.close(token);CHECK(r.bridge.metrics().clients==0);
}
void pumpRegression() {
    // Old policy: 100 Hz touchscreen, 50 Hz one-message drain, phase offset 10ms.
    // Reproduces seven deliveries then queue-full. No client requests at all.
    Rig old;auto t=old.connect();unsigned sent=0;
    for(unsigned ms=0;ms<160;ms+=10) {
        old.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,1,int(ms/10+1)));
        if(!old.bridge.alive(t))break;
        if(ms%20==10){CHECK(old.bridge.take(t,old.event));++sent;}
    }
    const auto failure=old.bridge.metrics();
    CHECK(sent==7&&failure.queue_full==1&&failure.first.present&&failure.first.queued==8&&failure.first.outstanding==0);
    CHECK(failure.first.stop==Stop::queue_full&&failure.updates_taken==7);
    CHECK(failure.updates_enqueued==15&&failure.first.last_enqueued_revision==15&&failure.first.rejected_revision==16);
    CHECK(old.bridge.clientInfo(t).stop==Stop::queue_full);
    old.bridge.close(t);CHECK(old.bridge.metrics().closed==1); // no double close.
    // New actual scheduling policy: bounded round-robin drain, same traffic.
    Rig fixed;Token clients[4];for(auto& token:clients)token=fixed.connect();
    unsigned first=0,delivered[4]{};uint64_t revisions[4]{};
    for(unsigned ms=0;ms<10000;ms+=10) {
        fixed.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,1,int((ms/10)%255+1)));
        if(ms%20==10)drain(4,first,[&](unsigned index){Event e;
            if(!fixed.bridge.take(clients[index],e))return false;
            CHECK(e.kind==Kind::channel&&e.revision>revisions[index]);revisions[index]=e.revision;++delivered[index];return true;
        },[]{return int64_t(0);});
    }
    for(unsigned i=0;i<4;++i)CHECK(delivered[i]==1000&&fixed.bridge.alive(clients[i]));
    auto m=fixed.bridge.metrics();CHECK(m.outbox==0&&m.queue_full==0&&m.updates_enqueued==4000&&m.updates_taken==4000&&m.outstanding==0);
    // Budget expiry rotates first client and does not spin/drain without bounds.
    int64_t time=0;first=0;unsigned calls=0;
    drain(4,first,[&](unsigned){++calls;time+=pump_budget_us;return true;},[&]{return time;});
    CHECK(calls==1&&first==1);
    calls=0;drain(4,first,[&](unsigned){++calls;return true;},[]{return int64_t(0);});
    CHECK(calls==4*pump_rounds);
    Rig slow;auto token=slow.connect();
    for(unsigned i=1;i<=20;++i){slow.dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,2,int(i)));
        slow.clock+=2000;CHECK(slow.bridge.take(token,slow.event)&&slow.event.value==i);}
    CHECK(slow.bridge.alive(token)&&slow.bridge.metrics().outstanding==0); // Seven is not an intrinsic message limit.
    Rig ordered;auto o=ordered.connect();
    ordered.send(o,blackout(1,true));ordered.apply();
    ordered.send(o,channel(2,1,100));ordered.apply();
    ordered.send(o,blackout(3,false));ordered.apply();
    Event events[6];unsigned n=0;first=0;
    for(unsigned pass=0;pass<2;++pass)drain(1,first,[&](unsigned){Event e;if(!ordered.bridge.take(o,e))return false;CHECK(n<6);events[n++]=e;return true;},[]{return int64_t(0);});
    CHECK(n==6&&events[0].kind==Kind::universe&&events[0].blackout&&events[1].kind==Kind::ack&&events[1].id==1);
    CHECK(events[2].kind==Kind::channel&&events[2].blackout&&events[3].id==2);
    CHECK(events[4].kind==Kind::universe&&!events[4].blackout&&events[4].values[0]==0&&events[5].id==3);
}
void diagnosticBounds() {
    using namespace websocket_diagnostics;
    Counters c;c.first_present=true;
    for(auto field:{&Counters::sockets_opened,&Counters::sockets_closed,&Counters::open_rejected,&Counters::ws_opened,&Counters::ws_closed,&Counters::upgrade_rejected,
        &Counters::updates_sent,&Counters::send_failures,&Counters::close_frames_sent,&Counters::unattributed_closes,&Counters::wifi_disconnects,&Counters::last_wifi_ms,
        &Counters::work_queued,&Counters::work_started,&Counters::work_completed,&Counters::work_failed})c.*field=UINT32_MAX;
    c.last_send_rc=c.last_work_rc=INT32_MIN;
    c.first.ms=c.first.generation=c.first.updates_sent=UINT32_MAX;c.first.queued=8;c.first.outstanding=4;
    c.first.fd=c.first.rc=c.first.socket_errno=INT32_MIN;c.first.reason=Reason::native_unattributed;c.first.source=Source::native_callback;
    c.first.last_sent_revision=c.first.attempt_revision=c.first.last_enqueued_revision=UINT64_MAX;c.last=c.first;
    Metrics b;b.opened=b.closed=b.updates_enqueued=b.updates_taken=b.queue_full=b.controller_events=b.broadcasts=UINT32_MAX;
    b.last_controller_revision=UINT64_MAX;b.first.present=true;b.first.last_enqueued_revision=b.first.rejected_revision=UINT64_MAX;
    b.first.ms=b.first.token.generation=UINT32_MAX;b.outbox=32;b.outbox_high=8;b.outstanding=16;
    SessionInfo sessions[6];for(auto& s:sessions){s.fd=INT32_MAX;s.websocket=true;s.generation=s.opened_ms=s.activity_ms=s.updates_sent=UINT32_MAX;
        s.last_sent_revision=UINT64_MAX;s.bridge.active=false;s.bridge.queued=8;s.bridge.outstanding=4;}
    char text[4096];CHECK(websocket_diagnostics::encode(c,b,sessions,6,false,UINT32_MAX,text,sizeof(text)));
    CHECK(strstr(text,"WS_FIRST_FAILURE")&&strstr(text,"\"native_evictions\":null"));
    CHECK(!websocket_diagnostics::encode(c,b,sessions,6,true,0,text,32));
}
int main(){protocol();wireProtocol();lifecycleAndOrder();snapshotRaceAndSources();blackoutAndFailure();inFlightDisconnectAndLatest();presets();boundsAndCleanup();crossTaskStress();pumpRegression();diagnosticBounds();
    printf("PASS protocol/session/revision/snapshot races/Dispatcher/touch/blackout/mode/presets/backpressure/generation/cleanup/cross-task stress; Bridge=%zu Event=%zu Request=%zu\n",sizeof(Bridge),sizeof(Event),sizeof(Request));}
