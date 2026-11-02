#include "wifi_configuration.h"
#include "wifi_credential_transaction.h"
#include "wifi_wire.h"
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
using W=WifiConfiguration;
struct Backend:W::Backend {
    softap_policy::Credentials credentials{},candidate{};
    bool online=true,ready=true;unsigned begins=0;W::Work result=W::Work::pending;
    Backend(){uint8_t mac[6]{};softap_policy::factory(mac,credentials);}
    bool available()override{return ready;}
    bool connected(uint32_t owner)override{return online&&owner==1;}
    void current(softap_policy::Credentials& c)override{c=credentials;}
    bool begin(const softap_policy::Credentials& c)override{++begins;candidate=c;return true;}
    W::Work progress()override{if(result==W::Work::done)credentials=candidate;return result;}
    void published()override{}
};
struct Store:wifi_credentials::Store {
    std::map<std::string,std::vector<uint8_t>> records;
    unsigned calls=0,fail=0;bool durable_failure=false;
    Read read(const char* key,void* out,size_t cap,size_t& size)override {
        auto i=records.find(key);if(i==records.end())return Read::missing;
        size=i->second.size();if(size>cap)return Read::error;memcpy(out,i->second.data(),size);return Read::found;
    }
    bool write(const char* key,const void* bytes,size_t size)override {
        const bool bad=++calls==fail;
        if(!bad||durable_failure){const auto* p=static_cast<const uint8_t*>(bytes);records[key]={p,p+size};}
        return !bad;
    }
    bool remove(const char* key)override {if(++calls==fail)return false;records.erase(key);return true;}
};
struct TestLock:websocket_bridge::Lock {uint32_t now=0;void enter()override{}void leave()override{}uint32_t nowMs()const override{return now;}};
struct Radio:wifi_credentials::Radio {
    unsigned step=0,fail=0,applies=0,commits=0;bool permanent=false;
    bool operation(){++step;return step!=fail&&!(permanent&&step>=fail);}
    bool notifyAndRetire()override{return operation();}
    bool apply(const softap_policy::Credentials&)override{++applies;return operation();}
    bool ready()override{return operation();}
    void committed(const softap_policy::Credentials&)override{++commits;}
};
int main(){
    for(uint32_t start:{0U,0xfffffff0U}) {
        Backend b;W w(b);w.poll(start);const auto original=b.credentials;
        assert(w.propose("Edited AP","",2,1,1,start)==W::Result::conflict);
        assert(w.propose("","",1,1,1,start)==W::Result::invalid);
        assert(w.propose("Edited AP","short",1,1,1,start)==W::Result::invalid);
        assert(w.propose("Edited AP","",1,1,1,start)==W::Result::accepted);
        assert(b.begins==0&&w.snapshot().phase==W::Phase::approval);
        assert(w.propose("Competing AP","",1,1,2,start)==W::Result::busy);
        assert(w.cancel(2,1)==W::Result::unauthorized);
        w.poll(start+59999);assert(w.snapshot().remaining_ms==1000&&b.begins==0);
        w.poll(start+60000);assert(!w.active()&&w.snapshot().reason==W::Reason::expired&&b.begins==0);
        assert(!w.approve(start+60001));
        w.propose("Edited AP","",1,1,2,start);w.cancelPhysical();assert(!w.active()&&b.begins==0);
        w.propose("Edited AP","",1,1,3,start);b.online=false;w.poll(start+1);
        assert(!w.active()&&w.snapshot().reason==W::Reason::disconnected&&b.begins==0);
        b.online=true;w.propose("Edited AP","",1,1,4,start);b.online=false;
        assert(!w.approve(start+1)&&b.begins==0); // Recheck at the actual physical press.
        b.online=true;w.propose("Edited AP","",1,1,5,start);assert(w.approve(start+59999));
        assert(b.begins==1&&!strcmp(b.candidate.pass,original.pass)&&b.candidate.origin==softap_policy::Origin::custom);
        assert(w.cancel(1,5)==W::Result::unauthorized);b.online=false;w.poll(start+60001);assert(w.applying());
        b.result=W::Work::done;w.poll(start+60002);assert(!w.active()&&w.snapshot().base==2&&!strcmp(w.snapshot().ssid,"Edited AP"));
        b.online=true;assert(w.propose("Edited AP","",2,1,6,start)==W::Result::no_change);
    }
    uint8_t mac[6]{};Backend b;auto candidate=b.credentials;candidate.origin=softap_policy::Origin::custom;strcpy(candidate.ssid,"Edited AP");
    {Backend unavailable;W authority(unavailable);authority.poll(0);authority.propose("Edited AP","",1,1,1,0);unavailable.ready=false;
     assert(!authority.approve(1)&&authority.snapshot().phase==W::Phase::failed&&unavailable.begins==0);}
    for(bool legacy:{false,true}) {
        Store initial;softap_policy::LegacyCredentials old{};old.version=1;strcpy(old.ssid,"Legacy AP");strcpy(old.pass,"Synthetic test key");
        assert(initial.write("ap1",legacy?static_cast<void*>(&old):static_cast<void*>(&b.credentials),legacy?sizeof(old):sizeof(b.credentials)));
        const auto before=initial.records["ap1"];
        wifi_credentials::Journal j;
        Store normal=initial;assert(wifi_credentials::stage(normal,candidate,mac,j));assert(normal.records["ap1"]==before);
        assert(wifi_credentials::promote(normal,j));assert(!normal.records.count("ap_try"));
        softap_policy::Credentials restored{};assert(softap_policy::select(normal.records["ap1"].data(),normal.records["ap1"].size(),true,false,mac,restored)==softap_policy::Decision::keep);
        assert(restored.origin==softap_policy::Origin::custom);
        for(unsigned cut=0;cut<3;++cut) {
            Store crash=initial;assert(wifi_credentials::stage(crash,candidate,mac,j));
            if(cut)assert(crash.write("ap1",&candidate,sizeof(candidate)));
            if(cut==2)assert(crash.remove("ap_try"));
            assert(wifi_credentials::recover(crash,mac));
            assert(cut==2?crash.records["ap1"]!=before:crash.records["ap1"]==before);
        }
        for(bool durable:{false,true})for(unsigned failure=1;failure<=3;++failure) {
            Store fault=initial;fault.calls=0;fault.fail=failure;fault.durable_failure=durable;
            const bool staged=wifi_credentials::stage(fault,candidate,mac,j);
            if(staged)wifi_credentials::promote(fault,j);
            fault.fail=0;assert(wifi_credentials::recover(fault,mac));
            const auto& bytes=fault.records["ap1"];assert(softap_policy::select(bytes.data(),bytes.size(),true,false,mac,restored)==softap_policy::Decision::keep);
        }
        Store radio=initial;assert(wifi_credentials::stage(radio,candidate,mac,j));assert(wifi_credentials::rollback(radio,j));assert(radio.records["ap1"]==before);
        for(unsigned failure=0;failure<=3;++failure) {
            Store transaction=initial;Radio device;device.fail=failure;
            const auto outcome=wifi_credentials::execute(transaction,device,candidate,mac);
            assert(outcome==(failure?W::Work::radio_error:W::Work::done));
            assert(device.commits==(failure?0U:1U));
            assert(failure?transaction.records["ap1"]==before:transaction.records["ap1"]!=before);
            assert(!transaction.records.count("ap_try"));
        }
        {Store transaction=initial;Radio device;device.fail=2;device.permanent=true;
         assert(wifi_credentials::execute(transaction,device,candidate,mac)==W::Work::rollback_error&&device.commits==0);}
        for(bool durable:{false,true})for(unsigned failure=1;failure<=3;++failure) {
            Store transaction=initial;transaction.calls=0;transaction.fail=failure;transaction.durable_failure=durable;Radio device;
            const auto outcome=wifi_credentials::execute(transaction,device,candidate,mac);
            assert(outcome!=W::Work::done&&device.commits==0);
            assert(transaction.records["ap1"]==before&&!transaction.records.count("ap_try"));
        }
        Store corrupt=initial;assert(wifi_credentials::stage(corrupt,candidate,mac,j));corrupt.records["ap_try"][20]^=1;
        assert(!wifi_credentials::recover(corrupt,mac)&&corrupt.records["ap1"]==before);
        assert(wifi_credentials::eraseVerified(corrupt,"ap_try")); // Explicit local reset invalidates only Wi-Fi journal.
    }
    char decoded[65];assert(wifi_wire::decode("",decoded,sizeof(decoded))&&!decoded[0]);
    assert(wifi_wire::decode("XCJBUA==",decoded,sizeof(decoded))&&!strcmp(decoded,"\\\"AP"));
    for(const char* text:{"A","AB==","AA==","AAAA","====","YWJj=AAA","YQ=A"})assert(!wifi_wire::decode(text,decoded,sizeof(decoded)));
    using namespace websocket_bridge;
    FakeStorage storage;DmxController controller(storage);CommandDispatcher dispatcher(controller);dispatcher.setSenderActive(true);
    Backend hw;W authority(hw);TestLock lock;Bridge bridge(controller,dispatcher,lock,nullptr,nullptr,&authority);
    const auto token=bridge.open(0);bridge.ownerPoll(true);Event event;while(bridge.take(token,event)){}
    const auto send=[&](const char* text){bridge.receive(token,text,strlen(text),lock.now);bridge.ownerPoll(true);};
    send("{\"v\":1,\"id\":1,\"type\":\"wifi.get\"}");while(bridge.take(token,event)){}
    send("{\"v\":1,\"id\":2,\"type\":\"wifi.propose\",\"base\":\"1\",\"ssid\":\"RWRpdGVkIEFQ\",\"password\":\"\"}");
    assert(authority.active()&&hw.begins==0);
    char output[2560];while(bridge.take(token,event)) {
        assert(encode(event,output,sizeof(output)));assert(!strstr(output,hw.credentials.pass));
        assert(!strstr(output,"\"password\":"));
    }
    assert(authority.approve(0));assert(hw.begins==1);bridge.close(token);authority.poll(1);assert(authority.applying());
    assert(storage.saves==0&&storage.erases==0&&controller.revision()==0);
    puts("PASS Wi-Fi approval/CANCEL/60s/wrap/disconnect/revision/conflict; V1/V2 journal power cuts, durable failures, rollback/reset; strict Base64, real Bridge and no outbound password/NVS domain writes");
}
