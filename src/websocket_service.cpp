#include "websocket_service.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <new>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#if DMX_HW_DIAGNOSTICS
#include "hardware_diagnostics.h"
#endif
namespace websocket_service {
namespace {
struct CriticalLock final : websocket_bridge::Lock {
    portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
    void enter() override {portENTER_CRITICAL(&mux);}
    void leave() override {portEXIT_CRITICAL(&mux);}
    uint32_t nowMs() const override {return uint32_t(esp_timer_get_time()/1000);}
    uint64_t nowUs() const override {return uint64_t(esp_timer_get_time());}
#if DMX_HW_DIAGNOSTICS
    void archivePrepared(uint32_t read_us,uint32_t json_us,unsigned slots) override {
        // Existing bounded Stage 0 queue: service() formats/drains later using
        // availableForWrite. Never log synchronously or initialize USB here.
        hardware_diagnostics::capture("EXPORT_SNAPSHOT_US","archive","read-only",false,nullptr,uint8_t(slots),int(read_us));
        hardware_diagnostics::capture("EXPORT_SERIALIZE_US","archive","read-only",false,nullptr,uint8_t(slots),int(json_us));
    }
#endif
} lock;
alignas(websocket_bridge::Bridge) unsigned char storage[sizeof(websocket_bridge::Bridge)];
websocket_bridge::Bridge* bridge=nullptr;
}
void initialize(DmxController& c,CommandDispatcher& d,DeviceSettings* s,DeviceMaintenance* m,WifiConfiguration* w){bridge=new(storage) websocket_bridge::Bridge(c,d,lock,s,m,w);}
void prepareArchive(){auto* buffer=static_cast<char*>(heap_caps_malloc(preset_archive::max_file,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));bridge->archiveBuffer(buffer,buffer?preset_archive::max_file:0);}
void ownerPoll(bool sender){bridge->ownerPoll(sender);}
websocket_bridge::Token open(uint64_t now){return bridge->open(now);}
void close(websocket_bridge::Token t){bridge->close(t);}
bool alive(websocket_bridge::Token t){return bridge->alive(t);}
bool hasGeneration(uint32_t generation){return bridge&&bridge->hasGeneration(generation);}
void receive(websocket_bridge::Token t,const char* p,size_t n,uint64_t now){bridge->receive(t,p,n,now);}
bool take(websocket_bridge::Token t,websocket_bridge::Event& e){return bridge->take(t,e);}
void sendFailed(websocket_bridge::Token t){bridge->sendFailed(t);}
websocket_bridge::Metrics metrics(){return bridge->metrics();}
websocket_bridge::ClientInfo clientInfo(websocket_bridge::Token t){return bridge->clientInfo(t);}
}
