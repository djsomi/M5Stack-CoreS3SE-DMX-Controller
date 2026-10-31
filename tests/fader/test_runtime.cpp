#include "physical_fader_diagnostics.h"
#include "Arduino.h"
#include "M5Unified.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t fake_us=0;FakeSerial Serial;FakeM5 M5;
void poll(int y,bool down=true) {
    fake_us+=5000;M5.Display.point.x=290;M5.Display.point.y=y;M5.Display.down=down;
    M5.Touch.update(millis());physical_fader::captureTouch();
}
int main() {
    M5.Touch.begin(&M5.Display);poll(100);
    physical_fader::mapped(physical_fader::Page::main,1,139,139);
    for(int y=101;y<=108;++y) {
        poll(y);assert(M5.Touch.getDetail().y==100);
        assert(M5.Touch.getTouchPointRaw().y==y);
        assert(!physical_fader::service() && Serial.writes==0);
    }
    poll(109);assert(M5.Touch.getDetail().y==109);
    physical_fader::mapped(physical_fader::Page::main,1,121,139);
    physical_fader::accepted(1,139,121);fake_us+=100;
    physical_fader::visual(physical_fader::Page::main,121);
    // Existing M5 <=4ms gate returns the old raw point, no sensor query.
    const auto reads=M5.Display.reads;M5.Display.point.y=110;
    M5.Touch.update(millis()+1);assert(M5.Display.reads==reads);
    poll(110);assert(M5.Touch.getDetail().y==110);
    poll(110,false);poll(110,false);
    Serial.space=0;physical_fader::service();assert(!Serial.writes);
    Serial.space=17;Serial.connected=false;physical_fader::service();assert(!Serial.writes);
    Serial.connected=true;
    for(int i=0;i<4000;++i) {fake_us+=4000;if(!physical_fader::service())break;}
    assert(strstr(Serial.output,"[FADER_END] id=1\n"));
    assert(strstr(Serial.output,"held_raw=8"));assert(strstr(Serial.output,"max_map_delta=18"));
    assert(strstr(Serial.output,"metric=cached_poll_gap"));
    assert(Serial.max_write<=17 && Serial.used<sizeof(Serial.output));
    const auto used=Serial.used;for(int i=0;i<200;++i){fake_us+=4000;physical_fader::service();}
    assert(used==Serial.used);
    puts("PASS exact installed M5Unified touch gate/deadband: 8 held pixels then 18-value mapping jump; next pixel 2 values; runtime drains once after release, no host waits/continuous logging/extra sensor reads");
}
