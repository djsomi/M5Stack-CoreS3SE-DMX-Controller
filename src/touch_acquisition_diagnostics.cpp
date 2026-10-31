#include "touch_acquisition_diagnostics.h"
#if DMX_TOUCH_ACQUISITION_DIAGNOSTICS
#include <Arduino.h>
#include <esp_timer.h>
#include <new>
#include <stdio.h>
namespace touch_acquisition {
namespace {
Cycle cycle,complete;bool in_cycle=false;uint32_t sequence=0;
Ledger ledgers[2];bool ready[2]={false,false};int active=-1;int read_index=-1;
char line[512];size_t length=0,sent=0,row=0;uint32_t last_tx=0;
Point point(const Frame& f,uint16_t id){for(unsigned i=0;i<f.count && i<5;++i)if(f.valid && f.points[i].id==id)return f.points[i];return {};}
bool different(const Point& a,const Point& b){return a.valid && b.valid && (a.x!=b.x || a.y!=b.y);}
bool mismatch(const Point& a,const Point& b){return a.valid!=b.valid || different(a,b);}
Frame decode(const uint8_t* bytes,size_t n) {
    Frame f;if(!n)return f;unsigned count=bytes[0]&15;
    if(count>5 || n<(count?count*6-1:1))return f;
    f.valid=true;f.count=count;
    for(unsigned i=0;i<count;++i){const auto* d=bytes+i*6;auto& p=f.points[i];p.x=((d[1]&15)<<8)|d[2];p.y=((d[3]&15)<<8)|d[4];p.id=d[3]>>4;p.valid=true;}
    return f;
}
void maximum(uint32_t& a,uint32_t b){if(b>a)a=b;}
Compact compact(const Cycle& c,uint16_t id) {
    Compact s;s.sequence=c.sequence;s.us=c.end;s.m5_ms=c.m5_ms;s.duration=c.end-c.start;
    s.reads=c.read_count;s.core_irq=c.core_irq;s.ft_irq=c.ft_irq;s.gate=c.gate;s.exhausted=c.retry_left==0 && c.mismatches;
    for(unsigned i=0;i<c.read_count;++i){auto p=point(c.reads[i].frame,id);if(p.valid){if(!s.wire_first.valid)s.wire_first=p;s.wire_last=p;}s.failed+=c.reads[i].failed!=0 || !c.reads[i].length;}
    s.selected=point(c.selected,id);s.driver=point(c.driver,id);s.cache=point(c.cache,id);
    s.selected_n=c.selected.valid?c.selected.count:-1;s.driver_n=c.driver.valid?c.driver.count:-1;s.cache_n=c.cache.valid?c.cache.count:-1;return s;
}
void accumulate(Ledger& l,const Cycle& c) {
    ++l.cycles;l.gated+=c.gate;l.core_high+=c.core_irq==1;l.core_low+=c.core_irq==0;l.ft_high+=c.ft_irq==1;
    l.retry_exhausted+=c.retry_left==0 && c.mismatches;
    bool movement=false,anomaly=false;const Frame* last_report=nullptr;
    for(unsigned i=0;i<c.read_count;++i) {
        const auto& r=c.reads[i];++l.read_calls;
        if(r.frame.valid)last_report=&r.frame;
        if(l.last_read)maximum(l.max_read_gap,r.start-l.last_read);l.last_read=r.start;
        l.failed_reads+=r.failed!=0 || !r.length;l.valid_reads+=r.frame.valid;l.empty_reads+=r.frame.valid && !r.frame.count;
        auto p=point(r.frame,l.point_id);
        if(p.valid) {
            if(!l.last_wire.valid)l.last_wire_change=r.end;
            else if(different(p,l.last_wire)){++l.wire_changes;if(!l.first_wire_change)l.first_wire_change=r.end;l.last_wire_change=r.end;movement=true;}
            maximum(l.max_repeat_age,r.end-l.last_wire_change);l.last_wire=p;
        }
        anomaly|=r.failed!=0 || !r.length;
    }
    if(l.last_read)maximum(l.max_no_read_age,c.end-l.last_read);
    const auto d=point(c.driver,l.point_id),k=point(c.cache,l.point_id);
    if(different(d,l.last_driver)){++l.driver_changes;if(!l.first_driver_change)l.first_driver_change=c.end;}
    if(different(k,l.last_cache)){++l.cache_changes;if(!l.first_cache_change)l.first_cache_change=c.end;}
    if(d.valid)l.last_driver=d;if(k.valid)l.last_cache=k;
    const auto s=compact(c,l.point_id);
    if(last_report && c.driver.valid && mismatch(point(*last_report,l.point_id),d)){++l.wire_driver_diff;anomaly=true;}
    if(c.driver.valid && c.cache.valid && mismatch(d,k)){++l.driver_cache_diff;anomaly=true;}
    if(movement && !l.motion_saved){l.motion=c;l.motion_saved=true;}
    if(anomaly && !l.anomaly_saved){l.anomaly=c;l.anomaly_saved=true;}
    const uint32_t age=c.end-l.start;
    if(age<=3000000){const auto i=age/250000;if(!l.onset_used[i] || (l.onset[i].gate && !c.gate)){l.onset_used[i]=true;l.onset[i]=s;}}
    if(!c.gate){l.tail[l.tail_count%8]=s;++l.tail_count;}
}
size_t finalRow(){return 2+13+8+3*(1+6);}
void formatPoint(const Point& p,char* out,size_t cap){if(p.valid)snprintf(out,cap,"%d,%d#%u",p.x,p.y,p.id);else snprintf(out,cap,"absent");}
void prepare(const Ledger& l) {
    int n=0;
    if(row==0)n=snprintf(line,sizeof(line),"[ACQ] id=%lu point=%u start_us=%lu capture_us=%lu end_us=%lu cycles=%lu gated=%lu core_low=%lu core_high=%lu ft_high=%lu read_calls=%lu valid_payloads=%lu failed_reads=%lu empty_reports=%lu retry_exhausted=%lu\n",
        (unsigned long)l.gesture,l.point_id,(unsigned long)l.start,(unsigned long)l.capture,(unsigned long)l.end,(unsigned long)l.cycles,(unsigned long)l.gated,
        (unsigned long)l.core_low,(unsigned long)l.core_high,(unsigned long)l.ft_high,(unsigned long)l.read_calls,(unsigned long)l.valid_reads,(unsigned long)l.failed_reads,(unsigned long)l.empty_reads,(unsigned long)l.retry_exhausted);
    else if(row==1)n=snprintf(line,sizeof(line),"[ACQ] id=%lu wire_changes=%lu driver_changes=%lu cache_changes=%lu first_wire_change_us=%lu first_driver_change_us=%lu first_cache_change_us=%lu max_read_start_gap_us=%lu max_no_read_age_us=%lu max_repeat_report_age_us=%lu wire_driver_diff=%lu driver_cache_diff=%lu\n",
        (unsigned long)l.gesture,(unsigned long)l.wire_changes,(unsigned long)l.driver_changes,(unsigned long)l.cache_changes,
        (unsigned long)l.first_wire_change,(unsigned long)l.first_driver_change,(unsigned long)l.first_cache_change,(unsigned long)l.max_read_gap,(unsigned long)l.max_no_read_age,(unsigned long)l.max_repeat_age,(unsigned long)l.wire_driver_diff,(unsigned long)l.driver_cache_diff);
    else if(row<23) {
        const Compact* s=nullptr;const char* group=row<15?"early250ms":"tail";size_t i=row<15?row-2:row-15;
        if(row<15){if(l.onset_used[i])s=&l.onset[i];}
        else{auto count=l.tail_count<8?l.tail_count:8;if(i<count)s=&l.tail[(l.tail_count-count+i)%8];}
        if(!s)n=snprintf(line,sizeof(line),"[ACQ] id=%lu group=%s index=%u missing=1\n",(unsigned long)l.gesture,group,(unsigned)i);
        else {
            char first[24],last[24],selected[24],driver[24],cache[24];
            formatPoint(s->wire_first,first,sizeof(first));formatPoint(s->wire_last,last,sizeof(last));formatPoint(s->selected,selected,sizeof(selected));formatPoint(s->driver,driver,sizeof(driver));formatPoint(s->cache,cache,sizeof(cache));
            n=snprintf(line,sizeof(line),"[ACQ] id=%lu group=%s index=%u seq=%lu us=%lu m5_ms=%lu duration_us=%lu gate=%u core_irq=%d ft_irq=%d reads=%u failed=%u exhausted=%u selected_n=%d driver_n=%d cache_n=%d wire_first=%s wire_last=%s selected0=%s driver=%s cache=%s\n",
                (unsigned long)l.gesture,group,(unsigned)i,(unsigned long)s->sequence,(unsigned long)s->us,(unsigned long)s->m5_ms,(unsigned long)s->duration,s->gate,s->core_irq,s->ft_irq,s->reads,s->failed,s->exhausted,s->selected_n,s->driver_n,s->cache_n,first,last,selected,driver,cache);
        }
    } else if(row<finalRow()) {
        const size_t group=(row-23)/7,index=(row-23)%7;const Cycle& c=group==0?l.initial:group==1?l.motion:l.anomaly;
        bool valid=group==0 || (group==1?l.motion_saved:l.anomaly_saved);
        if(!valid)n=snprintf(line,sizeof(line),"[ACQ] id=%lu detail=%u index=%u missing=1\n",(unsigned long)l.gesture,(unsigned)group,(unsigned)index);
        else if(!index)n=snprintf(line,sizeof(line),"[ACQ] id=%lu detail=%u seq=%lu start_us=%lu end_us=%lu m5_ms=%lu gate=%u core_irq=%d ft_irq=%d init_ok=%d released_gate=%u core_calls=%u ft_calls=%u clear_calls=%u comparisons=%u mismatches=%u retry_left=%u reads=%u overflow=%u\n",
            (unsigned long)l.gesture,(unsigned)group,(unsigned long)c.sequence,(unsigned long)c.start,(unsigned long)c.end,(unsigned long)c.m5_ms,c.gate,c.core_irq,c.ft_irq,c.init_ok,c.released_gate,c.core_calls,c.ft_calls,c.irq_clear,c.comparisons,c.mismatches,c.retry_left,c.read_count,c.read_overflow);
        else if(index-1<c.read_count) {
            const auto& r=c.reads[index-1];char p[24],hex[11]={};formatPoint(point(r.frame,l.point_id),p,sizeof(p));
            unsigned prefix=r.length<5?r.length:5;for(unsigned j=0;j<prefix;++j)snprintf(hex+j*2,sizeof(hex)-j*2,"%02x",r.prefix[j]);
            n=snprintf(line,sizeof(line),"[ACQ] id=%lu detail=%u read=%u seq=%lu start_us=%lu end_us=%lu bytes=%u passed=%u failed_op=%u payload_valid=%u contacts=%u point=%s prefix_n=%u hex=%s\n",
                (unsigned long)l.gesture,(unsigned)group,(unsigned)(index-1),(unsigned long)c.sequence,(unsigned long)r.start,(unsigned long)r.end,r.length,r.passed,r.failed,r.frame.valid,r.frame.count,p,prefix,prefix?hex:"-");
        } else n=snprintf(line,sizeof(line),"[ACQ] id=%lu detail=%u read=%u missing=1\n",(unsigned long)l.gesture,(unsigned)group,(unsigned)(index-1));
    } else n=snprintf(line,sizeof(line),"[ACQ_END] id=%lu\n",(unsigned long)l.gesture);
    length=n>0?size_t(n):0;if(length>=sizeof(line))length=sizeof(line)-1;sent=0;
}
static_assert(sizeof(ledgers)+sizeof(cycle)+sizeof(complete)<=8192,"Acquisition ledger budget exceeded");
}
uint32_t nowUs(){return uint32_t(esp_timer_get_time());}
void enter(uint32_t ms){new(&cycle)Cycle();cycle.sequence=++sequence;cycle.m5_ms=ms;cycle.start=nowUs();in_cycle=true;read_index=-1;}
void leave(){cycle.end=nowUs();complete=cycle;if(active>=0)accumulate(ledgers[active],cycle);in_cycle=false;read_index=-1;}
void gate(){if(in_cycle)cycle.gate=true;}
void core(){if(in_cycle)++cycle.core_calls;}
void ft(unsigned n){if(in_cycle){++cycle.ft_calls;cycle.requested=n;}}
bool pin(bool v,bool outer){if(in_cycle){if(outer)cycle.core_irq=v;else cycle.ft_irq=v;}return v;}
bool checked(Op op,bool value){if(in_cycle){if(op==Op::init)cycle.init_ok=value;else if(read_index>=0){auto& r=cycle.reads[read_index];if(value)r.passed|=uint8_t(1u<<(unsigned(op)-1));else if(!r.failed)r.failed=unsigned(op);}}return value;}
void released(bool v){if(in_cycle)cycle.released_gate=v;}
void irqClear(){if(in_cycle)++cycle.irq_clear;}
void readEnter(){if(!in_cycle)return;if(cycle.read_count>=6){++cycle.read_overflow;read_index=-1;return;}read_index=cycle.read_count++;cycle.reads[read_index].start=nowUs();}
void readLeave(const uint8_t* p,size_t n){if(in_cycle && read_index>=0){auto& r=cycle.reads[read_index];r.end=nowUs();r.length=n;r.frame=decode(p,n);for(size_t i=0;i<n && i<5;++i)r.prefix[i]=p[i];}read_index=-1;}
bool repeat(bool v){if(in_cycle){++cycle.comparisons;cycle.mismatches+=v;}return v;}
void selected(const uint8_t* p,size_t n,int retries){if(in_cycle){cycle.selected=decode(p,n);cycle.retry_left=retries;}}
void publish(const Frame& f,bool cache){if(in_cycle){if(cache)cycle.cache=f;else cycle.driver=f;}}
void beginGesture(uint32_t id,uint16_t p){if(active>=0 || !complete.sequence)return;int i=!ready[0]?0:!ready[1]?1:-1;if(i<0)return;active=i;auto& l=*new(&ledgers[i])Ledger();l.gesture=id;l.point_id=p;l.start=complete.start;l.capture=nowUs();l.initial=complete;accumulate(l,complete);}
void endGesture(uint32_t id){if(active>=0 && ledgers[active].gesture==id){ledgers[active].end=nowUs();ready[active]=true;active=-1;}}
const Ledger* pending(uint32_t id){for(int i=0;i<2;++i)if(ready[i] && ledgers[i].gesture==id)return &ledgers[i];return nullptr;}
const Cycle& latest(){return complete;}
bool service(uint32_t id) {
    const auto* l=pending(id);if(!l)return false;if(!Serial)return true;
    uint32_t now=millis();if(uint32_t(now-last_tx)<4)return true;last_tx=now;
    int space=Serial.availableForWrite();if(space<=0)return true;if(sent==length)prepare(*l);
    size_t n=length-sent;if(n>40)n=40;if(n>size_t(space))n=space;
    sent+=Serial.write(reinterpret_cast<const uint8_t*>(line+sent),n);
    if(sent==length){if(row==finalRow()){ready[l==&ledgers[0]?0:1]=false;row=0;}else ++row;}
    return true;
}
}
#endif
