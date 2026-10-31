// Real pinned IDF NVS; only NOR flash, RTOS locking and Arduino wrappers are
// host adapters. No device, filesystem persistence, erase command or mock KV map.
#include "preset_store.h"
#include "websocket_bridge.h"
#include "nvs_flash.h"
#include "nvs_partition_manager.hpp"
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
static void check(bool yes,const char* message){if(!yes){fprintf(stderr,"FAIL %s\n",message);exit(42);}}
struct Cut{};
struct Flash {
 std::vector<uint8_t> data=std::vector<uint8_t>(0x5000,255);
 unsigned writes=0,cut=0,fail=0;bool after=false,torn=false;
 void before(){++writes;if(cut==writes&&!after&&!torn)throw Cut{};}
 void done(){if(cut==writes&&after)throw Cut{};}
} flash;
esp_partition_t test_partition{0x9000,0x5000,"nvs",false};
const esp_partition_t* esp_partition_find_first(int,int,const char* name){return !name||!strcmp(name,"nvs")?&test_partition:nullptr;}
int esp_partition_read(const esp_partition_t*,size_t at,void* out,size_t size){if(at+size>flash.data.size())return ESP_ERR_INVALID_ARG;memcpy(out,flash.data.data()+at,size);return 0;}
int esp_partition_read_raw(const esp_partition_t* p,size_t at,void* out,size_t size){return esp_partition_read(p,at,out,size);}
int esp_partition_write(const esp_partition_t*,size_t at,const void* src,size_t size){
 if(at+size>flash.data.size())return ESP_ERR_INVALID_ARG;flash.before();if(flash.fail==flash.writes&&!flash.after)return ESP_ERR_FLASH_OP_FAIL;
 const bool partial=flash.torn&&flash.cut==flash.writes;if(partial)size=(size+1)/2;
 const auto* p=static_cast<const uint8_t*>(src);for(size_t i=0;i<size;++i){check((flash.data[at+i]&p[i])==p[i],"NOR 0-to-1 write without erase");flash.data[at+i]&=p[i];}
 if(partial)throw Cut{};flash.done();return flash.fail==flash.writes?ESP_ERR_FLASH_OP_FAIL:ESP_OK;
}
int esp_partition_write_raw(const esp_partition_t* p,size_t at,const void* src,size_t size){return esp_partition_write(p,at,src,size);}
int esp_partition_erase_range(const esp_partition_t*,size_t at,size_t size){
 if(at+size>flash.data.size()||at%4096||size%4096)return ESP_ERR_INVALID_ARG;flash.before();if(flash.fail==flash.writes&&!flash.after)return ESP_ERR_FLASH_OP_FAIL;
 const bool partial=flash.torn&&flash.cut==flash.writes;memset(flash.data.data()+at,255,partial?size/2:size);if(partial)throw Cut{};flash.done();return flash.fail==flash.writes?ESP_ERR_FLASH_OP_FAIL:ESP_OK;
}
static bool initialized=false;
void mount(){check(nvs_flash_init_partition_ptr(&test_partition)==ESP_OK,"native NVS mount/recovery");initialized=true;}
void unmount(){if(initialized)check(nvs::NVSPartitionManager::get_instance()->deinit_partition("nvs")==ESP_OK,"native NVS deinit");initialized=false;}
void reboot(){flash.cut=flash.fail=0;flash.torn=false;unmount();mount();}
void reset(){flash.cut=flash.fail=0;flash.torn=false;unmount();flash.data.assign(0x5000,255);mount();}
static const unsigned filler_bytes=3072;
void blob(const char* ns,const char* key,const void* bytes,size_t size){nvs_handle_t h;check(!nvs_open(ns,NVS_READWRITE,&h),"seed namespace");check(!nvs_set_blob(h,key,bytes,size)&&!nvs_commit(h),"seed blob");nvs_close(h);}
void seed(unsigned filler=filler_bytes){
 reset();for(unsigned i=0;i<8;++i){uint8_t bytes[513]{};memset(bytes+1,int(i+1),512);char key[4];snprintf(key,sizeof(key),"p%u",i+1);blob("dmxpreset",key,bytes,sizeof(bytes));}
 std::vector<uint8_t> other(filler,37);blob("unrelated","retained",other.data(),other.size());
 nvs_handle_t h;check(!nvs_open("dmxui",NVS_READWRITE,&h)&&!nvs_set_u8(h,"volume",0)&&!nvs_commit(h),"device settings");nvs_close(h);
 uint8_t credentials[96];memset(credentials,79,sizeof(credentials));blob("dmxwifi","ap1",credentials,sizeof(credentials));
}
void unrelated(){
 nvs_handle_t h;std::vector<uint8_t> bytes(filler_bytes);size_t n=bytes.size();check(!nvs_open("unrelated",NVS_READONLY,&h),"unrelated namespace retained");check(!nvs_get_blob(h,"retained",bytes.data(),&n)&&n==filler_bytes,"unrelated blob retained");nvs_close(h);for(auto v:bytes)check(v==37,"unrelated bytes unchanged");
 check(!nvs_open("dmxui",NVS_READONLY,&h),"settings retained");uint8_t value=99;check(!nvs_get_u8(h,"volume",&value)&&value==0,"volume retained");nvs_close(h);
 check(!nvs_open("dmxwifi",NVS_READONLY,&h),"credentials retained");uint8_t secret[96];n=sizeof(secret);check(!nvs_get_blob(h,"ap1",secret,&n)&&n==sizeof(secret),"credential length");nvs_close(h);for(auto v:secret)check(v==79,"credential bytes unchanged");
}
struct Clock:websocket_bridge::Lock{uint32_t now=1000;bool held=false;void enter()override{check(!held,"no recursive lock");held=true;}void leave()override{check(held,"unlock");held=false;}uint32_t nowMs()const override{return now;}};
struct Physical {
 DmxController& controller;CommandDispatcher& dispatcher;char preset_status[80]{};
 Physical(DmxController& c,CommandDispatcher& d):controller(c),dispatcher(d){}
#include "physical_presets_under_test.h"
};
struct Rig {
 PresetStore storage;DmxController controller{storage};CommandDispatcher dispatcher{controller};Clock clock;websocket_bridge::Bridge bridge{controller,dispatcher,clock};Physical physical{controller,dispatcher};std::vector<char> buffer=std::vector<char>(32768);websocket_bridge::Token a,b;unsigned id=0,published=0;
 Rig(){dispatcher.setSenderActive(true);dispatcher.setPublisher([](void* p,const ControllerCommand&){++*static_cast<unsigned*>(p);return 0;},&published);bridge.archiveBuffer(buffer.data(),buffer.size());bridge.ownerPoll(true);a=open();b=open();}
 websocket_bridge::Token open(){auto t=bridge.open(clock.now);bridge.ownerPoll(true);drain(t);return t;}
 void drain(websocket_bridge::Token t){websocket_bridge::Event e;while(bridge.take(t,e)){};}
 websocket_bridge::Event request(const std::string& fields,websocket_bridge::Kind expected=websocket_bridge::Kind::archive_result){
  auto data="{\"v\":1,\"id\":"+std::to_string(++id)+","+fields+"}";bridge.receive(a,data.data(),data.size(),clock.now);bridge.ownerPoll(true);dispatcher.process();clock.now+=101;websocket_bridge::Event e;bool found=false;
  while(bridge.take(a,e)){if(e.kind==expected&&e.id==id){found=true;break;}}
  check(found,"actual WS response");return e;
 }
 preset_archive::Collection capture(){check(controller.captureArchive()==DmxController::Result::ok,"authoritative collection");return controller.archive();}
 std::string json(){auto c=capture();size_t n=preset_archive::serialize(c,buffer.data(),buffer.size());check(n>0,"serialize");return {buffer.data(),n};}
 std::string exportFile(){auto start=request("\"type\":\"presets.export\"");check(!strcmp(start.reason,"ready"),"export ready");std::string out;while(out.size()<start.archive_total){auto e=request("\"type\":\"presets.read\",\"token\":"+std::to_string(start.archive_token)+",\"offset\":"+std::to_string(out.size()));check(!strcmp(e.reason,"data")&&e.archive_length&&e.archive_length<=512,"optimized export frame");out.append(reinterpret_cast<const char*>(e.values),e.archive_length);}auto done=request("\"type\":\"presets.cancel\",\"token\":"+std::to_string(start.archive_token));check(!strcmp(done.reason,"cancelled"),"export release");return out;}
 websocket_bridge::Event importFile(const std::string& data,bool cancel=false){auto e=request("\"type\":\"presets.import\",\"bytes\":"+std::to_string(data.size()));check(!strcmp(e.reason,"ready"),"import ready");unsigned token=e.archive_token;for(size_t at=0;at<data.size();at+=96){char encoded[129];auto n=std::min(size_t(96),data.size()-at);preset_archive::base64(reinterpret_cast<const uint8_t*>(data.data()+at),n,encoded);e=request("\"type\":\"presets.chunk\",\"token\":"+std::to_string(token)+",\"offset\":"+std::to_string(at)+",\"data\":\""+encoded+"\"");check(!strcmp(e.reason,"staged"),"chunk");}return request(std::string("\"type\":\"presets.")+(cancel?"cancel":"commit")+"\",\"token\":"+std::to_string(token));}
 void web(const char* operation,unsigned slot){auto e=request("\"type\":\"preset."+std::string(operation)+"\",\"slot\":"+std::to_string(slot),websocket_bridge::Kind::ack);check(e.result==CommandStatus::applied||e.result==CommandStatus::no_change,"web preset mutation accepted");}
 void peers(){bridge.ownerPoll(true);bool sync=false;websocket_bridge::Event e;while(bridge.take(b,e))if(e.kind==websocket_bridge::Kind::universe){sync=true;for(unsigned i=0;i<8;++i)check(e.presets[i]==unsigned(controller.presetState(uint8_t(i))),"peer occupancy");}check(sync,"peer preset publication");}
};
bool same(const preset_archive::Collection& a,const preset_archive::Collection& b){if(a.present!=b.present)return false;for(unsigned i=0;i<8;++i)if(a.present&(1U<<i))if(memcmp(a.values[i],b.values[i],512))return false;return true;}
void mutations(Rig& r,const char* phase){
 static unsigned round=0;++round;r.controller.setChannel(1,int(round));for(unsigned slot:{1U,3U})if(!r.controller.presetExists(slot))check(r.physical.savePreset(slot),"prepare occupied delete slots");r.bridge.ownerPoll(true);r.drain(r.a);r.drain(r.b);
 r.controller.setChannel(23,112);r.controller.setBlackout(true);r.controller.setChannel(23,9);uint8_t before[513];memcpy(before,r.controller.workingPacket(),513);unsigned publications=r.published;r.drain(r.b);
 check(r.physical.savePreset(0),phase);check(r.controller.presetExists(0),"touch Save visible");r.peers();r.drain(r.b);
 check(r.physical.clearPreset(1),"touch Delete");check(!r.controller.presetExists(1),"touch Delete visible");r.peers();r.drain(r.b);
 r.web("save",3);r.peers();r.web("clear",4);r.peers();check(!r.controller.presetExists(3),"web Delete visible");
 auto collection=r.capture();check(collection.present&1,"saved slot present");check(collection.values[0][22]==9&&!(collection.present&2),"stored values/absence");
 check(r.published==publications&&!memcmp(before,r.controller.workingPacket(),513)&&r.controller.blackoutActive()&&r.controller.outputPacket()[23]==0,"Save/Delete do not publish or alter live/blackout");
 r.controller.setBlackout(false);check(r.controller.channel(23)==112,"unchanged restore snapshot");
}
void primary(){
 seed();preset_archive::Collection accepted;
 {Rig r;accepted=r.capture();for(unsigned i=0;i<8;++i)check(accepted.values[i][0]==i+1,"v0.2 P1-P8 migration values");auto backup=r.json();auto imported=r.importFile(backup);check(!strcmp(imported.reason,"applied"),"JSON adoption succeeds before capacity regression");}
 reboot(); // No new import/export after this boot: reproduces persistent failure.
 {Rig r;check(same(r.capture(),accepted),"import persists reboot");mutations(r,"fresh boot physical Save after imported bank");auto before=r.json();check(r.exportFile()==before,"latest export includes Save/Delete");mutations(r,"physical Save immediately after export");
  auto cancelled=r.importFile(before,true);check(!strcmp(cancelled.reason,"cancelled"),"cancel import");mutations(r,"Save after cancelled import");auto failed=r.importFile("{bad}");check(!strcmp(failed.reason,"invalid"),"failed validation");mutations(r,"Save after failed import");
  auto backup=r.json();auto e=r.importFile(backup);check(!strcmp(e.reason,"applied"),"repeat import");mutations(r,"Save after another import");
  for(unsigned i=0;i<48;++i){r.controller.setChannel(1,int(50+i));check(r.physical.savePreset(6),"repeated Save");check(r.physical.clearPreset(6),"repeated Delete");r.drain(r.a);r.drain(r.b);}
  accepted=r.capture();unrelated();
  nvs_handle_t h;check(!nvs_open("dmxpreset",NVS_READONLY,&h),"bounded keys");
  for(unsigned i=1;i<=8;++i){char key[4];snprintf(key,sizeof(key),"p%u",i);size_t n=0;check(nvs_get_blob(h,key,nullptr,&n)==ESP_ERR_NVS_NOT_FOUND,"all obsolete legacy blobs reclaimed");}
  for(const char* key:{"xbank0","xbank1"}){size_t n=0;check(!nvs_get_blob(h,key,nullptr,&n)&&n==preset_archive::Store::bytes,"two bounded generations reused");}nvs_close(h);}
 reboot();{Rig r;check(same(r.capture(),accepted),"repeated mutations persist reboot");check(!r.controller.presetExists(6),"deleted slot stays deleted");check(r.controller.loadPreset(0)==DmxController::Result::ok,"explicit load still works");unrelated();nvs_stats_t stats;check(!nvs_get_stats("nvs",&stats),"stats");printf("PASS real NVS primary: fresh-boot physical/web Save/Delete, import/export/cancel/failure, 48 repeated cycles, reboot, peers, DMX/blackout; used=%u free=%u total=%u\n",unsigned(stats.used_entries),unsigned(stats.free_entries),unsigned(stats.total_entries));}
}
bool capture(preset_archive::Collection& out){PresetStore storage;out={};return storage.captureCollection(out)==DmxPresetStorage::SlotState::valid;}
void interrupted(){
 for(bool adopted:{false,true}){
  seed();preset_archive::Collection old;check(capture(old),"initial collection");
  if(adopted){PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::ok,"adopt before interruption");}
  const auto image=flash.data;auto next=old;next.values[0][22]=237;next.present=0x81;
  flash.writes=0;{PresetStore s;check(s.replaceAll(next,{})==preset_archive::Commit::ok,"count flash boundaries");}
  const unsigned writes=flash.writes;
  for(bool after:{false,true})for(unsigned cut=1;cut<=writes;++cut){
   unmount();flash.data=image;mount();flash.writes=0;flash.cut=cut;flash.after=after;
   try{PresetStore s;s.replaceAll(next,{});}catch(Cut&){}
   reboot();preset_archive::Collection recovered;check(capture(recovered),"recover every interrupted cleanup/write");
   check(same(recovered,old)||same(recovered,next),"atomic old-or-new collection, never mixture");unrelated();
   PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::ok,"retry after interrupted staging/cleanup");
  }
  for(unsigned cut=1;cut<=writes;++cut){
   unmount();flash.data=image;mount();flash.writes=0;flash.cut=cut;flash.torn=true;
   try{PresetStore s;s.replaceAll(next,{});}catch(Cut&){}
   reboot();preset_archive::Collection recovered;check(capture(recovered)&&(same(recovered,old)||same(recovered,next)),"torn NOR program/erase recovery");unrelated();
  }
  // Flash API errors before and after programming; no claimed success on an
  // ambiguous selector, and reboot must still choose a complete collection.
  for(bool after:{false,true})for(unsigned fail=1;fail<=writes;++fail){
   unmount();flash.data=image;mount();flash.writes=0;flash.fail=fail;flash.after=after;
   {PresetStore s;s.replaceAll(next,{});}reboot();preset_archive::Collection recovered;
   check(capture(recovered)&&(same(recovered,old)||same(recovered,next)),"flash-error recovery");unrelated();
  }
  printf("PASS real NVS interruptions: adopted=%u flash boundaries=%u, before/after cuts and errors; selected collection and unrelated data retained\n",adopted,writes);
 }
}
void capacity(){
 seed();preset_archive::Collection old;check(capture(old),"capacity baseline");
 {PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::ok,"capacity adoption");uint8_t p[513]{};p[1]=42;check(s.save(0,p),"reclaim before genuine exhaustion");}
 check(capture(old),"capture after reclaim");
 {PresetArchiveKv kv(true);uint8_t active=2;check(kv.selector(active)==preset_archive::Io::ok&&kv.erase(active^1)&&kv.commit(),"remove only inactive test bank before exhaustion");}
 nvs_handle_t h;check(!nvs_open("capacity",NVS_READWRITE,&h),"capacity ns");
 uint8_t extra[128];memset(extra,65,sizeof(extra));unsigned count=0;
 for(;count<100;++count){char key[8];snprintf(key,sizeof(key),"e%u",count);auto rc=nvs_set_blob(h,key,extra,sizeof(extra));if(rc==ESP_ERR_NVS_NOT_ENOUGH_SPACE)break;check(rc==ESP_OK,"capacity filler error");}
 nvs_close(h);check(count<100,"real NVS capacity reached");reboot();
 {Rig r;r.controller.setChannel(23,77);check(!r.physical.savePreset(0),"full NVS Save rejected");check(!strcmp(r.physical.preset_status,"PRESET STORAGE FULL"),"actionable physical no-space");
  auto e=r.request("\"type\":\"preset.clear\",\"slot\":2",websocket_bridge::Kind::error);check(e.result==CommandStatus::storage_error&&e.domain==14,"specific web no-space domain");
  auto data=r.json();auto imported=r.importFile(data);check(!strcmp(imported.reason,"storage_full"),"specific import no-space");check(same(r.capture(),old),"full NVS preserves selected data");unrelated();}
 printf("PASS real NVS genuine exhaustion: %u filler records; physical/web/import actionable failure, old collection retained\n",count);
}
void guards(){
 seed();preset_archive::Collection old;check(capture(old),"guard baseline");
 {PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::ok,"guard adoption");}
 const auto image=flash.data;
 // Read-only Load/Export/capture must not trigger reclamation.
 flash.writes=0;check(capture(old)&&flash.writes==0,"read-only does not reclaim");
 nvs_handle_t h;check(!nvs_open("dmxpreset",NVS_READWRITE,&h),"guard namespace");
 size_t size=0;check(!nvs_get_blob(h,"p1",nullptr,&size)&&size==513,"legacy retained until first bank-backed mutation");
 check(!nvs_set_u8(h,"xactive",7),"corrupt selector");nvs_close(h);flash.writes=0;
 {PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::failed&&flash.writes==0,"invalid selector forbids cleanup");}
 unmount();flash.data=image;mount();check(!nvs_open("dmxpreset",NVS_READWRITE,&h),"CRC guard namespace");
 std::vector<uint8_t> bank(preset_archive::Store::bytes);size=bank.size();check(!nvs_get_blob(h,"xbank0",bank.data(),&size),"read bank");bank[42]^=1;check(!nvs_erase_key(h,"xbank0"),"prepare CRC corruption");check(!nvs_set_blob(h,"xbank0",bank.data(),size),"damage CRC");nvs_close(h);flash.writes=0;
 {PresetStore s;check(s.replaceAll(old,{})==preset_archive::Commit::failed&&flash.writes==0,"invalid active CRC forbids cleanup");}unrelated();
 puts("PASS real NVS guards: read-only operations, invalid selector and corrupt selected CRC never reclaim legacy data");
}
void legacy(){
 seed();{PresetStore s;check(s.saveClearedMask(2),"native legacy tombstone");}
 reboot();{Rig r;check(!r.controller.presetExists(1),"v0.2 logical deletion retained");r.controller.setChannel(23,211);check(r.physical.savePreset(1),"legacy Save restores deleted slot");check(r.physical.clearPreset(0),"legacy Delete");}
 reboot();preset_archive::Collection old;check(capture(old)&&!(old.present&1)&&(old.present&2)&&old.values[1][22]==211,"v0.2 mutations and metadata persist");
 unsigned calls=0;preset_archive::Gate gate;gate.context=&calls;gate.check=[](void* p){return ++*static_cast<unsigned*>(p)==1;};
 {PresetStore s;auto next=old;next.values[1][22]=240;check(s.replaceAll(next,gate)==preset_archive::Commit::failed,"cancel first adoption after candidate write");}
 reboot();preset_archive::Collection recovered;check(capture(recovered)&&same(recovered,old),"cancelled candidate never selected");
 {Rig r;r.controller.setChannel(23,212);check(r.physical.savePreset(1),"legacy Save after cancelled candidate");check(r.physical.clearPreset(2),"legacy Delete after cancelled candidate");}unrelated();
 puts("PASS real NVS v0.2 tombstones/Save/Delete/reboot and cancelled first-import candidate recovery");
}
int main(){setvbuf(stdout,nullptr,_IONBF,0);primary();interrupted();capacity();guards();legacy();unmount();}
