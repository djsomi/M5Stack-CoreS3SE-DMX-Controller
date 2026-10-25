#include "websocket_service.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <new>
#include <esp_timer.h>
namespace websocket_service {
namespace {
struct CriticalLock final : websocket_bridge::Lock {
    portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
    void enter() override {portENTER_CRITICAL(&mux);}
    void leave() override {portEXIT_CRITICAL(&mux);}
    uint32_t nowMs() const override {return uint32_t(esp_timer_get_time()/1000);}
} lock;
alignas(websocket_bridge::Bridge) unsigned char storage[sizeof(websocket_bridge::Bridge)];
websocket_bridge::Bridge* bridge=nullptr;
}
void initialize(DmxController& c,CommandDispatcher& d){bridge=new(storage) websocket_bridge::Bridge(c,d,lock);}
void ownerPoll(bool sender){bridge->ownerPoll(sender);}
websocket_bridge::Token open(uint64_t now){return bridge->open(now);}
void close(websocket_bridge::Token t){bridge->close(t);}
bool alive(websocket_bridge::Token t){return bridge->alive(t);}
void receive(websocket_bridge::Token t,const char* p,size_t n,uint64_t now){bridge->receive(t,p,n,now);}
bool take(websocket_bridge::Token t,websocket_bridge::Event& e){return bridge->take(t,e);}
void sendFailed(websocket_bridge::Token t){bridge->sendFailed(t);}
websocket_bridge::Metrics metrics(){return bridge->metrics();}
websocket_bridge::ClientInfo clientInfo(websocket_bridge::Token t){return bridge->clientInfo(t);}
}
