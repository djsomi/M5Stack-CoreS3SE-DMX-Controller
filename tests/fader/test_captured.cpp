// Real Main fader slices + Controller/Dispatcher + exact installed Touch_Class.
// Sensor, drawing and wire publication are deliberately mocked, not hardware.
#include "captured_fader_touch.h"
#include "physical_fader_diagnostics.h"
#include "Arduino.h"
#include "M5Unified.hpp"
#include "dmx_controller.h"
#include "command_dispatcher.h"
#include "../support/fake_preset_storage.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#if defined(FADER_REGRESSION_BEFORE)
#undef assert
#define assert(condition) do {if(!(condition)){fprintf(stderr,"legacy regression: %s\n",#condition);exit(1);}} while(false)
#endif
uint32_t fake_us=0;FakeSerial Serial;FakeM5 M5;

struct Main {
    FakeStorage storage;DmxController controller;CommandDispatcher dispatcher;
    bool main_value_slider_dragging=false,main_touch_consume_release=false;
    uint8_t main_value_slider_touch_id=0;uint16_t target_channel=1;int target_value=0;
    unsigned publications=0,bars=0,grids=0,edit_opens=0;bool other_page=false;
    Main():controller(storage),dispatcher(controller) {
        M5=FakeM5{};M5.Touch.begin(&M5.Display);dispatcher.setSenderActive(true);
        controller.setObserver(changed,this);dispatcher.setPublisher(published,this);
    }
    static void changed(void* c,const DmxController::Change&) {auto& m=*static_cast<Main*>(c);m.target_value=m.controller.channel(m.target_channel);}
    static int published(void* c,const ControllerCommand&) {++static_cast<Main*>(c)->publications;return 0;}
    bool pointInRect(int x,int y,int left,int top,int w,int h) {return x>=left && x<left+w && y>=top && y<top+h;}
    void drawUIValueSet(int) {++bars;}
    void updateDisplay() {++grids;}
    void openPrecisionEditor() {++edit_opens;other_page=true;}
    #include "captured_main_under_test.h"
    void poll(int x,int y,bool down=true,uint8_t id=0) {
        fake_us+=5000;M5.Display.point.x=x;M5.Display.point.y=y;M5.Display.point.id=id;M5.Display.down=down;
        M5.Touch.update(millis());FADER_TOUCH();frame();
    }
    void neutral() {fake_us+=1000;M5.Touch.update(millis());FADER_TOUCH();frame();}
    void release() {M5.Display.second=false;poll(290,100,false);}
};
int main() {
    {
        Main m;m.poll(290,109);assert(m.target_value==121 && m.main_value_slider_dragging);
        unsigned n=m.publications;
        for(int dy=1;dy<=7;++dy) {
            m.poll(290,109-dy);assert(M5.Touch.getDetail().y==109);
            assert(m.target_value==121+2*dy && m.publications==n+unsigned(dy));
        }
        for(int y:{103,104,103,102}) {
            m.poll(290,y);assert(M5.Touch.getDetail().y==109);
            assert(m.target_value==255-2*(y-42));
        }
        m.poll(290,98);assert(M5.Touch.getDetail().y==98 && m.target_value==143);
        for(int y=99;y<=105;++y){m.poll(290,y);assert(m.target_value==255-2*(y-42));}
        n=m.publications;auto b=m.bars,g=m.grids;for(int i=0;i<50;++i)m.poll(290,105);
        assert(n==m.publications && b==m.bars && g==m.grids);
        const auto reads=M5.Display.reads;auto owner=M5.Touch.getDetail();int y=0;
        assert(captured_fader_touch::read(M5.Touch,M5.Display,0,owner,y) && y==105);
        assert(M5.Display.reads==reads); // cached copy conversion, no extra acquisition.
        M5.Touch.update(millis()+1);m.frame();
        assert(M5.Display.reads==reads && m.publications==n && m.bars==b && m.grids==g);
        assert(m.publications==m.bars && m.bars==m.grids);
        m.release();assert(!m.main_value_slider_dragging);
        assert(M5.Touch.getDetail().wasReleased()); // Never mutate the library state.
        // Normal 1ms app turn retires the library's release state before repress.
        m.neutral();
        m.poll(290,109);assert(m.main_value_slider_dragging && m.target_value==121);
        m.release();
    }
    {
        Main m;assert(m.controller.setChannel(1,121)==DmxController::Result::ok);
        m.poll(290,109);m.release();assert(!m.publications && !m.bars && !m.grids);
    }
    {
        Main m;m.poll(290,170);unsigned previous=m.publications;
        for(int y=169;y>=42;--y) {m.poll(290,y);assert(m.target_value==255-2*(y-42));}
        assert(m.target_value==255 && m.publications==previous+128);
        for(int y=43;y<=170;++y)m.poll(290,y);
        assert(m.target_value==0);
        // Fast moves, clamps and endpoints; no interpolation is submitted.
        previous=m.publications;m.poll(290,-100);assert(m.target_value==255);
        m.poll(290,300);assert(m.target_value==0);assert(m.publications==previous+2);
        m.release();
    }
    {
        Main m;m.poll(290,109);auto n=m.publications;
        m.release();assert(m.publications==n && !m.edit_opens); // tap/release no extra write
        // Capture by press origin only: entering from grid cannot capture.
        m.poll(200,109);m.poll(290,100);assert(!m.main_value_slider_dragging && m.publications==n);
        m.release();m.neutral();m.poll(280,200);m.poll(290,100);m.release();assert(m.edit_opens==0);
        // A genuine stationary EDIT tap retains its original click semantics.
        m.neutral();m.poll(280,200);m.poll(280,200,false);assert(m.edit_opens==1);
    }
    {
        Main m;m.poll(290,109);auto n=m.publications;
        // Captured moves across neighboring grid/EDIT controls stay exclusive.
        m.poll(100,108);assert(m.target_value==123 && !m.edit_opens);
        m.poll(280,200);m.poll(280,200,false);assert(!m.edit_opens && !m.main_value_slider_dragging);
        assert(m.publications==n+2);
        // Explicit existing UI cancellation cannot recapture a held finger.
        m.neutral();m.poll(290,109);m.main_value_slider_dragging=false;m.main_touch_consume_release=true;
        n=m.publications;m.poll(290,108);assert(m.publications==n);m.release();
    }
    {
        Main m;m.poll(290,109, true,2);
        // A second pointer moves ahead in the raw array; retain captured ID 2.
        M5.Display.second=true;M5.Display.second_point.x=200;M5.Display.second_point.y=80;
        M5.Display.second_point.id=1;M5.Display.reversed=true;
        m.poll(290,108,true,2);assert(m.target_value==123 && m.main_value_slider_touch_id==2);
        auto n=m.publications;
        // Original point disappears: never follow/activate the other point.
        M5.Display.second=false;M5.Display.reversed=false;m.poll(280,200,true,1);
        assert(m.publications==n && !m.main_value_slider_dragging && m.main_touch_consume_release);
        m.poll(280,200,false,1);m.poll(280,200,false,1);
        assert(!m.edit_opens && !m.main_touch_consume_release);
        m.poll(290,100,true,1);assert(m.main_value_slider_dragging && m.target_value==139);
        m.release();
    }
    {
        Main m;M5.Display.swap_axes=true;m.poll(109,290);
        assert(m.target_value==121);m.poll(108,290);assert(m.target_value==123);
        assert(M5.Touch.getTouchPointRaw().x==108 && M5.Touch.getDetail().y==109);
        // Same threshold outside captured Main input: no global state changes.
        m.other_page=true;auto n=m.publications;m.poll(107,290);
        assert(M5.Touch.getDetail().y==109 && m.publications==n);
    }
    puts("PASS actual Main slices + Controller/Dispatcher + installed M5: 1-7px, threshold/reverse, full range/fast/clamp, release/repress, tap/no duplicates, hitbox/neighbor/cancel, reordered/lost IDs, raw conversion and unchanged library/other-page gestures");
}
