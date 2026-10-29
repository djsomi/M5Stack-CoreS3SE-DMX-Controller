#include "device_settings.h"
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
using namespace websocket_bridge;
struct TestLock : Lock {int depth=0;void enter()override{++depth;}void leave()override{--depth;}};
struct Backend : DeviceSettingsBackend {
    uint8_t saved[3]{3,3,2};bool present[3]{true,true,true},success=true;
    unsigned writes=0,applies=0;TestLock* lock=nullptr;
    bool read(DeviceSetting k,uint8_t& v)override{v=saved[unsigned(k)];return present[unsigned(k)];}
    bool write(DeviceSetting k,uint8_t v)override{assert(!lock||lock->depth==0);++writes;if(success)saved[unsigned(k)]=v;return success;}
    void apply(DeviceSetting,uint8_t)override{assert(!lock||lock->depth==0);++applies;}
};
int main(){
    for(unsigned k=0;k<3;++k){
        Backend b;DeviceSettings s(b);s.begin();assert(s.snapshot().persisted==7);
        for(unsigned v=k?0:1;v<=(k?4:5);++v){
            const unsigned before=b.writes;const bool changed=s.value(DeviceSetting(k))!=v;
            auto r=s.set(DeviceSetting(k),v);assert(r==(changed?DeviceSettings::Result::applied:DeviceSettings::Result::no_change));
            assert(s.value(DeviceSetting(k))==v&&b.writes==before+(changed?1:0));
            assert(s.set(DeviceSetting(k),v)==DeviceSettings::Result::no_change&&b.writes==before+(changed?1:0));
        }
        const auto revision=s.snapshot().revision;
        assert(s.set(DeviceSetting(k),256)==DeviceSettings::Result::invalid);
        assert(s.set(DeviceSetting(3),1)==DeviceSettings::Result::invalid);
        if(!k)assert(s.set(DeviceSetting(k),0)==DeviceSettings::Result::invalid);
        assert(s.snapshot().revision==revision);
    }
    Backend b;DeviceSettings s(b);s.begin();
    for(unsigned v:{1U,2U,4U,5U})s.set(DeviceSetting::brightness,v,false);
    assert(b.writes==0&&s.value(DeviceSetting::brightness)==5);s.commitPending();assert(b.writes==1&&b.saved[0]==5);
    s.commitPending();assert(b.writes==1);
    b.success=false;assert(s.set(DeviceSetting::volume,4)==DeviceSettings::Result::storage_error);
    assert(s.value(DeviceSetting::volume)==4&&!(s.snapshot().persisted&4));
    const auto writes=b.writes;s.commitPending();s.set(DeviceSetting::volume,4);assert(b.writes==writes);
    // A write may reach flash before confirmation fails: never trust old cache.
    b.saved[2]=4;b.success=true;s.set(DeviceSetting::volume,2);
    assert(b.writes==writes+1&&b.saved[2]==2&&(s.snapshot().persisted&4));
    Backend startup;startup.saved[0]=0;startup.saved[1]=9;startup.saved[2]=5;
    DeviceSettings legacy(startup);legacy.begin();assert(legacy.value(DeviceSetting::brightness)==3&&legacy.value(DeviceSetting::timeout)==3&&legacy.value(DeviceSetting::volume)==4);
    assert(startup.writes==0&&legacy.snapshot().persisted==0);
    Backend absent;for(auto& p:absent.present)p=false;DeviceSettings defaults(absent);defaults.begin();assert(defaults.snapshot().persisted==0&&defaults.value(DeviceSetting::volume)==2);
    TestLock lock;Backend settings_backend;settings_backend.lock=&lock;DeviceSettings settings(settings_backend);settings.begin();
    FakeStorage storage;DmxController controller(storage);CommandDispatcher dispatcher(controller);Bridge bridge(controller,dispatcher,lock,&settings);
    auto open=[&](){auto t=bridge.open(1000);bridge.ownerPoll(false);Event e;assert(bridge.take(t,e)&&e.kind==Kind::hello);assert(bridge.take(t,e)&&e.kind==Kind::snapshot);return t;};
    auto old=open(),a=open(),c=open();Event e;
    auto send=[&](Token t,const char* text){bridge.receive(t,text,strlen(text),1000);bridge.ownerPoll(false);};
    send(a,"{\"v\":1,\"id\":1,\"type\":\"settings.request\"}");assert(bridge.take(a,e)&&e.kind==Kind::settings_snapshot&&e.values[0]==3);
    send(c,"{\"v\":1,\"id\":1,\"type\":\"settings.request\"}");assert(bridge.take(c,e)&&e.kind==Kind::settings_snapshot);
    send(a,"{\"v\":1,\"id\":2,\"type\":\"settings.set\",\"setting\":\"brightness\",\"value\":4}");
    assert(bridge.take(a,e)&&e.kind==Kind::settings_update&&e.values[0]==4);
    assert(bridge.take(a,e)&&e.kind==Kind::settings_result&&e.settings_result==DeviceSettings::Result::applied&&e.domain==7);
    assert(bridge.take(c,e)&&e.kind==Kind::settings_update&&e.values[0]==4);assert(!bridge.take(old,e));
    settings.set(DeviceSetting::timeout,0,false);settings.commitPending();bridge.ownerPoll(false);
    assert(bridge.take(a,e)&&e.kind==Kind::settings_update&&e.values[1]==0);assert(bridge.take(c,e)&&e.kind==Kind::settings_update);
    settings_backend.success=false;send(c,"{\"v\":1,\"id\":2,\"type\":\"settings.set\",\"setting\":\"volume\",\"value\":0}");
    assert(bridge.take(c,e)&&e.kind==Kind::settings_update&&e.values[2]==0&&!(e.domain&4));
    assert(bridge.take(c,e)&&e.kind==Kind::settings_result&&e.settings_result==DeviceSettings::Result::storage_error);
    char encoded[512];assert(encode(e,encoded,sizeof(encoded))&&strstr(encoded,"storage_error"));
    bridge.close(c);auto reconnected=open();send(reconnected,"{\"v\":1,\"id\":1,\"type\":\"settings.request\"}");
    assert(bridge.take(reconnected,e)&&e.kind==Kind::settings_snapshot&&e.values[0]==4&&e.values[1]==0&&e.values[2]==0);
    while(bridge.take(a,e)){}
    settings_backend.success=true;
    const char* first="{\"v\":1,\"id\":3,\"type\":\"settings.set\",\"setting\":\"brightness\",\"value\":1}";
    const char* second="{\"v\":1,\"id\":2,\"type\":\"settings.set\",\"setting\":\"brightness\",\"value\":5}";
    bridge.receive(a,first,strlen(first),2000);bridge.receive(reconnected,second,strlen(second),2000);
    assert(settings.value(DeviceSetting::brightness)==4); // HTTP-side enqueue cannot apply.
    bridge.ownerPoll(false);assert(settings.value(DeviceSetting::brightness)==1);
    bridge.ownerPoll(false);assert(settings.value(DeviceSetting::brightness)==5); // FIFO owner order.
    uint64_t latest=0;while(bridge.take(a,e))if(e.kind==Kind::settings_update){assert(e.revision>latest);latest=e.revision;}
    assert(latest==settings.snapshot().revision);
    const auto current_writes=settings_backend.writes;
    bridge.receive(c,first,strlen(first),2000);bridge.ownerPoll(false); // Dead generation cannot mutate.
    assert(settings_backend.writes==current_writes);
    DeviceSettings restored(settings_backend);restored.begin();
    assert(restored.value(DeviceSetting::brightness)==5&&restored.value(DeviceSetting::timeout)==0);
    Request request;const char* error;
    for(const char* bad:{"{\"v\":1,\"id\":3,\"type\":\"settings.set\",\"setting\":\"password\",\"value\":1}",
        "{\"v\":1,\"id\":3,\"type\":\"settings.set\",\"setting\":\"brightness\",\"value\":0}",
        "{\"v\":1,\"id\":3,\"type\":\"settings.set\",\"setting\":\"volume\",\"value\":5}",
        "{\"v\":1,\"id\":3,\"type\":\"settings.request\",\"setting\":\"volume\"}"})assert(!decode(bad,strlen(bad),request,error));
    assert(controller.revision()==0); // Settings never mutate the DMX domain, even in Receiver.
    puts("PASS settings bounds/defaults/restoration, unchanged writes, release-only persistence, errors, receiver operation, legacy isolation, two-client ordering and reconnect");
}
