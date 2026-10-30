// Compile the actual physical maintenance methods against a pixel/clip LCD fake.
// Tests ownership/transfers/clears, not real font rasterization, DMA or panel timing.
#include "device_maintenance.h"
#include "wifi_configuration.h"
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
    enum fullscreen_page_t {fullscreen_page_none,fullscreen_page_settings,fullscreen_page_precision,fullscreen_page_keypad,fullscreen_page_preset,fullscreen_page_maintenance,fullscreen_page_wifi};
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
    bool wifi_ui_open=false,wifi_released=false,wifi_restored=false,wifi_frame_ready=false;int wifi_previous_page=0;uint64_t wifi_revision=0;
#include "wifi_render_methods.h"
};

struct WifiBackend:WifiConfiguration::Backend {
    softap_policy::Credentials value{};bool online=true;unsigned begins=0;
    WifiBackend(){value.version=2;value.origin=softap_policy::Origin::custom;strcpy(value.ssid,"Synthetic AP");strcpy(value.pass,"Never render this password");}
    bool available()override{return true;}bool connected(uint32_t)override{return online;}
    void current(softap_policy::Credentials& out)override{out=value;}
    bool begin(const softap_policy::Credentials&)override{++begins;return true;}
    WifiConfiguration::Work progress()override{return WifiConfiguration::Work::pending;}
    void published()override{}
};
int main(){
 for(unsigned page=0;page<6;++page)for(unsigned action=0;action<4;++action){
  View v;WifiBackend b;WifiConfiguration w(b);w.poll(0);M5.Display=Surface{};M5.Touch.detail=Touch{};
  if(page==1){v.settings_screen=true;v.active_fullscreen_page=View::fullscreen_page_settings;}
  if(page==2||page==3){v.precision_editor_screen=true;v.active_fullscreen_page=page==2?View::fullscreen_page_precision:View::fullscreen_page_keypad;}
  if(page==4||page==5){v.preset_screen=true;v.preset_options_screen=page==5;v.active_fullscreen_page=View::fullscreen_page_preset;}
  auto previous=v.active_fullscreen_page;now_ms=100;w.propose("Edited AP","",1,1,1,now_ms);
  M5.Display.pixels.fill(77);M5.Display.setClipRect(0,0,3,3);lcd_busy=true;
  v.serviceWifiUi(w);assert(v.active_fullscreen_page==View::fullscreen_page_wifi&&v.allocations==1&&M5.Display.cw==320);
  assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),77)==M5.Display.pixels.end());
  assert(b.begins==0);
  auto frames=M5.Display.full;for(unsigned n=0;n<10;++n)v.serviceWifiUi(w);assert(M5.Display.full==frames);
  M5.Touch.detail.state=M5.Touch.detail.pressed=true;M5.Touch.detail.x=25;M5.Touch.detail.y=180;
  if(action==0){v.serviceWifiUi(w);assert(b.begins==1&&w.applying());continue;}
  if(action==1)M5.Touch.detail.x=175;
  if(action==2){now_ms=60100;w.poll(now_ms);}
  if(action==3){b.online=false;w.poll(now_ms);}
  M5.Speaker.active=page==1;v.serviceWifiUi(w);assert(!w.active()&&b.begins==0);
  if(page==1){assert(v.active_fullscreen_page==View::fullscreen_page_wifi);M5.Speaker.active=false;v.serviceWifiUi(w);}
  assert(v.active_fullscreen_page==previous&&v.wifi_ui_open&&v.wifi_restored);
  M5.Touch.detail=Touch{};v.serviceWifiUi(w);assert(!v.wifi_ui_open&&v.precision_step==5&&v.target_value==87);
  assert(std::find(M5.Display.pixels.begin(),M5.Display.pixels.end(),22)==M5.Display.pixels.end());
 }
 puts("PASS actual Wi-Fi popup: OK/CANCEL/60s/disconnect, exact hitboxes, six-page restore/clip/speaker/release ownership, stable frames and one shared canvas");
}
