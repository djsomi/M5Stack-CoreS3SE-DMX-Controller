#include "physical_fader_diagnostics.h"
#include <assert.h>
#include <stdio.h>
using namespace physical_fader;
Touch sample(uint32_t now,int raw,int processed,uint8_t state=1) {
    Touch t;t.us=now;t.raw_x=t.unfiltered_x=t.x=290;t.raw_y=t.unfiltered_y=raw;
    t.y=processed;t.state=state;return t;
}
int main() {
    Recorder r;r.loopStart(100);r.observe(sample(100,100,100));
    r.mapped(Page::main,512,139,139,110);
    r.observe(sample(5100,105,100));r.duration(Stage::m5,321);
    r.loopStart(5200);r.observe(sample(10100,109,109));
    r.mapped(Page::main,512,121,139,10200);r.accepted(512,139,121,10300);
    r.visual(Page::precision,121,10400); // Wrong surface must not count.
    r.visual(Page::main,121,10500);r.visual(Page::main,121,10600);
    r.observe(sample(15100,110,110));r.mapped(Page::main,512,119,121,15200);
    r.accepted(512,121,119,15300);r.visual(Page::main,119,15500);
    r.observe(sample(20100,110,110,2));r.observe(sample(25100,110,110,0));
    const auto* p=r.pending();assert(p && p->channel==512 && p->release_edges==1);
    assert(p->raw_moves==3 && p->held_raw_moves==1 && p->processed_moves==2);
    assert(p->max_map_delta==18 && p->skipped_values==18 && p->jumps10==1);
    assert(p->accepted_changes==2 && p->accepted_gaps.maximum==5000);
    assert(p->visual_lag.count==2 && p->visual_lag.maximum==200);
    assert(p->touch_to_accept.maximum==200 && p->stages[0].maximum==5100);
    assert(p->stages[1].maximum==321 && !r.touchActive());
    r.releasePending();assert(!r.pending());
    // Initial tap reposition is reported separately, not counted as lost values.
    r.observe(sample(30000,42,42));r.mapped(Page::precision,1,255,0,30100);
    r.accepted(1,0,0,30200); // Rejected or unchanged: never claim an applied value.
    r.accepted(2,0,255,30300);r.visual(Page::main,255,30400);
    for(unsigned i=0;i<300;++i)r.observe(sample(31000+i*5000,42+(i%20),42+(i%20)));
    r.observe(sample(1600000,42,42,0));p=r.pending();
    assert(p && p->initial_delta==255 && !p->skipped_values && !p->accepted_changes);
    assert(p->unchanged_attempts==1 && p->overwritten>0 && p->first.kind=='B');
    // Both report slots full: bounded suppression until release, no overwrite.
    r.observe(sample(1700000,50,50));r.mapped(Page::main,1,1,0,1700001);
    r.observe(sample(1710000,50,50,0));const auto first=r.pending()->id;
    r.observe(sample(1720000,50,50));r.mapped(Page::main,1,2,1,1720001);
    r.mapped(Page::main,1,3,1,1720002);assert(r.dropped==1 && r.pending()->id==first);
    r.observe(sample(1730000,50,50,0));r.releasePending();r.releasePending();
    // uint32 microsecond wrap is safe for sub-wrap gestures.
    Recorder wrap;wrap.observe(sample(UINT32_MAX-100,100,100));
    wrap.mapped(Page::main,1,0,0,UINT32_MAX-90);wrap.accepted(1,0,1,UINT32_MAX-50);
    wrap.observe(sample(49,99,99));wrap.accepted(1,1,2,99);wrap.observe(sample(149,99,99,0));
    assert(wrap.pending()->accepted_gaps.maximum==150 && wrap.pending()->polls.maximum==150);
    printf("PASS recorder: held raw vs detail, skips vs initial tap, accepted/render correlation, stage gaps, bounded tail/queue, rejection, page isolation and wrap; bytes=%u\n",unsigned(sizeof(Recorder)));
}
