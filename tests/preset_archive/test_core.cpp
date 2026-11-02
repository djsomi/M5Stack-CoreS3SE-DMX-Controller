#include "preset_archive_store.h"
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
using namespace preset_archive;
struct Cut{};
struct MemoryKv:Kv{
 std::vector<uint8_t> bank[2];int active=-1;unsigned step=0,fail=0,cut=0;bool after=false;
 bool tick(){++step;if(cut==step)throw Cut{};return fail!=step;}
 Io selector(uint8_t& value)override{if(!tick())return Io::error;if(active<0)return Io::missing;value=uint8_t(active);return Io::ok;}
 bool select(uint8_t value)override{bool ok=tick();if(ok||after)active=value;return ok;}
 Io read(unsigned b,uint8_t* out,size_t& size)override{if(!tick())return Io::error;if(bank[b].empty())return Io::missing;if(size<bank[b].size())return Io::error;size=bank[b].size();memcpy(out,bank[b].data(),size);return Io::ok;}
 bool write(unsigned b,const uint8_t* data,size_t size)override{bool ok=tick();if(ok||after)bank[b].assign(data,data+size);return ok;}
 bool erase(unsigned b)override{bool ok=tick();if(ok||after)bank[b].clear();return ok;}
 bool commit()override{return tick();}
};
std::string json(const Collection& c){std::vector<char> data(max_file);size_t n=serialize(c,data.data(),data.size());assert(n);return std::string(data.data(),n);}
Collection first(){Collection c;c.present=0x89;for(unsigned slot=0;slot<8;++slot)for(unsigned ch=0;ch<512;++ch)c.values[slot][ch]=uint8_t(slot+ch);return c;}
void parser(){
 Collection a=first(),b;for(unsigned slot=0;slot<8;++slot)if(!(a.present&(1U<<slot)))memset(a.values[slot],0,512);
 auto text=json(a);assert(parse(text.data(),text.size(),b)&&!memcmp(&a,&b,sizeof(a)));assert(text==json(b));
 Collection all;all.present=255;memset(all.values,255,sizeof(all.values));text=json(all);assert(parse(text.data(),text.size(),b)&&b.present==255);
 Collection empty;text=json(empty);assert(parse(text.data(),text.size(),b)&&!b.present);
 for(auto bad:{std::string("{}"),std::string("{bad"),std::string(max_file+1,' '),std::string("{\"format\":\"other\",\"version\":1,\"presets\":[]}"),std::string("{\"format\":\"cores3se-dmx-presets\",\"version\":2,\"presets\":[]}")})assert(!parse(bad.data(),bad.size(),b));
 text=json(a);for(const std::string& replacement:{"256","-1","1.5","true"}){auto bad=text;auto at=bad.find("\"values\":[")+10;bad.replace(at,bad.find(',',at)-at,replacement);assert(!parse(bad.data(),bad.size(),b));}
 for(const std::string& name:{"P9","Long preset name","\xC3\xA9"}){auto bad=text;bad.replace(bad.find("P1"),2,name);assert(!parse(bad.data(),bad.size(),b));}
 auto bad=text;bad.insert(bad.find("\"slot\":1"),"\"slot\":1,");assert(!parse(bad.data(),bad.size(),b));
 for(const std::string& replacement:{"0","9"}){bad=text;bad.replace(bad.find("\"slot\":1")+7,1,replacement);assert(!parse(bad.data(),bad.size(),b));}
 Collection two;two.present=3;bad=json(two);bad.replace(bad.find("\"slot\":2"),8,"\"slot\":1");bad.replace(bad.find("P2"),2,"P1");assert(!parse(bad.data(),bad.size(),b));
 bad=text;bad.insert(bad.find("]"),",0");assert(!parse(bad.data(),bad.size(),b));
 bad=text;bad.erase(bad.size()-1);assert(!parse(bad.data(),bad.size(),b));
 for(unsigned slot=0;slot<8;++slot){Collection one;one.present=uint8_t(1U<<slot);one.values[slot][0]=255;one.values[slot][511]=128;text=json(one);assert(parse(text.data(),text.size(),b)&&b.present==one.present&&b.values[slot][511]==128);}
 uint8_t bytes[96],decoded[96];for(unsigned i=0;i<96;++i)bytes[i]=uint8_t(i*13);char encoded[129];base64(bytes,96,encoded);size_t n=0;assert(unbase64(encoded,decoded,n)&&n==96&&!memcmp(bytes,decoded,96));assert(!unbase64("AA=A",decoded,n));
 puts("PASS archive parser/serializer: all slots/512 channels, sparse values/empty/max, deterministic roundtrip, bad JSON/version/format/ranges/names/duplicates/truncation/oversize and raw Base64");
}
void persistence(){
 const auto a=first();auto b=a;b.present=2;memset(b.values,222,sizeof(b.values));Store store;MemoryKv kv;
 assert(store.load(kv)==Store::State::empty&&store.mask()==0);
 for(unsigned slot=0;slot<slots;++slot)for(unsigned ch=0;ch<channels;++ch)assert(store.values(slot)[ch]==0);
 assert(store.publish(kv,{})==Commit::ok&&kv.active==0);
 Store empty_boot;assert(empty_boot.load(kv)==Store::State::active&&empty_boot.mask()==0);
 for(unsigned slot=0;slot<slots;++slot)for(unsigned ch=0;ch<channels;++ch)assert(empty_boot.values(slot)[ch]==0);
 assert(store.replace(kv,a,{})==Commit::ok&&kv.active==1);assert(store.load(kv)==Store::State::active&&store.mask()==a.present);
 MemoryKv baseline=kv;kv.step=0;assert(store.replace(kv,b,{})==Commit::ok);const unsigned operations=kv.step;
 for(bool existing:{false,true})for(bool after:{false,true})for(unsigned failure=1;failure<=operations+2;++failure){
  MemoryKv model=existing?baseline:MemoryKv{};model.step=0;model.fail=failure;model.after=after;Store attempt;const auto result=attempt.replace(model,b,{});(void)result;
  model.fail=0;model.step=0;Store boot;auto state=boot.load(model);assert(state==Store::State::active||(!existing&&state==Store::State::empty));
  if(state==Store::State::active){bool old=boot.mask()==a.present&&!memcmp(boot.values(0),a.values,sizeof(a.values));bool fresh=boot.mask()==b.present&&!memcmp(boot.values(0),b.values,sizeof(b.values));assert((existing&&old)||fresh);}
 }
 for(bool existing:{false,true})for(unsigned cut=1;cut<=operations+2;++cut){MemoryKv model=existing?baseline:MemoryKv{};model.step=0;model.cut=cut;try{Store attempt;attempt.replace(model,b,{});}catch(Cut&){}model.cut=0;Store boot;auto state=boot.load(model);assert(state==Store::State::active||(!existing&&state==Store::State::empty));if(state==Store::State::active)assert((existing&&boot.mask()==a.present&&!memcmp(boot.values(0),a.values,sizeof(a.values)))||(boot.mask()==b.present&&!memcmp(boot.values(0),b.values,sizeof(b.values))));}
 kv=baseline;kv.active=-1;assert(store.load(kv)==Store::State::empty&&store.mask()==0); // Reused Store must forget prior selection; orphan banks are invisible.
 for(unsigned slot=0;slot<slots;++slot)for(unsigned ch=0;ch<channels;++ch)assert(store.values(slot)[ch]==0);
 assert(store.replace(kv,b,{})==Commit::ok&&kv.active==0);
 kv=baseline;for(unsigned i=0;i<12;++i){Store next;assert(next.replace(kv,i&1?a:b,{})==Commit::ok);} // Reclaim only inactive generation.
 kv=baseline;kv.bank[kv.active][100]^=1;assert(store.load(kv)==Store::State::corrupt);assert(store.replace(kv,b,{})==Commit::failed);
 kv=baseline;Gate gate;gate.check=[](void*){return false;};assert(store.replace(kv,b,gate)==Commit::failed);assert(kv.active==baseline.active);
 // Disconnect/cancel after the candidate has been written and verified, but
 // before the visibility boundary, must retain the selected old generation.
 unsigned checks=0;gate.context=&checks;gate.check=[](void* p){return ++*static_cast<unsigned*>(p)==1;};kv=baseline;assert(store.replace(kv,b,gate)==Commit::failed&&checks==2&&kv.active==baseline.active);
 puts("PASS atomic generations: first empty publication, every API failure/before-after mutation, simulated reboot/power interruption, current-format first import, repeated replacements, CRC fail-closed and cancellation; never mixed visible slots");
}
struct Storage:FakeStorage{
 Commit outcome=Commit::ok;
 bool fail_after_save=false,fail_after_erase=false;
 bool erase(uint8_t slot)override{const bool ok=FakeStorage::erase(slot);return ok&&!fail_after_erase;}
 bool save(uint8_t slot,const uint8_t* packet)override{const bool ok=FakeStorage::save(slot,packet);return ok&&!fail_after_save;}
 Commit replaceAll(const Collection& c,const Gate& gate)override{if(!gate.allowed())return Commit::failed;if(outcome==Commit::failed)return outcome;for(unsigned i=0;i<8;++i){states[i]=c.present&(1U<<i)?SlotState::valid:SlotState::missing;packets[i][0]=0;memcpy(packets[i]+1,c.values[i],512);}return outcome;}
};
struct Mutex:websocket_bridge::Lock{uint32_t now=100;void enter()override{}void leave()override{}uint32_t nowMs()const override{return now;}};
void domain(){
 Storage storage;DmxController c(storage);CommandDispatcher d(c);d.setSenderActive(true);unsigned publications=0;d.setPublisher([](void* p,const ControllerCommand&){++*static_cast<unsigned*>(p);return 0;},&publications);
 c.setChannel(1,12);c.setBlackout(true);c.setChannel(1,99);uint8_t working[513];memcpy(working,c.workingPacket(),513);
 auto a=first();auto data=json(a);assert(c.stageArchive(data.data(),data.size(),c.presetRevision(),{})==DmxController::Result::ok);
 ControllerCommand command;command.type=CommandType::replace_presets;command.payload.archive_token=c.archiveToken();auto result=d.executeLocal(command);assert(result.status==CommandStatus::applied&&publications==0&&c.blackoutActive()&&!memcmp(working,c.workingPacket(),513)&&c.outputPacket()[1]==0);
 assert(c.presetExists(0)&&!c.presetExists(1));c.setBlackout(false);assert(c.channel(1)==12);
 assert(c.stageArchive(data.data(),data.size(),c.presetRevision(),{})==DmxController::Result::ok);command.payload.archive_token=c.archiveToken();c.clearPreset(0);assert(d.executeLocal(command).domain==DmxController::Result::archive_conflict);
 auto token=c.archiveToken();c.discardArchive();assert(!c.archiveValid(token));
 assert(c.stageArchive(data.data(),data.size(),c.presetRevision(),{})==DmxController::Result::ok);command.payload.archive_token=c.archiveToken();
 storage.fail_after_save=true;c.setChannel(1,17);assert(c.savePreset(3)==DmxController::Result::write_failed);storage.fail_after_save=false;
 assert(d.executeLocal(command).domain==DmxController::Result::archive_conflict); // A failed API can still have changed flash.
 assert(c.stageArchive(data.data(),data.size(),c.presetRevision(),{})==DmxController::Result::ok);command.payload.archive_token=c.archiveToken();
 c.loadPreset(3);assert(c.loadedPreset()==4);storage.fail_after_erase=true;
 assert(c.clearPreset(3)==DmxController::Result::write_failed&&!c.presetExists(3)&&c.loadedPreset()==0);storage.fail_after_erase=false;
 assert(d.executeLocal(command).domain==DmxController::Result::archive_conflict); // Failed delete may have committed; occupancy must be refreshed.
 assert(c.captureArchive()==DmxController::Result::ok);auto exported=json(c.archive());Collection parsed;assert(parse(exported.data(),exported.size(),parsed)&&!(parsed.present&1));
 const auto live=c.channel(1);storage.outcome=Commit::uncertain;assert(c.stageArchive(data.data(),data.size(),c.presetRevision(),{})==DmxController::Result::ok);command.payload.archive_token=c.archiveToken();
 auto uncertain=d.executeLocal(command);assert(uncertain.domain==DmxController::Result::archive_uncertain&&uncertain.status==CommandStatus::storage_error&&c.channel(1)==live&&publications==0);
 puts("PASS actual Controller/Dispatcher: owned staging token, conflict detection, no DMX publication, unchanged live/blackout/restore values, occupancy and read-only export");
}
int main(){parser();persistence();domain();}
