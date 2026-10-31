#include "physical_fader_diagnostics.h"
#include <assert.h>
#include <stdio.h>
using namespace physical_fader;
Touch sample(uint32_t at,int y,uint8_t state) {
    Touch t;t.us=at;t.m5_ms=at/1000;t.raw_x=t.unfiltered_x=t.x=290;
    t.raw_y=t.unfiltered_y=y;t.y=y;t.state=state;return t;
}
int value(int y){int v=255-2*(y-42);return v<0?0:v>255?255:v;}
int main() {
    Recorder r;const uint32_t start=10000;int current=121;
    r.loopStart(start);r.observe(sample(start,109,3));r.mapped(Page::main,1,current,current,start+10);
    for(uint32_t age=1000;age<=3000000;age+=1000) {
        int y=age<1000000?109-int(age>600000?(age-600000)/100000:0):
              age<1500000?105-int((age-1000000)/25000):
              age<2000000?85-int((age-1500000)/10000):35-int((age-2000000)/5000);
        uint8_t state=age<501000?1:age==501000?7:age<1125000?5:age==1125000?15:13;
        auto t=sample(start+age,y,state);t.raw_x=t.unfiltered_x=290+int((age/30000)%2);
        if(age<1125000)t.y=109;
        r.loopStart(t.us);r.observe(t);r.duration(Stage::m5,30,t.us+40);r.duration(Stage::owner,50,t.us+90);
        int next=value(y);r.mapped(Page::main,1,next,current,t.us+100);
        if(next!=current){r.accepted(1,current,next,t.us+150);r.visual(Page::main,next,t.us+200);current=next;}
    }
    r.observe(sample(start+3001000,-165,14));r.observe(sample(start+3002000,-165,0));
    const auto* p=r.pending();assert(p && p->overwritten>0 && p->onset_count==61);
    assert(p->onset[0].touch.us==start && p->onset[60].touch.us==start+1500000);
    assert(p->onset[44].touch.y==109 && p->onset[44].touch.unfiltered_y==101);
    assert(p->onset[45].touch.state==15 && p->onset[45].accepted==139);
    assert(p->onset[45].accept_us==start+1125150 && p->onset[45].render_us==start+1125200);
    assert(p->transition_count==8 && p->transition_overflow==0);
    assert(p->transitions[2].edge.at==start+501000 && p->transitions[2].edge.touch.m5_ms==511);
    assert(p->transitions[4].from==5 && p->transitions[4].edge.touch.state==15);
    assert(p->transitions[4].edge.rendered==139 && p->transitions[4].edge.render_us==start+1125200);
    assert(p->transitions[4].has_after && p->transitions[4].after.touch.state==13);
    assert(p->last_pressed_state==13 && p->last_pressed_us==start+3000000);
    uint32_t moves=0;for(const auto& s:p->speeds){assert(s.moves);moves+=s.moves;}
    assert(moves==p->y_moves && p->raw_moves>p->y_moves);
    assert(p->periods[0].polls==249 && p->periods[1].polls==250 && p->periods[4].polls==500);
    assert(p->periods[2].max_y_gap_us==700000 && p->periods[2].max_poll_us==1000);
    assert(p->periods[5].polls && p->periods[5].y_moves);
    assert(p->unchanged_y.max_us==700000 && p->unchanged_y.total_us==3001000);
    assert(p->unchanged_y.ge100ms>=4 && p->unchanged_xy.max_us<p->unchanged_y.max_us);
    // Same-coordinate duration alone does not identify stationary hardware.
    Recorder regular;regular.loopStart(1000);regular.observe(sample(1000,100,3));regular.mapped(Page::main,1,139,139,1010);
    for(unsigned i=1;i<=400;++i){regular.loopStart(1000+i*1000);regular.observe(sample(1000+i*1000,100,1));}
    regular.observe(sample(402000,100,0));const auto* q=regular.pending();
    assert(q->periods[1].max_poll_us==1000 && q->unchanged_y.max_us==401000);
    Recorder delayed;delayed.loopStart(1000);delayed.observe(sample(1000,100,3));delayed.mapped(Page::main,1,139,139,1010);
    delayed.loopStart(401000);delayed.observe(sample(401000,100,1));delayed.duration(Stage::m5,399000,401000);
    delayed.observe(sample(402000,100,0));q=delayed.pending();
    assert(q->periods[1].max_poll_us==400000 && q->periods[1].max_loop_us==400000);
    assert(!q->onset_used[1] && !q->onset_used[15] && q->onset_used[16]);
    assert(q->periods[1].max_m5_us==399000 && q->unchanged_y.max_us==401000);
    // Excess transitions are explicitly counted, never overwrite retained edges.
    Recorder noisy;noisy.observe(sample(1000,100,3));noisy.mapped(Page::main,1,139,139,1010);
    for(unsigned i=0;i<50;++i)noisy.observe(sample(2000+i*1000,100,i%2?9:13));
    noisy.observe(sample(60000,100,0));q=noisy.pending();
    assert(q->transition_overflow>0 && q->transitions[0].edge.touch.us==1000);
    // A missing observation window remains missing rather than interpolated.
    assert(!q->onset_used[2] || q->onset[2].touch.us>=51000);
    puts("PASS early 1.5s survives tail overwrite; exact observed state edges/followups and response times; six age/four speed buckets; Y vs XY and unchanged-run/app-gap distinction; bounded overflow/missing windows");
}
