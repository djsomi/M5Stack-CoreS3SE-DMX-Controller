#include "fullscreen_canvas_memory.h"
#include "guard_under_test.h" // Generated from the actual unchanged network guard.
#include <assert.h>
#include <stdio.h>
#include <string>

namespace {
char external_buffer[8],internal_buffer[8];
bool external(const void* p) { return p==external_buffer; }
struct Canvas {
    void* next=external_buffer;
    int creates=0,deletes=0,depth=0,width=0,height=0;
    bool psram=false;
    std::string order;
    void setColorDepth(int n) {depth=n;order+='D';}
    void setPsram(bool n) {psram=n;order+='P';}
    void* createSprite(int w,int h) {
        assert(depth==16 && psram);width=w;height=h;++creates;order+='C';return next;
    }
    void deleteSprite() {++deletes;order+='X';}
};
}
int main() {
    Canvas good;bool attempted=false;
    void* retained=fullscreen_canvas_memory::allocateOnce(good,attempted,320,240,external);
    assert(retained==external_buffer && attempted && good.order=="DPC");
    assert(good.width==320 && good.height==240 && good.creates==1 && !good.deletes);
    // Caller caches the valid sprite across all pages; accidental re-entry must
    // neither allocate nor delete the live buffer or change its RGB565 content.
    external_buffer[0]=char(0xf8);external_buffer[1]=char(0x1f);
    for(int i=0;i<1000;++i)
        assert(!fullscreen_canvas_memory::allocateOnce(good,attempted,320,240,external));
    assert(good.creates==1 && good.deletes==0 && external_buffer[1]==char(0x1f));
    Canvas fallback;fallback.next=internal_buffer;attempted=false;
    assert(!fullscreen_canvas_memory::allocateOnce(fallback,attempted,320,240,external));
    assert(fallback.order=="DPCX" && fallback.deletes==1);
    assert(!fullscreen_canvas_memory::allocateOnce(fallback,attempted,320,240,external));
    assert(fallback.creates==1 && fallback.deletes==1); // No failed-redraw heap loop.
    Canvas exhausted;exhausted.next=nullptr;attempted=false;
    assert(!fullscreen_canvas_memory::allocateOnce(exhausted,attempted,320,240,external));
    for(int i=0;i<1000;++i)
        assert(!fullscreen_canvas_memory::allocateOnce(exhausted,attempted,320,240,external));
    assert(exhausted.creates==1 && exhausted.deletes==0);
    // Execute the real production predicate, including the recorded failing case.
    using guard_under_test::Memory;
    Memory m{};m.internal=22316;m.dma=14324;
    assert(!guard_under_test::safe(m));
    m.internal=32768;m.dma=16384;assert(guard_under_test::safe(m));
    --m.internal;assert(!guard_under_test::safe(m));
    ++m.internal;--m.dma;assert(!guard_under_test::safe(m));
    ++m.dma;m.psram=8388608;assert(guard_under_test::safe(m));
    m.internal=22316;assert(!guard_under_test::safe(m)); // PSRAM cannot satisfy internal reserve.
    puts("PASS real fullscreen allocation policy: explicit RGB565/PSRAM, pointer validation, fallback retirement, bounded failure/reuse; actual unchanged heap guard boundaries and failing hardware baseline");
}
