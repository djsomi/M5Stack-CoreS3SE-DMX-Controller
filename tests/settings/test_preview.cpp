#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
using namespace websocket_bridge;
using Edit=DeviceSettings::Edit;using Result=DeviceSettings::Result;
struct Backend:DeviceSettingsBackend {
    uint8_t saved[3]{3,3,2},hardware[3]{};unsigned writes=0;bool success=true;
    bool read(DeviceSetting k,uint8_t& v)override{v=saved[unsigned(k)];return true;}
    bool write(DeviceSetting k,uint8_t v)override{++writes;if(success)saved[unsigned(k)]=v;return success;}
    void apply(DeviceSetting k,uint8_t v)override{hardware[unsigned(k)]=v;}
};
struct TestLock:Lock {uint32_t now=0;void enter()override{}void leave()override{}uint32_t nowMs()const override{return now;}};
int main(){
    for(unsigned i=0;i<3;++i){
        Backend b;DeviceSettings s(b);s.begin();const auto k=DeviceSetting(i);
        unsigned time=0;bool first=true;
        for(unsigned v=i?0:1;v<=(i?4:5);++v){
            const auto r=s.edit(Edit::preview,1,7,k,v,s.snapshot().revision,++time,first);first=false;
            assert(r==Result::applied||r==Result::no_change);
            assert(b.hardware[i]==v&&s.value(k)==v&&b.writes==0);
            s.commitPending();assert(b.writes==0); // Physical idle must not save web previews.
        }
        assert(s.edit(Edit::commit,1,7,k,0,s.snapshot().revision,++time)==Result::applied);
        assert(b.writes==1&&s.previewOwner()==0&&s.snapshot().persisted==7);
        assert(s.edit(Edit::commit,1,7,k,0,s.snapshot().revision,++time)==Result::conflict&&b.writes==1);
        DeviceSettings reboot(b);reboot.begin();assert(reboot.value(k)==s.value(k));
    }
    Backend b;DeviceSettings s(b);s.begin();const auto k=DeviceSetting::brightness;
    auto preview=[&](uint32_t owner,uint32_t id,unsigned value,uint32_t now,bool first){return s.edit(Edit::preview,owner,id,k,value,s.snapshot().revision,now,first);};
    preview(1,1,3,1,true);s.edit(Edit::commit,1,1,k,0,s.snapshot().revision,2);assert(b.writes==0);
    preview(1,2,5,3,true);assert(s.value(k)==5&&!(s.snapshot().persisted&1));
    assert(s.edit(Edit::cancel,2,2,k,0,s.snapshot().revision,4)==Result::conflict&&s.value(k)==5);
    s.edit(Edit::cancel,1,2,k,0,1,4);assert(s.value(k)==3&&s.snapshot().persisted==7&&b.writes==0);
    preview(1,3,5,5,true);preview(2,4,1,6,true); // Explicit fresh-revision peer intent takes over.
    assert(s.edit(Edit::cancel,1,3,k,0,s.snapshot().revision,7)==Result::conflict&&s.value(k)==1);
    s.edit(Edit::cancel,2,4,k,0,s.snapshot().revision,8);assert(s.value(k)==3);
    preview(1,5,5,9,true);s.set(DeviceSetting::volume,0,false);
    assert(s.value(k)==3&&s.value(DeviceSetting::volume)==0&&s.previewOwner()==0);
    assert(s.edit(Edit::commit,1,5,k,0,s.snapshot().revision,10)==Result::conflict);
    assert(preview(1,6,4,11,true)==Result::conflict); // Physical drag still pending.
    s.commitPending();assert(b.writes==1&&b.saved[0]==3&&b.saved[2]==0);
    preview(1,7,5,12,true);s.set(k,4);assert(s.value(k)==4&&b.saved[0]==4&&s.previewOwner()==0);
    assert(s.edit(Edit::commit,1,7,k,0,s.snapshot().revision,13)==Result::conflict);
    preview(1,8,1,20,true);s.servicePreview(10019);assert(s.value(k)==1);
    s.servicePreview(10020);assert(s.value(k)==4&&s.previewOwner()==0);
    preview(1,9,1,20000,true);
    for(unsigned t=25000;t<50000;t+=5000)preview(1,9,t%2?1:2,t,false);
    s.servicePreview(50000);assert(s.value(k)==4&&s.previewOwner()==0); // Absolute bound.
    preview(1,10,1,UINT32_MAX-50,true);s.servicePreview(9949);assert(s.value(k)==4); // Wrap-safe 10 s.
    preview(1,11,1,60000,true);s.servicePreview(60001,false);assert(s.value(k)==4); // Owner closed.
    preview(1,12,5,60002,true);b.success=false;
    assert(s.edit(Edit::commit,1,12,k,0,s.snapshot().revision,60003)==Result::storage_error);
    assert(s.value(k)==5&&b.hardware[0]==5&&!(s.snapshot().persisted&1)&&s.previewOwner()==0);
    const auto writes=b.writes;s.servicePreview(999999);assert(b.writes==writes&&s.value(k)==5);
    const auto revision=s.snapshot().revision;
    assert(s.edit(Edit::preview,1,13,k,256,revision,1,true)==Result::invalid);
    assert(s.edit(Edit(4),1,13,k,0,revision,1,true)==Result::invalid);
    assert(s.edit(Edit::preview,1,13,k,1,revision-1,1,true)==Result::conflict);
    assert(s.snapshot().revision==revision);

    TestLock lock;Backend hardware;DeviceSettings settings(hardware);settings.begin();
    FakeStorage storage;DmxController controller(storage);CommandDispatcher dispatcher(controller);Bridge bridge(controller,dispatcher,lock,&settings);
    auto token=bridge.open(0);bridge.ownerPoll(false);Event e;while(bridge.take(token,e)){}
    const char* read="{\"v\":1,\"id\":1,\"type\":\"settings.request\"}";
    bridge.receive(token,read,strlen(read),0);bridge.ownerPoll(false);while(bridge.take(token,e)){}
    const char* start="{\"v\":1,\"id\":2,\"type\":\"settings.preview\",\"setting\":\"brightness\",\"value\":5,\"gesture\":2,\"base\":\"1\"}";
    bridge.receive(token,start,strlen(start),0);assert(hardware.hardware[0]==3);bridge.ownerPoll(false);
    assert(hardware.hardware[0]==5&&hardware.writes==0);
    assert(bridge.take(token,e)&&e.kind==Kind::settings_update);
    assert(bridge.take(token,e)&&e.settings_edit==Edit::preview&&e.settings_gesture==2);
    char json[512];assert(encode(e,json,sizeof(json))&&strstr(json,"\"phase\":\"preview\""));
    bridge.close(token);bridge.ownerPoll(false);assert(hardware.hardware[0]==3&&hardware.writes==0);
    Request request;const char* error;
    const char* good="{\"v\":1,\"id\":8,\"type\":\"settings.commit\",\"setting\":\"volume\",\"gesture\":7,\"base\":\"18446744073709551615\"}";
    assert(decode(good,strlen(good),request,error)&&request.settings_base==UINT64_MAX);
    for(const char* bad:{
        "{\"v\":1,\"id\":1,\"type\":\"settings.preview\",\"setting\":\"volume\",\"value\":5,\"gesture\":1,\"base\":\"1\"}",
        "{\"v\":1,\"id\":1,\"type\":\"settings.preview\",\"setting\":\"volume\",\"value\":1,\"gesture\":2,\"base\":\"1\"}",
        "{\"v\":1,\"id\":1,\"type\":\"settings.commit\",\"setting\":\"volume\",\"gesture\":1,\"base\":\"18446744073709551616\"}",
        "{\"v\":1,\"id\":1,\"type\":\"settings.commit\",\"setting\":\"volume\",\"gesture\":1,\"base\":\"01\"}",
        "{\"v\":1,\"id\":1,\"type\":\"settings.commit\",\"setting\":\"volume\",\"gesture\":1,\"base\":\"1\",\"value\":1}",
        "{\"v\":1,\"id\":1,\"type\":\"settings.cancel\",\"setting\":\"volume\",\"gesture\":0,\"base\":\"1\"}"}) {
        Request invalid;assert(!decode(bad,strlen(bad),invalid,error));
    }
    assert(controller.revision()==0);
    puts("PASS live previews: all levels, zero intermediate writes, one commit, cancellation, peer/physical supersession, stale base, timeout/wrap, failed save, closed owner and strict protocol");
}
