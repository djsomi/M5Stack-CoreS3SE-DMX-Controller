// Actual owner-task Bridge/settings admission: no device, NVS or radio access.
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace websocket_bridge;
struct SettingsBackend:DeviceSettingsBackend {
 unsigned writes=0;uint8_t saved[3]{3,3,2};
 bool read(DeviceSetting k,uint8_t& v)override{v=saved[unsigned(k)];return true;}
 bool write(DeviceSetting k,uint8_t v)override{++writes;saved[unsigned(k)]=v;return true;}
 void apply(DeviceSetting,uint8_t)override{}
};
struct MaintenanceBackend:DeviceMaintenance::Backend {
 DeviceSettings& settings;bool conflict=false;
 explicit MaintenanceBackend(DeviceSettings& s):settings(s){}
 bool busy()override{return settings.persistencePending()||conflict;}
 void begin(bool)override{}DeviceMaintenance::Step zeroFrame(int64_t)override{return DeviceMaintenance::Step::ready;}
 DeviceMaintenance::Step quiesce(int64_t)override{return DeviceMaintenance::Step::ready;}
 void execute(DeviceMaintenance::Action)override{assert(false);}
};
struct TestLock:Lock{void enter()override{}void leave()override{}uint32_t nowMs()const override{return 100;}};
int main(){
 for(unsigned key=0;key<3;++key)for(unsigned action=0;action<2;++action)for(unsigned scenario=0;scenario<4;++scenario){
  SettingsBackend sb;DeviceSettings s(sb);s.begin();MaintenanceBackend mb(s);DeviceMaintenance m(mb);
  TestLock lock;FakeStorage storage;DmxController controller(storage);CommandDispatcher dispatcher(controller);
  dispatcher.setSenderActive(true);Bridge bridge(controller,dispatcher,lock,&s,&m);Event e;
  auto drain=[&](Token t){while(bridge.take(t,e)){};};
  auto a=bridge.open(100);bridge.ownerPoll(true);drain(a);auto peer=bridge.open(100);bridge.ownerPoll(true);drain(peer);
  auto send=[&](Token t,const char* text){bridge.receive(t,text,strlen(text),100);bridge.ownerPoll(true);};
  send(a,"{\"v\":1,\"id\":1,\"type\":\"maintenance.get\"}");drain(a);
  send(peer,"{\"v\":1,\"id\":1,\"type\":\"settings.request\"}");drain(peer);
  const auto k=DeviceSetting(key);const auto entry=s.value(k);const uint8_t next=entry==3?4:3;
  auto rev=s.snapshot().revision;
  assert(s.edit(DeviceSettings::Edit::preview,peer.generation,9,k,next,rev,100,true)==DeviceSettings::Result::applied);
  assert(s.previewOwner()==peer.generation&&sb.writes==0);
  if(scenario==1){assert(s.edit(DeviceSettings::Edit::commit,peer.generation,9,k,next,s.snapshot().revision,100)==DeviceSettings::Result::applied);assert(sb.writes==1);}
  if(scenario==2)mb.conflict=true;
  if(scenario==3){s.set(k,next,false);assert(s.persistencePending());} // Physical release must still persist first.
  rev=s.snapshot().revision;
  char request[128];snprintf(request,sizeof(request),"{\"v\":1,\"id\":2,\"type\":\"maintenance.request\",\"action\":\"%s\"}",action?"power_off":"restart");
  send(a,request);bool accepted=false;while(bridge.take(a,e))if(e.kind==Kind::maintenance_result)accepted=e.maintenance_result==DeviceMaintenance::Result::accepted;
  assert(accepted==(scenario<2));
  if(scenario<2){
   assert(!s.previewOwner()&&m.snapshot().remaining_ms==5000);
   assert(s.value(k)==(scenario==1?next:entry));assert(sb.writes==(scenario==1?1U:0U));
   bool update=false;uint8_t last=0;uint64_t revision=0;while(bridge.take(peer,e))if(e.kind==Kind::settings_update){update=true;assert(e.revision>revision);revision=e.revision;last=e.values[key];}
   assert(update&&last==s.value(k)); // ownerPoll publishes commit or explicit cancellation to all subscribers.
   assert(s.edit(DeviceSettings::Edit::commit,peer.generation,9,k,next,s.snapshot().revision,101)==DeviceSettings::Result::conflict);
   assert(m.cancelPhysical(101)==DeviceMaintenance::Result::cancelled);
  }else{assert(s.snapshot().revision==rev);if(scenario==2)assert(s.previewOwner()==peer.generation);}
 }
 puts("PASS actual Bridge: both maintenance actions/three settings; peer preview cancellation without NVS, committed value retained, stale commit rejected, real conflicts and pending physical writes preserved");
}
