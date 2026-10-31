#include "captured_fader_touch.h"
#include "M5Unified.hpp"
#include <assert.h>
#include <stdio.h>
uint32_t fake_us=0;FakeSerial Serial;FakeM5 M5;
void update(uint32_t ms,int y) {
    fake_us=ms*1000;M5.Display.point.x=290;M5.Display.point.y=y;M5.Display.down=true;
    M5.Touch.update(ms);
}
void expect(uint8_t state,int raw_y,int detail_y) {
    const auto detail=M5.Touch.getDetail();assert(uint8_t(detail.state)==state && detail.y==detail_y);
    auto owner=detail;int y=-1;unsigned reads=M5.Display.reads;
    assert(captured_fader_touch::read(M5.Touch,M5.Display,0,owner,y) && y==raw_y);
    assert(M5.Display.reads==reads && owner.state==detail.state && M5.Touch.getDetail().y==detail_y);
}
int main() {
    M5.Touch.begin(&M5.Display);update(10,109);expect(3,109,109);
    update(11,109);expect(1,109,109);update(15,100);expect(11,100,100);
    update(16,100);expect(9,100,100);update(1100,99);expect(9,99,99);
    // Flick remains flick beyond 500ms: no automatic time-based upgrade to drag.
    M5=FakeM5{};M5.Touch.begin(&M5.Display);update(10,109);update(11,109);
    update(510,109);expect(1,109,109);update(511,109);expect(7,109,109);
    update(512,109);expect(5,109,109);update(515,108);expect(5,108,109);
    update(520,107);expect(5,107,109);update(550,100);expect(15,100,100);
    update(551,100);expect(13,100,100);update(555,99);expect(13,99,99);
    M5.Display.down=false;M5.Touch.update(560);assert(M5.Touch.getDetail().state==m5::touch_state_t::drag_end);
    auto owner=M5.Touch.getDetail();int y=999;
    assert(!captured_fader_touch::read(M5.Touch,M5.Display,0,owner,y) && y==999);
    puts("PASS exact installed gesture machine: >500ms hold, movement-before-hold flick 9 remains 9, movement-after-hold drag 13, same raw updates in both; captured path also follows small points during hold without global mutation");
}
