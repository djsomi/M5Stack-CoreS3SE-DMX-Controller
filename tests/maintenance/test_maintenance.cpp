#include "device_maintenance.h"
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
using M=DeviceMaintenance;
struct Backend : M::Backend {
    bool conflict=false;unsigned begins=0,zeros=0,retired=0,executions=0;
    M::Step zero=M::Step::ready,retire=M::Step::ready;M::Action action=M::Action::none;
    bool busy()override{return conflict;}
    void begin(bool)override{++begins;}
    M::Step zeroFrame(int64_t)override{++zeros;return zero;}
    M::Step quiesce(int64_t)override{++retired;return retire;}
    void execute(M::Action a)override{++executions;action=a;}
};
struct TestLock : websocket_bridge::Lock {
    int depth=0;uint32_t ms=1000;
    void enter()override{++depth;}void leave()override{--depth;}uint32_t nowMs()const override{return ms;}
};
int main(){
    for(auto action:{M::Action::restart,M::Action::power_off}){
        Backend b;M m(b);
        assert(m.request(M::Action::none,1,1,100)==M::Result::invalid);
        assert(m.request(action,0,1,100)==M::Result::invalid);
        assert(m.request(action,1,1,100)==M::Result::accepted);
        assert(m.snapshot().phase==M::Phase::countdown&&m.remainingSeconds(100)==5&&!m.committed());
        assert(m.request(action,2,2,101)==M::Result::busy);
        assert(m.cancel(2,1,101)==M::Result::unauthorized);
        for(unsigned elapsed:{0U,1U,999U,1000U,3999U,4000U,4999U}){
            m.poll(100+elapsed,(100+elapsed)*1000,true);
            assert(!m.committed()&&b.begins==0&&b.executions==0);
            assert(m.remainingSeconds(100+elapsed)==(5000-elapsed+999)/1000);
        }
        assert(m.cancel(1,1,5099)==M::Result::cancelled&&!m.active());
        for(unsigned i=2;i<22;++i){assert(m.request(action,1,i,10000)==M::Result::accepted);assert(m.cancelPhysical(10000)==M::Result::cancelled);}
        assert(m.request(action,1,22,10000)==M::Result::accepted);
        assert(m.cancel(1,22,15000)==M::Result::busy);
        m.poll(15000,15000000,true);assert(m.committed()&&b.executions==0);
        assert(m.cancelPhysical(15000)==M::Result::busy);
        for(unsigned t=15001;t<15006;++t)m.poll(t,t*1000,true);
        assert(b.executions==1&&b.action==action);
        if(action==M::Action::power_off)assert(b.begins==0&&b.zeros==0&&b.retired==0);
        else assert(b.begins==1&&b.zeros==3&&b.retired==1);
        assert(m.snapshot().phase==M::Phase::failed&&m.snapshot().reason==M::Reason::returned);
        for(unsigned t=15006;t<15020;++t)m.poll(t,t*1000,true);
        assert(b.executions==1);
    }
    Backend wrap;M w(wrap);const uint32_t start=0xfffffff0;
    w.request(M::Action::power_off,1,1,start);w.poll(uint32_t(start+4999),1,true);assert(!w.committed());
    w.poll(uint32_t(start+5000),2,true);assert(w.committed());w.poll(uint32_t(start+5001),3,true);assert(wrap.executions==1);
    for(bool fail:{false,true}){
        Backend bad;M f(bad);bad.zero=fail?M::Step::failed:M::Step::pending;
        f.local(M::Action::restart,1,true);f.poll(fail?2:2001,1,true);
        assert(f.snapshot().phase==M::Phase::failed&&bad.executions==0);
    }
    Backend conflict;M busy(conflict);conflict.conflict=true;assert(busy.request(M::Action::restart,1,1,0)==M::Result::busy);
    Backend retirement;M retire(retirement);retirement.retire=M::Step::failed;
    retire.local(M::Action::restart,1,false);retire.poll(2,1,false);assert(retirement.executions==0);
    using namespace websocket_bridge;
    Backend hw;M authority(hw);TestLock lock;FakeStorage storage;DmxController controller(storage);
    CommandDispatcher dispatcher(controller);dispatcher.setSenderActive(true);Bridge bridge(controller,dispatcher,lock,nullptr,&authority);
    Event e;auto drain=[&](Token t){while(bridge.take(t,e)){};};
    auto open=[&](){auto t=bridge.open(lock.ms);bridge.ownerPoll(true);drain(t);return t;};
    auto a=open(),peer=open(),legacy=open();
    auto send=[&](Token t,const char* json){bridge.receive(t,json,strlen(json),lock.ms);bridge.ownerPoll(true);};
    auto result=[&](Token t){M::Result last=M::Result::invalid;while(bridge.take(t,e))if(e.kind==Kind::maintenance_result)last=e.maintenance_result;return last;};
    send(a,"{\"v\":1,\"id\":1,\"type\":\"maintenance.request\",\"action\":\"power_off\"}");assert(result(a)==M::Result::unauthorized&&!authority.active());
    send(a,"{\"v\":1,\"id\":2,\"type\":\"maintenance.get\"}");drain(a);
    send(peer,"{\"v\":1,\"id\":1,\"type\":\"maintenance.get\"}");drain(peer);
    send(a,"{\"v\":1,\"id\":3,\"type\":\"maintenance.request\",\"action\":\"power_off\"}");assert(result(a)==M::Result::accepted);
    assert(bridge.take(peer,e)&&e.kind==Kind::maintenance_update&&e.maintenance.remaining_ms==5000);assert(!bridge.take(legacy,e));
    send(a,"{\"v\":1,\"id\":4,\"type\":\"maintenance.request\",\"action\":\"restart\"}");assert(result(a)==M::Result::busy);
    send(peer,"{\"v\":1,\"id\":2,\"type\":\"maintenance.cancel\",\"request\":3}");assert(result(peer)==M::Result::unauthorized);
    send(peer,"{\"v\":1,\"id\":3,\"type\":\"channel.set\",\"channel\":1,\"value\":99}");dispatcher.process();assert(controller.channel(1)==99);drain(a);drain(peer);
    const auto revision=controller.revision();lock.ms=5999;
    send(a,"{\"v\":1,\"id\":5,\"type\":\"maintenance.cancel\",\"request\":3}");assert(result(a)==M::Result::cancelled);
    assert(controller.revision()==revision&&controller.channel(1)==99);
    lock.ms=6000;send(a,"{\"v\":1,\"id\":6,\"type\":\"maintenance.request\",\"action\":\"power_off\"}");result(a);drain(peer);
    bridge.close(a);auto fresh=open();assert(fresh.generation!=a.generation);
    send(fresh,"{\"v\":1,\"id\":1,\"type\":\"maintenance.get\"}");drain(fresh);
    authority.poll(10999,10999000,true);assert(authority.snapshot().phase==M::Phase::countdown);
    authority.poll(11000,11000000,true);assert(authority.snapshot().phase==M::Phase::executing);
    lock.ms=11000;send(fresh,"{\"v\":1,\"id\":2,\"type\":\"maintenance.request\",\"action\":\"power_off\"}");assert(result(fresh)==M::Result::busy);
    send(peer,"{\"v\":1,\"id\":4,\"type\":\"channel.set\",\"channel\":1,\"value\":12}");dispatcher.process();assert(controller.channel(1)==99);
    authority.poll(11001,11001000,true);assert(hw.executions==1&&hw.zeros==0&&hw.retired==0);
    assert(storage.saves==0&&storage.metadata_writes==0&&controller.workingPacket()[0]==0);
    for(const char* bad:{"{\"v\":1,\"id\":9,\"type\":\"maintenance.approve\"}",
        "{\"v\":1,\"id\":9,\"type\":\"maintenance.request\",\"action\":\"erase\"}",
        "{\"v\":1,\"id\":9,\"type\":\"maintenance.cancel\",\"request\":0}"}){
        Request request;const char* error;assert(!decode(bad,strlen(bad),request,error));
    }
    Event encoded;encoded.kind=Kind::maintenance_result;encoded.id=1;encoded.sequence=encoded.revision=1;
    char json[512];assert(encode(encoded,json,sizeof(json))&&strstr(json,"remaining_ms"));
    puts("PASS five-second boundaries/wrap/cancel, duplicate/conflict, reconnect-independent deadline, normal countdown mutations, original Power Off dispatch, Restart safety, strict protocol and NVS preservation");
}
