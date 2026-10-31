// Real domain/Bridge objects, no radio. Native HTTP close invokes this same
// Bridge::close path. Electrical DMX and SDK teardown still require hardware.
#include "websocket_bridge.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace websocket_bridge;
struct SingleTaskLock:Lock{void enter()override{}void leave()override{}};
int main(){
 FakeStorage storage;DmxController controller(storage);CommandDispatcher dispatcher(controller);
 SingleTaskLock lock;Bridge bridge(controller,dispatcher,lock);dispatcher.setSenderActive(true);
 bridge.ownerPoll(true);Token clients[2];Event event;
 for(auto& token:clients){token=bridge.open(1000);bridge.ownerPoll(true);assert(bridge.take(token,event)&&event.kind==Kind::hello);assert(bridge.take(token,event)&&event.kind==Kind::snapshot);}
 const char* pending="{\"v\":1,\"id\":1,\"type\":\"channel.set\",\"channel\":23,\"value\":99}";
 bridge.receive(clients[0],pending,strlen(pending),1000);
 for(auto token:clients)bridge.close(token); // OFF retires every socket generation.
 bridge.ownerPoll(true);dispatcher.process();assert(controller.channel(23)==0&&bridge.metrics().clients==0);
 // Physical/application owner remains usable with no HTTP/radio clients.
 auto result=dispatcher.executeLocal(ControllerCommand::channelCommand(CommandType::set_channel,23,143));
 assert(result.status==CommandStatus::applied&&controller.outputPacket()[23]==143&&controller.outputPacket()[0]==0);
 for(auto old:clients){auto token=bridge.open(2000);assert(token.generation!=old.generation);bridge.ownerPoll(true);
  assert(bridge.take(token,event)&&event.kind==Kind::hello);
  assert(bridge.take(token,event)&&event.kind==Kind::snapshot&&event.values[22]==143&&event.revision==controller.revision());
  bridge.receive(old,pending,strlen(pending),2000);bridge.close(old);assert(bridge.alive(token));}
 bridge.ownerPoll(true);dispatcher.process();assert(controller.channel(23)==143);
 puts("PASS real Controller/Dispatcher/Bridge: two-client retirement, no stale mutation replay, local output while offline and latest complete snapshot on reconnect");
}
