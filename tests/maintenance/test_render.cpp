// Compile the actual physical maintenance methods against a pixel/clip LCD fake.
// Tests ownership/transfers/clears, not real font rasterization, DMA or panel timing.
#include "device_maintenance.h"
#include <assert.h>
#include <algorithm>
#include <array>
#include <stdio.h>
#include <string.h>
using M=DeviceMaintenance;
uint32_t now_ms=0;uint32_t millis(){return now_ms;}
enum {TFT_BLACK=0,TFT_WHITE=1,TFT_RED=2};
enum class textdatum_t {middle_center};
namespace fonts {const int Font0=0,Font4=4;}
bool lcd_busy=false;
struct Touch {bool state=false,pressed=false;int x=0,y=0;bool wasPressed()const{return pressed;}};
struct Surface {
    std::array<unsigned char,320*240> pixels{};
    int cx=0,cy=0,cw=320,ch=240;unsigned full=0,partial=0,waits=0;
    void clearClipRect(){cx=cy=0;cw=320;ch=240;}
    void setClipRect(int x,int y,int w,int h){cx=x;cy=y;cw=w;ch=h;}
    void fillRect(int x,int y,int w,int h,int value){for(int yy=std::max(y,cy);yy<std::min(y+h,cy+ch);++yy)for(int xx=std::max(x,cx);xx<std::min(x+w,cx+cw);++xx)pixels[yy*320+xx]=value;}
    void fillScreen(int color){assert(!lcd_busy);fillRect(0,0,320,240,color);}
    void setTextSize(int){} void setTextDatum(textdatum_t){} void setTextColor(int,int){} void setFont(const int*){} void setTextWrap(bool){}
    void drawString(const char*,int x,int y){fillRect(x-20,y-3,40,6,11);}
    void drawNumber(int n,int,int){fillRect(145,90,30,30,n+10);}
    void startWrite(){} void endWrite(){} void waitDisplay(){lcd_busy=false;++waits;}
    void pushSprite(Surface* target,int,int){
        assert(!lcd_busy);
        if(target->cw==320&&target->ch==240)++target->full;else ++target->partial;
        for(int y=target->cy;y<target->cy+target->ch;++y)for(int x=target->cx;x<target->cx+target->cw;++x)target->pixels[y*320+x]=pixels[y*320+x];
    }
};
struct FakeM5 {Surface Display;struct T {Touch detail;Touch getDetail(){return detail;}} Touch;struct S {bool active=false;bool isPlaying(){return active;}} Speaker;} M5;
struct Backend : M::Backend {
    bool* open=nullptr;
    bool busy()override{return open&&*open;}void begin(bool)override{}
    M::Step zeroFrame(int64_t)override{return M::Step::ready;}M::Step quiesce(int64_t)override{return M::Step::ready;}void execute(M::Action)override{assert(false);}
};
struct View {
    enum fullscreen_page_t {fullscreen_page_none,fullscreen_page_settings,fullscreen_page_precision,fullscreen_page_keypad,fullscreen_page_preset,fullscreen_page_maintenance};
    enum class ui_mode_t {mode_value_setting,mode_channel_select};
    Surface framebuffer;unsigned allocations=1,main_redraws=0;
    fullscreen_page_t active_fullscreen_page=fullscreen_page_none;
    bool settings_screen=false,precision_editor_screen=false,preset_screen=false,preset_options_screen=false;
    bool main_value_slider_dragging=false,precision_slider_dragging=false,settings_restart_tap_armed=false,settings_poweroff_tap_armed=false,controller_redraw_pending=true;
    int precision_repeat_dir=0,scroll_y_add=0,settings_scroll_add=0,target_value=87,precision_step=5;
    ui_mode_t ui_mode=ui_mode_t::mode_channel_select;
    struct P {void reset(){}} preset_double_tap;
    bool maintenance_ui_open=false,maintenance_released=false,maintenance_restored=false,maintenance_frame_ready=false;
    int maintenance_previous_page=0,maintenance_last_seconds=-1;M::Phase maintenance_phase=M::Phase::idle;
    void setActiveFullscreenPage(fullscreen_page_t page){active_fullscreen_page=page;}
    Surface* getFullscreenCanvas(fullscreen_page_t page,const char*){assert(page==active_fullscreen_page);return &framebuffer;}
    void pushFullscreenCanvas(fullscreen_page_t page,const char*){assert(page==active_fullscreen_page);framebuffer.pushSprite(&M5.Display,0,0);}
    void updateBatteryMonitor(){}
    void drawPrecisionButton(Surface* c,int x,int y,int w,int h,const char*){c->fillRect(x,y,w,h,22);}
    void page(int color){framebuffer.clearClipRect();framebuffer.fillScreen(color);framebuffer.pushSprite(&M5.Display,0,0);}
    void drawSettingsScreen(){page(3);}void drawPrecisionEditor(){page(4);}void drawPresetScreen(){page(5);}void drawPresetOptionsScreen(){page(6);}
    void updateDisplay(bool){++main_redraws;M5.Display.fillRect(0,0,250,240,7);}
    void drawUIValueSet(int,bool){M5.Display.fillRect(265,30,30,170,8);}
    void drawPresetOpenButton(){M5.Display.fillRect(250,0,70,240,9);}
#include "render_methods.h"
};
int main(){
    for(unsigned page=0;page<6;++page){
        View v;Backend b;b.open=&v.maintenance_ui_open;M m(b);M5.Display=Surface{};
        if(page==1){v.settings_screen=true;v.active_fullscreen_page=View::fullscreen_page_settings;}
        if(page==2||page==3){v.precision_editor_screen=true;v.active_fullscreen_page=page==2?View::fullscreen_page_precision:View::fullscreen_page_keypad;}
        if(page==4||page==5){v.preset_screen=true;v.preset_options_screen=page==5;v.active_fullscreen_page=View::fullscreen_page_preset;}
        if(page==0)v.ui_mode=View::ui_mode_t::mode_value_setting;
        const auto previous=v.active_fullscreen_page;auto* buffer=&v.framebuffer;
        now_ms=100;assert(m.request(M::Action::power_off,1,1,now_ms)==M::Result::accepted);
        M5.Touch.detail=Touch{};M5.Display.pixels.fill(77);M5.Display.setClipRect(0,0,8,8);lcd_busy=true;
        assert(v.serviceMaintenanceUi(m));assert(v.active_fullscreen_page==View::fullscreen_page_maintenance);
        assert(M5.Display.full==1&&M5.Display.waits>=1&&M5.Display.cw==320);
        assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),77)==M5.Display.pixels.end());
        const auto button=M5.Display.pixels[190*320+100];assert(button==22);
        for(unsigned i=0;i<25;++i){now_ms=101+i;m.poll(now_ms,now_ms*1000,true);v.serviceMaintenanceUi(m);}
        assert(M5.Display.full==1&&M5.Display.partial==0);
        now_ms=1100;m.poll(now_ms,now_ms*1000,true);v.serviceMaintenanceUi(m);
        assert(M5.Display.full==1&&M5.Display.partial==1&&M5.Display.pixels[190*320+100]==button&&M5.Display.cw==320);
        // Cancel on press; the stale countdown is never pushed after the state becomes idle.
        now_ms=5099;M5.Touch.detail.state=M5.Touch.detail.pressed=true;M5.Touch.detail.x=100;M5.Touch.detail.y=190;
        M5.Speaker.active=page==1;const auto full=M5.Display.full;v.serviceMaintenanceUi(m);assert(!m.active());
        if(page==1){assert(v.active_fullscreen_page==View::fullscreen_page_maintenance&&M5.Display.full==full);M5.Speaker.active=false;M5.Touch.detail.pressed=false;v.serviceMaintenanceUi(m);}
        assert(v.active_fullscreen_page==previous&&v.maintenance_ui_open&&v.maintenance_restored);
        assert(m.request(M::Action::restart,1,2,now_ms)==M::Result::busy); // Finish handoff/release first.
        assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),22)==M5.Display.pixels.end());
        assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),14)==M5.Display.pixels.end());
        const auto restore_full=M5.Display.full;v.serviceMaintenanceUi(m);assert(M5.Display.full==restore_full);
        M5.Touch.detail=Touch{};v.serviceMaintenanceUi(m);assert(!v.maintenance_ui_open);
        assert(v.target_value==87&&v.precision_step==5&&v.allocations==1&&buffer==&v.framebuffer);
        assert(m.request(M::Action::restart,1,3,now_ms)==M::Result::accepted);
        // A touch already down when the popup opens cannot act as CANCEL.
        M5.Touch.detail.state=M5.Touch.detail.pressed=true;v.serviceMaintenanceUi(m);assert(m.active());
        M5.Touch.detail=Touch{};v.serviceMaintenanceUi(m);
        now_ms+=5000;m.poll(now_ms,now_ms*1000,true);v.serviceMaintenanceUi(m);assert(m.committed());
        assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),22)==M5.Display.pixels.end());
    }
    puts("PASS extracted physical renderer: full clean handoff, old clip/DMA guard, bounded digit-only updates, immediate cancel, six-page full restore, speaker deferral, release consumption, repeated request and single canvas");
}
