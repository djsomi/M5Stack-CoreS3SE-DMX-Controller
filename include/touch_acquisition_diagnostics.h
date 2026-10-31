#pragma once
#include "hardware_diagnostics.h"
#ifndef DMX_FADER_DIAGNOSTICS
#define DMX_FADER_DIAGNOSTICS 0
#endif
#ifndef DMX_TOUCH_ACQUISITION_DIAGNOSTICS
#define DMX_TOUCH_ACQUISITION_DIAGNOSTICS 0
#endif
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS && !DMX_FADER_DIAGNOSTICS
#error "Acquisition diagnostics require the existing fader diagnostics output"
#endif
#include <stdint.h>
#include <stddef.h>

namespace touch_acquisition {
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
struct Point {int16_t x=0,y=0;uint16_t id=0;bool valid=false;};
struct Frame {Point points[5];uint8_t count=0;bool valid=false;};
struct Read {Frame frame;uint32_t start=0,end=0;uint8_t length=0,failed=0,passed=0,prefix[5]={};};
struct Cycle {
    uint32_t sequence=0,m5_ms=0,start=0,end=0;
    Frame selected,driver,cache;Read reads[6];
    uint8_t read_count=0,read_overflow=0,comparisons=0,mismatches=0,retry_left=5;
    int8_t core_irq=-1,ft_irq=-1,init_ok=-1;
    uint8_t core_calls=0,ft_calls=0,irq_clear=0,requested=0;
    bool gate=false,released_gate=false;
};
struct Compact {
    uint32_t sequence=0,us=0,m5_ms=0,duration=0;
    Point wire_first,wire_last,selected,driver,cache;
    uint8_t reads=0,failed=0;int8_t core_irq=-1,ft_irq=-1;
    int8_t selected_n=-1,driver_n=-1,cache_n=-1;
    bool gate=false,exhausted=false;
};
struct Ledger {
    uint32_t gesture=0,start=0,capture=0,end=0,cycles=0,gated=0,core_high=0,core_low=0,ft_high=0;
    uint32_t read_calls=0,valid_reads=0,failed_reads=0,empty_reads=0,retry_exhausted=0;
    uint32_t wire_changes=0,driver_changes=0,cache_changes=0;
    uint32_t first_wire_change=0,first_driver_change=0,first_cache_change=0;
    uint32_t max_read_gap=0,max_no_read_age=0,max_repeat_age=0,wire_driver_diff=0,driver_cache_diff=0;
    uint16_t point_id=0;bool motion_saved=false,anomaly_saved=false;
    Point last_wire,last_driver,last_cache;uint32_t last_read=0,last_wire_change=0;
    Cycle initial,motion,anomaly;
    Compact onset[13],tail[8];bool onset_used[13]={};uint32_t tail_count=0;
};
enum class Op:uint8_t { begin=1,write=2,restart=3,count=4,body=5,end=6,init=7 };
uint32_t nowUs();
void enter(uint32_t ms);void leave();void gate();void core();void ft(unsigned count);
bool pin(bool value,bool outer);bool checked(Op op,bool value);void released(bool value);void irqClear();
void readEnter();void readLeave(const uint8_t* data,size_t bytes);
bool repeat(bool value);void selected(const uint8_t* data,size_t bytes,int retries);
void publish(const Frame& frame,bool cache);
void beginGesture(uint32_t id,uint16_t point);void endGesture(uint32_t id);
const Ledger* pending(uint32_t id);const Cycle& latest();bool service(uint32_t id);
class TouchScope {public:explicit TouchScope(uint32_t ms){enter(ms);}~TouchScope(){leave();}};
class ReadScope {
 public:ReadScope(const uint8_t* data,const size_t& size):data_(data),size_(size){readEnter();}
    ~ReadScope(){readLeave(data_,size_);}
 private:const uint8_t* data_;const size_t& size_;
};
template<class T> bool io(Op op,const T& result){return checked(op,static_cast<bool>(result));}
template<class T> void points(const T* data,size_t count,bool cache) {
    Frame f;f.valid=true;f.count=count<5?uint8_t(count):5;
    for(size_t i=0;i<f.count;++i){f.points[i].x=data[i].x;f.points[i].y=data[i].y;f.points[i].id=data[i].id;f.points[i].valid=true;}
    publish(f,cache);
}
#else
inline void beginGesture(uint32_t,uint16_t){} inline void endGesture(uint32_t){}
inline bool service(uint32_t){return false;}
#endif
}
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
#define ACQ_TOUCH(ms) touch_acquisition::TouchScope acq_touch_scope(ms)
#define ACQ_GATE() touch_acquisition::gate()
#define ACQ_CORE() touch_acquisition::core()
#define ACQ_FT(count) touch_acquisition::ft(count)
#define ACQ_PIN(value,outer) touch_acquisition::pin(static_cast<bool>(value),outer)
#define ACQ_IO(op,value) touch_acquisition::io(touch_acquisition::Op::op,value)
#define ACQ_RELEASE(value) touch_acquisition::released(value)
#define ACQ_CLEAR() touch_acquisition::irqClear()
#define ACQ_READ(data,size) touch_acquisition::ReadScope acq_read_scope(data,size)
#define ACQ_REPEAT(value) touch_acquisition::repeat(static_cast<bool>(value))
#define ACQ_SELECTED(data,size,retry) touch_acquisition::selected(data,size,retry)
#define ACQ_POINTS(data,count,cache) touch_acquisition::points(data,count,cache)
#else
#define ACQ_TOUCH(ms)
#define ACQ_GATE()
#define ACQ_CORE()
#define ACQ_FT(count)
#define ACQ_PIN(value,outer) (value)
#define ACQ_IO(op,value) (value)
#define ACQ_RELEASE(value)
#define ACQ_CLEAR()
#define ACQ_READ(data,size)
#define ACQ_REPEAT(value) (value)
#define ACQ_SELECTED(data,size,retry)
#define ACQ_POINTS(data,count,cache)
#endif
