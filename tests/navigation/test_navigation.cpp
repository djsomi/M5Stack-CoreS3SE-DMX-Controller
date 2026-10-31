// Compile actual complete Sender loop and entry/exit/ownership methods.
// Only peripheral/rendering/touch inputs are fakes; no panel timing/font claims.
#include "captured_fader_touch.h"
#include "physical_fader_diagnostics.h"
#include "dmx_controller.h"
#include "command_dispatcher.h"
#include "preset_double_tap.h"
#include "../support/fake_preset_storage.h"
#include <array>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"navigation regression: %s\n",#c);exit(1);}} while(false)
namespace m5 {enum touch_state_t {none=0};}
constexpr int TFT_BLACK=0;
uint32_t millis(){return 100;}
void delay(unsigned){}
struct Detail {
    int x=0,y=0,base_x=0,base_y=0,state=0,dy=0;uint8_t id=0;
    bool clicked=false,pressed=false,down=false;
    bool wasClicked()const{return clicked;}bool wasPressed()const{return pressed;}
    bool isPressed()const{return down;}int deltaY()const{return dy;}
};
struct Surface {
    std::array<uint8_t,320*240> pixels{};int cx=0,cy=0,cw=320,ch=240;
    unsigned pushes=0;
    int height()const{return 240;}
    void clearClipRect(){cx=cy=0;cw=320;ch=240;}
    void fillRect(int x,int y,int w,int h,int color){
        for(int yy=std::max(y,cy);yy<std::min(y+h,cy+ch);++yy)
            for(int xx=std::max(x,cx);xx<std::min(x+w,cx+cw);++xx)pixels[yy*320+xx]=uint8_t(color);
    }
    void fillScreen(int color){fillRect(0,0,320,240,color);}
    void startWrite(){}void endWrite(){}
    void convertRawXY(Detail*,unsigned){}
    void pushSprite(Surface* to,int,int){to->pixels=pixels;++to->pushes;}
};
struct Button {bool pressed=false,holding=false,clicked=false;
    bool wasPressed()const{return pressed;}bool isHolding()const{return holding;}bool wasClicked()const{return clicked;}};
struct FakeM5 {
    Surface Display;
    struct T {enum {TOUCH_MAX_POINTS=3};Detail detail;
        const Detail& getDetail(size_t=0)const{return detail;}
        Detail getTouchPointRaw(size_t=0)const{return detail;}
        size_t getCount()const{return detail.down?1:0;}} Touch;
    struct S {bool active=false;bool isPlaying()const{return active;}} Speaker;
    Button BtnA,BtnB,BtnC;
} M5;
#if !DMX_HW_DIAGNOSTICS
struct {template<class... A>int printf(const char*,A...){return 0;}} Serial;
#endif
struct Settings {struct State {uint32_t revision=1;};State snapshot(){return {};}};
Settings& deviceSettings(){static Settings s;return s;}
void persistDeviceSettings(){}

struct ViewBase {virtual bool loop()=0;};
struct View : ViewBase {
    enum fullscreen_page_t {fullscreen_page_none,fullscreen_page_settings,fullscreen_page_precision,fullscreen_page_keypad,fullscreen_page_preset};
    enum ui_mode_t {mode_channel_select,mode_value_setting};
    fullscreen_page_t active_fullscreen_page=fullscreen_page_none;
    ui_mode_t ui_mode=mode_channel_select;
    FakeStorage storage;DmxController controller;CommandDispatcher dispatcher;
    Surface fullscreen_canvas;void* fullscreen_canvas_buffer=&fullscreen_canvas;
    bool fullscreen_canvas_ready=false;
    unsigned allocations=0,main_draws=0,stale_actions=0,dmx_calls=0,battery_calls=0,settings_frames=0,precision_frames=0;
    bool wifi_power_active=false;
    bool settings_screen=false,settings_redraw_pending=false,precision_editor_screen=false,precision_keypad_screen=false;
    bool precision_redraw_pending=false,precision_consume_release=false,precision_slider_dragging=false;
    bool preset_screen=false,preset_options_screen=false,preset_options_consume_release=false,preset_copy_mode=false;
    bool preset_save_holding=false,preset_save_completed=false,preset_slot_holding=false,preset_clear_holding=false,preset_clear_completed=false;
    bool main_value_slider_dragging=false,main_touch_consume_release=false;
    uint8_t main_value_slider_touch_id=0;
    uint32_t settings_device_revision=1,settings_restart_first_tap_ms=0,settings_poweroff_first_tap_ms=0;
    uint32_t preset_save_hold_start_ms=0,preset_slot_hold_start_ms=0,preset_clear_hold_start_ms=0;
    bool settings_restart_tap_armed=false,settings_poweroff_tap_armed=false,blackout_tap_armed=false;
    int settings_scroll_y=0,settings_scroll_add=0,settings_drag_mode=0,settings_drag_none=0;
    int preset_save_last_progress=0,preset_clear_last_progress=0,precision_repeat_dir=0,precision_step=5;
    char preset_status[32]={},keypad_value[8]={};unsigned keypad_len=0;bool keypad_invalid=false;
    PresetDoubleTap preset_double_tap;
    int scroll_y=600,scroll_y_add=0,target_channel_index=0,target_channel=1,target_value=0;
    uint16_t enable_channels[512]={};int enable_channel_count=512;
    bool diagnostics_settings_seen=false,diagnostics_precision_seen=false,diagnostics_keypad_seen=false;
    View():controller(storage),dispatcher(controller){
        M5=FakeM5{};dispatcher.setSenderActive(true);
        for(int i=0;i<512;++i)enable_channels[i]=uint16_t(i+1);
    }
    void recordHardwareDiagnostics(const char*){}
    void serviceDmxOutput(){++dmx_calls;}void updateBatteryMonitor(){++battery_calls;}
    void drawBatteryStatus(){}void updateBlackoutArmState(){}void drawDmxActivityIndicator(){}
    void serviceSettingsNetwork(){}void serviceSettingsKineticScroll(bool){}void updateSettingsPowerArmState(){}
    void handleSettingsTouch(const Detail&){}void handlePrecisionEditorTouch(const Detail&){}
    int getPresetSlotAt(int,int){return -1;}bool handlePresetSlotLongPress(const Detail&){return false;}
    bool handlePresetSaveHold(const Detail&){return false;}bool handlePresetClearHold(const Detail&){return false;}
    void handlePresetOptionsTouch(int,int){}void handlePresetScreenTouch(int,int,bool){}
    void handleBlackoutTap(){}
    bool pointInRect(int x,int y,int rx,int ry,int rw,int rh){return x>=rx&&x<rx+rw&&y>=ry&&y<ry+rh;}
    void updateDisplay(bool=false){++main_draws;M5.Display.fillRect(0,0,250,240,1);}
    void hideUIValueSet(){M5.Display.fillRect(250,0,70,240,2);}
    void drawUIValueSet(int,bool=false){++stale_actions;}
    void setUIMode(ui_mode_t mode){++stale_actions;ui_mode=mode;updateDisplay(true);}
    bool scrollToTargetItem(){return false;}
    bool applyMainCommand(const ControllerCommand& command){++stale_actions;dispatcher.executeLocal(command);return true;}
    Surface* getFullscreenCanvas(fullscreen_page_t owner,const char*){
        CHECK(owner==active_fullscreen_page);
        if(!fullscreen_canvas_ready){fullscreen_canvas_ready=true;++allocations;}
        return &fullscreen_canvas;
    }
    void drawSettingsScreen(){
        CHECK(active_fullscreen_page==fullscreen_page_settings);
        if(M5.Speaker.isPlaying()){settings_redraw_pending=true;return;}
        auto* c=getFullscreenCanvas(fullscreen_page_settings,"SETTINGS");c->clearClipRect();
        c->fillScreen(3);c->fillRect(250,0,70,240,30);
        pushFullscreenCanvas(fullscreen_page_settings,"SETTINGS");settings_redraw_pending=false;++settings_frames;
    }
    void drawPrecisionEditor(){
        CHECK(active_fullscreen_page==fullscreen_page_precision);
        if(M5.Speaker.isPlaying()){precision_redraw_pending=true;return;}
        auto* c=getFullscreenCanvas(fullscreen_page_precision,"PRECISION");c->fillScreen(5);
        pushFullscreenCanvas(fullscreen_page_precision,"PRECISION");precision_redraw_pending=false;++precision_frames;
    }
    void drawPresetScreen(){CHECK(active_fullscreen_page==fullscreen_page_preset);M5.Display.fillScreen(4);}
#include "navigation_under_test.h"
    void click(int y){M5.Touch.detail=Detail{};auto& t=M5.Touch.detail;t.x=t.base_x=280;t.y=t.base_y=y;t.state=1;t.clicked=true;}
    void release(){M5.Touch.detail=Detail{};}
    void fullSettings()const{for(int y=0;y<240;++y)for(int x=0;x<320;++x)CHECK(M5.Display.pixels[y*320+x]==(x<250?3:30));}
};

int main(){
    for(int momentum:{-150,-1,0,1,150})for(bool speaking:{false,true}){
        View v;v.scroll_y_add=momentum;M5.Display.fillScreen(77);M5.Speaker.active=speaking;
        auto* buffer=v.fullscreen_canvas_buffer;const int position=v.scroll_y;
        const auto draws=v.main_draws;v.click(v.settings_open_y+5);CHECK(v.loop());
        CHECK(v.main_draws == draws);CHECK(v.scroll_y_add == 0);CHECK(v.scroll_y==position);
        CHECK(v.settings_screen&&v.active_fullscreen_page==View::fullscreen_page_settings);
        CHECK(v.dmx_calls==1&&v.battery_calls==1&&!v.stale_actions);
        if(speaking){CHECK(v.settings_redraw_pending&&!v.settings_frames);v.release();v.loop();CHECK(!v.settings_frames);M5.Speaker.active=false;}
        v.release();v.loop();v.fullSettings();CHECK(!v.settings_redraw_pending&&v.settings_frames==1);
        for(int i=0;i<5;++i){v.loop();v.fullSettings();}
        CHECK(v.allocations==1&&buffer==v.fullscreen_canvas_buffer);
        v.closeSettingsScreen();CHECK(v.scroll_y==position&&v.scroll_y_add==0);v.release();v.loop();CHECK(v.scroll_y==position);
        v.click(v.settings_open_y+5);v.loop();v.fullSettings();CHECK(v.allocations==1);
    }
    for(bool precision:{false,true})for(int momentum:{-90,90}){
        View v;v.scroll_y_add=momentum;const int position=v.scroll_y;const auto draws=v.main_draws;
        if(precision)v.ui_mode=View::mode_value_setting;
        v.click(v.preset_open_y+5);v.loop();CHECK(v.main_draws == draws);CHECK(v.scroll_y_add == 0);CHECK(v.scroll_y==position);
        CHECK(v.active_fullscreen_page==(precision?View::fullscreen_page_precision:View::fullscreen_page_preset));
        for(auto pixel:M5.Display.pixels)CHECK(pixel==(precision?5:4));
        v.release();v.loop();if(precision)v.closePrecisionEditor();else v.closePresetScreen();
        v.release();v.loop();CHECK(v.scroll_y==position&&v.scroll_y_add==0);
    }
    // Entry cancellation alone must not permit stale Main buttons/actions.
    for(bool precision:{false,true}){
        View v;v.scroll_y_add=0;const auto draws=v.main_draws;
        if(precision)v.ui_mode=View::mode_value_setting;
        M5.BtnC.pressed=true;v.click(precision?v.value_edit_button_y+5:v.settings_open_y+5);v.loop();
        CHECK(v.main_draws == draws);CHECK(!v.stale_actions&&v.target_channel_index==0);
        CHECK(v.dmx_calls==1&&v.battery_calls==1);
    }
    // Every fullscreen owner cancels velocity, including keypad ownership.
    {View v;for(auto page:{View::fullscreen_page_settings,View::fullscreen_page_precision,View::fullscreen_page_keypad,View::fullscreen_page_preset}){
        v.scroll_y_add=60;v.setActiveFullscreenPage(page);CHECK(!v.scroll_y_add&&v.scroll_y==600);
    }}
    // No navigation: exact existing linear deceleration/clamping trajectory.
    for(int initial:{-90,-1,0,1,90})for(int start:{0,600,5322}){
        View v;v.scroll_y=start;v.scroll_y_add=initial;int position=start,momentum=initial;
        for(int i=0;i<100;++i){
            int next=position+momentum;if(momentum)momentum+=momentum<0?1:-1;
            const int limit=54*((511+5)/5)-240;next=std::max(0,std::min(limit,next));position=next;
            v.loop();CHECK(v.scroll_y==position&&v.scroll_y_add==momentum);CHECK(v.active_fullscreen_page==View::fullscreen_page_none);
        }
    }
    puts("complete Sender loop: Settings +/- momentum and full pixels, deferred speaker draw, Presets/Precision, no stale buttons, preserved position/no resume, normal inertia/clamps, single canvas and common DMX/battery calls");
}
