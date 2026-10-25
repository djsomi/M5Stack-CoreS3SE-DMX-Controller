#include "wifi_startup_trace_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace wifi_startup_trace;
int main() {
    History<2> h;
    Entry a{"before",{1,2,3,4,5,6,7,8},nullptr,0,0,false,true};
    h.append(a);
    assert(h.count==1 && !h.first_present && h.entries[0].before);
    a.step="guard.preflight";a.result=257;a.failed=true;a.before=false;a.detail=2;
    h.append(a);
    assert(h.first_present && h.first.result==257 && h.first.detail==2);
    a.step="phase.FAILED";a.result=-1;h.append(a);
    assert(h.count==2 && h.dropped==1);
    assert(!strcmp(h.first.step,"guard.preflight") && h.first.result==257);
    assert(h.entries[0].memory.dma_free==5 && h.entries[0].memory.dma_largest==6);
    History<1> full;
    a.failed=false;full.append(a);
    a.failed=true;a.step="esp_wifi_init";a.result=257;full.append(a);
    assert(full.count==1 && full.dropped==1 && full.first_present);
    assert(!strcmp(full.first.step,"esp_wifi_init")); // Error retained even at capacity.
    for(int i=0;i<1000;++i) full.append(a);
    assert(full.count==1 && full.dropped==1001 && full.first.result==257);
    assert(full.first.memory.psram==8 && full.first.memory.minimum==7);
    puts("PASS bounded startup history, distinct free/largest DMA metrics, before/after, first error retention and overflow without overwrite");
}
