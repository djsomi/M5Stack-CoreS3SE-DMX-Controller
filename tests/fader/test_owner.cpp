#include "physical_fader_diagnostics.h"
#include "M5Unified.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t fake_us=0;FakeSerial Serial;FakeM5 M5;
void poll(){fake_us+=5000;M5.Touch.update(millis());unsigned reads=M5.Display.reads;physical_fader::captureTouch();assert(reads==M5.Display.reads);}
int main() {
    M5.Touch.begin(&M5.Display);M5.Display.down=true;M5.Display.point.x=290;M5.Display.point.y=109;poll();
    physical_fader::mapped(physical_fader::Page::main,1,121,121);
    M5.Display.second=true;M5.Display.reversed=true;M5.Display.second_point.id=1;M5.Display.second_point.x=200;M5.Display.second_point.y=200;
    M5.Display.point.y=108;poll();physical_fader::mapped(physical_fader::Page::main,1,123,121);
    physical_fader::accepted(1,121,123);physical_fader::visual(physical_fader::Page::main,123);
    M5.Display.second=false;M5.Display.reversed=false;M5.Display.point.id=1;M5.Display.point.y=200;poll();
    for(int i=0;i<20;++i){fake_us+=4000;physical_fader::service();}
    assert(Serial.writes==0 && physical_fader::touchActive());
    M5.Display.down=false;poll();poll();assert(!physical_fader::touchActive());
    for(int i=0;i<5000;++i){fake_us+=4000;if(!physical_fader::service())break;}
    assert(strstr(Serial.output,"[FADER_END] id=1\n"));
    assert(strstr(Serial.output,"unfiltered=290,108") && strstr(Serial.output,"accepted=123"));
    assert(strstr(Serial.output,"state=2 point=0") && strstr(Serial.output,"state=0 point=0"));
    assert(!strstr(Serial.output,"point=1"));
    // A released detail may remain addressable through a stale raw-array slot.
    // Separately exercise true cache absence with an ID not in that array.
    Serial.used=Serial.writes=0;Serial.output[0]=0;M5=FakeM5{};M5.Touch.begin(&M5.Display);
    M5.Display.down=true;M5.Display.point.id=2;M5.Display.point.x=290;M5.Display.point.y=109;poll();
    physical_fader::mapped(physical_fader::Page::main,1,121,121);
    M5.Display.point.id=1;M5.Display.point.y=200;poll();
    assert(physical_fader::touchActive() && Serial.writes==0);
    M5.Display.down=false;poll();poll();
    for(int i=0;i<5000;++i){fake_us+=4000;if(!physical_fader::service())break;}
    assert(strstr(Serial.output,"[FADER_END] id=2\n") && strstr(Serial.output,"cache_match=0"));
    assert(!strstr(Serial.output,"point=1"));
    puts("PASS diagnostic identity follows captured point through raw ordering, flags missing cache, never records the other finger and defers all output until all contacts release");
}
