#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
using namespace websocket_bridge;
struct Storage:FakeStorage{unsigned replacements=0;bool fail=false;
 preset_archive::Commit replaceAll(const preset_archive::Collection& c,const preset_archive::Gate& gate)override{
  if(fail||!gate.allowed())return preset_archive::Commit::failed;++replacements;
  mask=uint8_t(~c.present);for(unsigned i=0;i<8;++i){states[i]=c.present&(1U<<i)?SlotState::valid:SlotState::missing;packets[i][0]=0;memcpy(packets[i]+1,c.values[i],512);}return preset_archive::Commit::ok;
 }};
struct L:Lock{uint32_t now=1000;bool held=false;unsigned captures=0;
 void enter()override{assert(!held);held=true;}void leave()override{assert(held);held=false;}uint32_t nowMs()const override{return now;}
 void archivePrepared(uint32_t,uint32_t,unsigned)override{assert(!held);++captures;}
};
struct Rig{
 Storage storage;DmxController controller{storage};CommandDispatcher dispatcher{controller};L lock;Bridge bridge{controller,dispatcher,lock};std::vector<char> buffer;uint32_t id=0;
 Rig():buffer(preset_archive::max_file){dispatcher.setSenderActive(true);bridge.archiveBuffer(buffer.data(),buffer.size());bridge.ownerPoll(true);}
 Token connect(){auto token=bridge.open(lock.now);bridge.ownerPoll(true);Event e;assert(bridge.take(token,e)&&e.kind==Kind::hello);assert(bridge.take(token,e)&&e.kind==Kind::snapshot);return token;}
 void send(Token token,const std::string& fields,bool process=true){auto text="{\"v\":1,\"id\":"+std::to_string(++id)+","+fields+"}";assert(text.size()<=256);bridge.receive(token,text.data(),text.size(),lock.now);bridge.ownerPoll(true);if(process)dispatcher.process();lock.now+=101;}
 Event result(Token token){Event e;while(bridge.take(token,e))if(e.kind==Kind::archive_result)return e;assert(false);return {};}
 void drain(Token token){Event e;while(bridge.take(token,e)){};}
 uint32_t begin(Token t,const std::string& file){send(t,"\"type\":\"presets.import\",\"bytes\":"+std::to_string(file.size()));auto e=result(t);assert(!strcmp(e.reason,"ready"));return e.archive_token;}
 void upload(Token t,uint32_t token,const std::string& file){for(size_t i=0;i<file.size();i+=preset_archive::chunk_size){char data[129];auto n=std::min(preset_archive::chunk_size,file.size()-i);preset_archive::base64(reinterpret_cast<const uint8_t*>(file.data()+i),n,data);send(t,"\"type\":\"presets.chunk\",\"token\":"+std::to_string(token)+",\"offset\":"+std::to_string(i)+",\"data\":\""+data+"\"");auto e=result(t);assert(!strcmp(e.reason,"staged")&&e.archive_offset==i+n);}}
 void commit(Token t,uint32_t token,bool process=true){send(t,"\"type\":\"presets.commit\",\"token\":"+std::to_string(token),process);}
};
std::string collection(){preset_archive::Collection c;c.present=129;c.values[0][0]=255;c.values[7][511]=200;std::vector<char> data(preset_archive::max_file);size_t size=preset_archive::serialize(c,data.data(),data.size());return std::string(data.data(),size);}
void exportContinuity(){
 Rig r;preset_archive::Collection c;c.present=15;for(unsigned i=0;i<34;++i)c.values[0][i]=100;
 for(unsigned i=0;i<4;++i){r.storage.states[i]=DmxPresetStorage::SlotState::valid;memcpy(r.storage.packets[i]+1,c.values[i],512);}
 std::vector<char> data(preset_archive::max_file);auto size=preset_archive::serialize(c,data.data(),data.size());assert(size==4357);std::string expected(data.data(),size);
 auto a=r.connect(),peer=r.connect();unsigned publications=0;r.dispatcher.setPublisher([](void* p,const ControllerCommand&){++*static_cast<unsigned*>(p);return 0;},&publications);
 r.send(a,"\"type\":\"presets.export\"");auto info=r.result(a);std::string downloaded;unsigned reads=0;
 assert(r.lock.captures==1&&!r.lock.held); // Capture/serialization and diagnostics never hold the Bridge lock.
 while(downloaded.size()<info.archive_total){
  ControllerCommand command;command.type=CommandType::load_preset;command.payload.preset.slot=0;assert(r.dispatcher.executeLocal(command).domain==DmxController::Result::ok);
  command.type=CommandType::set_channel;command.payload.channel.channel=23;command.payload.channel.value=int(++reads);assert(r.dispatcher.executeLocal(command).domain==DmxController::Result::ok);
  command.type=CommandType::save_preset;command.payload.preset.slot=4;assert(r.dispatcher.executeLocal(command).domain==DmxController::Result::ok);
  command.type=CommandType::clear_preset;assert(r.dispatcher.executeLocal(command).domain==DmxController::Result::ok);
  command.type=CommandType::set_blackout;command.payload.blackout=reads&1;assert(r.dispatcher.executeLocal(command).domain==DmxController::Result::ok);
  r.drain(a);r.drain(peer);uint8_t live[513];memcpy(live,r.controller.workingPacket(),513);const auto sent=publications;
  r.send(a,"\"type\":\"presets.read\",\"token\":"+std::to_string(info.archive_token)+",\"offset\":"+std::to_string(downloaded.size()));auto chunk=r.result(a);
  assert(!strcmp(chunk.reason,"data")&&chunk.archive_length==std::min(size_t(512),size-downloaded.size()));downloaded.append(reinterpret_cast<char*>(chunk.values),chunk.archive_length);
  assert(publications==sent&&!memcmp(live,r.controller.workingPacket(),513)&&r.bridge.alive(peer));
 }
 assert(downloaded==expected&&reads==9&&publications>0);
 // A genuinely backpressured client still follows the bounded queue policy;
 // it is closed explicitly, never allowed to own/block the Controller loop.
 ControllerCommand command;command.type=CommandType::set_channel;command.payload.channel.channel=1;
 for(unsigned i=0;i<10;++i){command.payload.channel.value=int(150+i);r.dispatcher.executeLocal(command);}
 assert(!r.bridge.alive(a));r.bridge.ownerPoll(true);assert(r.controller.channel(1)==159);
 puts("PASS frozen 4357-byte/4-preset export: nine reads while actual Dispatcher Load/Save/Delete/channel/blackout commands continue; two clients, no export publication, explicit bounded slow-client closure");
}
int main(){
 exportContinuity();
 Rig r;auto a=r.connect(),b=r.connect();auto file=collection();r.controller.setChannel(21,113);r.controller.setBlackout(true);r.controller.setChannel(21,9);r.drain(a);r.drain(b);
 auto token=r.begin(a,file);r.send(b,"\"type\":\"presets.import\",\"bytes\":100");assert(!strcmp(r.result(b).reason,"busy"));
 r.upload(a,token,file);assert(r.storage.replacements==0);r.commit(a,token);auto reply=r.result(a);assert(!strcmp(reply.reason,"applied")&&reply.archive_count==2&&r.storage.replacements==1);
 Event event;bool synchronized=false;while(r.bridge.take(b,event))if(event.kind==Kind::universe){assert(event.presets[0]==1&&event.presets[7]==1&&event.values[20]==9&&event.blackout);synchronized=true;}assert(synchronized);
 assert(r.controller.channel(21)==9&&r.controller.outputPacket()[21]==0);r.controller.setBlackout(false);assert(r.controller.channel(21)==113);r.drain(a);r.drain(b);
 r.send(a,"\"type\":\"presets.export\"");reply=r.result(a);assert(!strcmp(reply.reason,"ready"));std::string downloaded;
 r.controller.setChannel(21,88);r.controller.savePreset(0); // Export must stay frozen despite subsequent preset changes.
 while(downloaded.size()<reply.archive_total){r.send(a,"\"type\":\"presets.read\",\"token\":"+std::to_string(reply.archive_token)+",\"offset\":"+std::to_string(downloaded.size()));auto chunk=r.result(a);assert(!strcmp(chunk.reason,"data")&&chunk.archive_length<=512&&chunk.archive_length>0);downloaded.append(reinterpret_cast<const char*>(chunk.values),chunk.archive_length);}
 assert(downloaded==file);r.send(a,"\"type\":\"presets.cancel\",\"token\":"+std::to_string(reply.archive_token));assert(!strcmp(r.result(a).reason,"cancelled"));assert(r.storage.replacements==1);
 token=r.begin(a,file);r.upload(a,token,file);r.controller.clearPreset(0);r.commit(a,token);assert(!strcmp(r.result(a).reason,"conflict")&&r.storage.replacements==1);r.drain(a);r.drain(b);
 token=r.begin(a,file);r.upload(a,token,file);r.send(a,"\"type\":\"presets.cancel\",\"token\":"+std::to_string(token));assert(!strcmp(r.result(a).reason,"cancelled")&&r.storage.replacements==1);
 token=r.begin(a,file);r.upload(a,token,file);r.commit(a,token,false);r.bridge.close(a);r.bridge.ownerPoll(true);r.dispatcher.process();assert(r.storage.replacements==1); // No disconnected queued commit.
 a=r.connect();token=r.begin(a,file);r.upload(a,token,file);r.lock.now+=10001;r.commit(a,token);assert(!strcmp(r.result(a).reason,"unauthorized")&&r.storage.replacements==1);
 r.send(a,"\"type\":\"presets.import\",\"bytes\":32769");assert(r.storage.replacements==1);r.drain(a);
 token=r.begin(a,file);r.upload(a,token,file);r.storage.fail=true;r.commit(a,token);assert(!strcmp(r.result(a).reason,"storage_error")&&r.storage.replacements==1);r.storage.fail=false;
 auto invalid=file;invalid.erase(invalid.size()-1);token=r.begin(a,invalid);r.upload(a,token,invalid);r.commit(a,token);assert(!strcmp(r.result(a).reason,"invalid")&&r.storage.replacements==1);
 token=r.begin(a,file);r.upload(a,token,file);r.bridge.ownerPoll(false);r.commit(a,token);assert(!strcmp(r.result(a).reason,"unauthorized")&&r.storage.replacements==1);
 // Every outbound response is encoded with the same strict bounded contract.
 Event output;output.kind=Kind::archive_result;output.reason="data";output.archive_length=512;char json[max_json];assert(encode(output,json,sizeof(json))>0);output.archive_length=513;assert(!encode(output,json,sizeof(json)));
 puts("PASS actual WebSocket Bridge: bounded chunked roundtrip, two-client occupancy sync, no live/blackout mutation, replace confirmation boundary, conflict/cancel/expiry/disconnect, no queued replay and protocol budgets");
}
