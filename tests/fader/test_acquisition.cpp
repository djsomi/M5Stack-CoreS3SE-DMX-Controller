// Real generated driver bodies and installed Touch_Class, simulated existing bus.
#include "M5Unified.hpp"
#include "touch_acquisition_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
uint32_t fake_us=0;FakeSerial Serial;FakeM5 M5;
namespace bus {
struct Packet {int y;unsigned fail;bool empty;};
std::vector<Packet> packets;size_t index=0;std::vector<int> pins;size_t pin_index=0;
uint64_t signature=1469598103934665603ULL;unsigned transactions=0,operations=0;
void mark(unsigned c){signature=(signature^c)*1099511628211ULL;++operations;fake_us+=2;}
Packet current(){return packets[index<packets.size()?index:packets.size()-1];}
bool op(unsigned stage,char tag){mark(tag);return current().fail!=stage;}
void payload(std::vector<Packet> p){packets=p;index=0;}
}
namespace lgfx {
using m5gfx::touch_point_t;
bool gpio_in(int){bus::mark('P');int v=bus::pins.empty()?0:bus::pins[bus::pin_index<bus::pins.size()?bus::pin_index:bus::pins.size()-1];++bus::pin_index;return v;}
namespace i2c {
struct Result {bool valid;explicit operator bool()const{return valid;}bool has_value()const{return valid;}};
bool beginTransaction(int,int,unsigned,bool){++bus::transactions;bool ok=bus::op(1,'B');if(!ok)++bus::index;return ok;}
bool writeBytes(int,uint8_t* p,size_t n){assert(n==1 && p[0]==2);return bus::op(2,'W');}
bool restart(int,int,unsigned,bool){return bus::op(3,'R');}
bool readBytes(int,uint8_t* out,size_t n){
    if(!bus::op(n==1?4:5,n==1?'H':'D'))return false;
    auto p=bus::current();if(n==1){out[0]=p.empty?0:1;return true;}
    assert(n==4);out[0]=1;out[1]=34;out[2]=uint8_t(p.y>>8)&15;out[3]=uint8_t(p.y);return true;
}
Result endTransaction(int){bool ok=bus::op(6,'E');++bus::index;return {ok};}
Result readRegister8(int,int,uint8_t,unsigned){bus::mark('C');return {true};}
}
struct Touch_FT5x06 {
    struct Config {int i2c_port=0,i2c_addr=0x38,pin_int=21;unsigned freq=400000;} _cfg;
    bool _flg_released=false,initialized=true;enum{max_touch_points=5,FT5x06_INTMODE_REG=0xA4};
    bool _check_init(){return initialized;}
    bool _write_reg(uint8_t,uint8_t){bus::mark('U');return true;}
    size_t _read_data(uint8_t*);virtual uint_fast8_t getTouchRaw(touch_point_t*,uint_fast8_t);
};
#include "acquisition_ft_under_test.h"
}
namespace m5gfx {
using lgfx::gpio_in;using lgfx::touch_point_t;
static const int i2c_port=0,aw9523_i2c_addr=0x58;static const unsigned i2c_freq=400000;
struct Core : lgfx::Touch_FT5x06 {
    #include "acquisition_core_under_test.h"
};
}
m5gfx::Core driver;
size_t acquire(m5gfx::touch_point_t* out,size_t count){return driver.getTouchRaw(out,count);}
void setup(){M5=FakeM5{};driver=m5gfx::Core{};M5.Display.acquire=acquire;M5.Touch.begin(&M5.Display);bus::pins.clear();}
void cycle(unsigned ms){fake_us=ms*1000;bus::pin_index=0;M5.Touch.update(ms);}
void drain(unsigned id){touch_acquisition::endGesture(id);
    for(int i=0;i<3000;++i){fake_us+=4000;if(!touch_acquisition::service(id))break;}
}
int main(){
    setup();bus::payload({{109,0,false}});cycle(10);assert(M5.Touch.getTouchPointRaw().y==109);
    touch_acquisition::beginGesture(1,0);
    for(unsigned ms=11;ms<=1010;++ms)cycle(ms);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    touch_acquisition::endGesture(1);auto* l=touch_acquisition::pending(1);
    assert(l && l->read_calls>350 && l->valid_reads==l->read_calls && !l->wire_changes && !l->cache_changes);
    assert(l->max_read_gap<6000 && l->max_repeat_age>=990000 && l->gated>=790);
#endif
    drain(1);
    // All available small report changes reach the pre-classification M5 cache.
    touch_acquisition::beginGesture(2,0);bus::payload({{108,0,false}});cycle(1015);
    assert(M5.Touch.getTouchPointRaw().y==108 && M5.Touch.getDetail().y==109);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(touch_acquisition::latest().driver.points[0].y==108 && touch_acquisition::latest().cache.points[0].y==108);
#endif
    drain(2);
    // Existing consistency retry can select buffer 0 while the last read differs.
    setup();bus::payload({{109,0,false}});cycle(2000);touch_acquisition::beginGesture(3,0);
    bus::payload({{109,0,false},{108,0,false},{109,0,false},{108,0,false},{109,0,false},{108,0,false}});cycle(2005);
    assert(M5.Touch.getTouchPointRaw().y==109);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    const auto& c=touch_acquisition::latest();assert(c.read_count==6 && c.retry_left==0 && c.selected.points[0].y==109);
    assert(c.reads[5].frame.points[0].y==108 && c.driver.points[0].y==109 && c.cache.points[0].y==109);
    touch_acquisition::endGesture(3);l=touch_acquisition::pending(3);assert(l->wire_driver_diff && l->wire_changes && l->anomaly_saved);
#endif
    drain(3);
    setup();bus::payload({{109,0,false}});cycle(3000);touch_acquisition::beginGesture(4,0);
    auto tx=bus::transactions;bus::pins={1};cycle(3005);assert(bus::transactions==tx);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(touch_acquisition::latest().core_irq==1 && touch_acquisition::latest().read_count==0);
    assert(touch_acquisition::latest().driver.valid && touch_acquisition::latest().driver.count==0);
#endif
    drain(4);
    // FT's own released gate, using the two EXISTING GPIO reads.
    setup();bus::payload({{109,0,false}});cycle(4000);touch_acquisition::beginGesture(5,0);
    tx=bus::transactions;bus::pins={0,1};cycle(4005);assert(bus::transactions==tx);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(touch_acquisition::latest().ft_irq==1 && touch_acquisition::latest().released_gate);
#endif
    drain(5);
    setup();bus::payload({{109,0,false}});cycle(5000);touch_acquisition::beginGesture(6,0);
    // Initial failed begin followed by successful existing retries. Buffer 0 is
    // valid before decode; do not exercise vendor uninitialized-data UB here.
    bus::payload({{109,1,false},{108,0,false},{108,0,false}});cycle(5005);
    assert(M5.Touch.getTouchPointRaw().y==108);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(touch_acquisition::latest().reads[0].failed==1 && !touch_acquisition::latest().reads[0].length);
    assert(touch_acquisition::latest().reads[1].prefix[0]==1 && touch_acquisition::latest().reads[1].prefix[4]==108);
#endif
    bus::payload({{108,0,true}});cycle(5010);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(touch_acquisition::latest().reads[0].length==1);
    assert(touch_acquisition::latest().driver.valid && !touch_acquisition::latest().driver.count);
    assert(touch_acquisition::latest().cache.valid && !touch_acquisition::latest().cache.count);
#endif
    drain(6);
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
    assert(strstr(Serial.output,"[ACQ_END] id=6") && strstr(Serial.output,"hex=010122006c"));
    touch_acquisition::beginGesture(7,0);touch_acquisition::enter(5015);
    touch_acquisition::Frame reported,missing;reported.valid=missing.valid=true;reported.count=1;
    reported.points[0].valid=true;reported.points[0].y=108;
    touch_acquisition::publish(reported,false);touch_acquisition::publish(missing,true);touch_acquisition::leave();
    touch_acquisition::endGesture(7);l=touch_acquisition::pending(7);
    assert(l->driver_cache_diff==1 && l->anomaly_saved);
    drain(7);
    assert(Serial.max_write<=40);puts("PASS observed existing reads, repeated sensor payloads, changing reports, buffer-0 selection, IRQ/no-read gates, failures and raw-byte prefixes; bounded deferred logs");
#endif
    printf("SIGNATURE %llu transactions=%u operations=%u\n",(unsigned long long)bus::signature,bus::transactions,bus::operations);
}
