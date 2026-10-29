#pragma once
#include "websocket_bridge.h"
namespace websocket_service {
// Setup and ownerPoll: application task. Other calls: HTTP/worker tasks.
void initialize(DmxController& controller,CommandDispatcher& dispatcher,DeviceSettings* settings=nullptr,DeviceMaintenance* maintenance=nullptr);
void ownerPoll(bool sender);
websocket_bridge::Token open(uint64_t now_ms);
void close(websocket_bridge::Token token);
bool alive(websocket_bridge::Token token);
void receive(websocket_bridge::Token token,const char* data,size_t length,uint64_t now_ms);
bool take(websocket_bridge::Token token,websocket_bridge::Event& event);
void sendFailed(websocket_bridge::Token token);
websocket_bridge::Metrics metrics();
websocket_bridge::ClientInfo clientInfo(websocket_bridge::Token token);
}
