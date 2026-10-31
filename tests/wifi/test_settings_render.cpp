// Compile the actual Settings rows/reset handler with deterministic font/touch fakes.
// Display-only width/geometry checks; not real LCD rasterization or hardware reset.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <vector>
// Same pressed/change bits and classifications as installed M5Unified 0.2.21.
enum TouchState{none=0,touch=1,touch_end=2,touch_begin=3,hold=5,hold_end=6,drag=13,drag_end=14};
struct Touch {int x=0,y=0,base_x=0,base_y=0;uint8_t id=1;int state=none;
 bool wasClicked()const{return state==touch_end;}bool wasPressed()const{return state==touch_begin;}bool isPressed()const{return state&1;}};
struct {struct T{Touch getDetail(){return {};}}Touch;struct S{bool playing=false;bool isPlaying()const{return playing;}}Speaker;}M5;
uint32_t now=0;uint32_t millis(){return now;}
namespace network_service {
 enum class Phase{disabled,ready,failed,prepared,starting,stopping};struct Status{Phase phase=Phase::ready;unsigned clients=3,max_clients=4,ui_revision=7;int error=0;bool reset_pending=false,radio_on=true;int power_error=0;char ip[16]="192.168.4.1";}value;
 std::string name,pass;unsigned toggles=0;bool requested=false,request_ok=true;bool requestEnabled(bool enabled){++toggles;requested=enabled;if(!request_ok)value.power_error=1;return request_ok;}
 unsigned resets=0;bool reset_ok=true;
 Status status(){return value;}const char* ssid(){return name.c_str();}const char* password(){return pass.c_str();}
 const char* phaseName(Phase p){return p==Phase::disabled?"OFF":p==Phase::starting?"STARTING":p==Phase::stopping?"STOPPING":p==Phase::prepared?"PREPARED":"FAILED";}bool requestFactoryReset(){++resets;return reset_ok;}
}
namespace fonts {const int Font0=0;}
enum class textdatum_t{middle_center};enum{TFT_WHITE,TFT_BLACK,TFT_GREEN,TFT_DARKGRAY,TFT_YELLOW};
struct Canvas {
 struct Text{std::string text;int x,y,width;const int* font;};std::vector<Text> rows;struct Rect{int x,y,w,h,radius,color;};std::vector<Rect> rects;int rect_y=0,rect_h=0;
 const int* font=nullptr;
 void setFont(const int* value){font=value;}void setTextDatum(textdatum_t){}void setTextColor(int,int){}
 int textWidth(const char* s){return int(strlen(s))*6;}
 void drawString(const char* s,int x,int y){rows.push_back({s,x,y,textWidth(s),font});}
 void drawRoundRect(int x,int y,int w,int h,int radius,int color){assert(x==10&&w==231&&h==50&&radius==6);rects.push_back({x,y,w,h,radius,color});rect_y=y;rect_h=h;}
};
struct View {
 static constexpr int settings_wifi_power_y=288,settings_wifi_reset_y=434,settings_wifi_button_x=10,settings_wifi_button_w=231,settings_wifi_button_h=50,settings_wifi_notice_y=502,build_timestamp_y=540,settings_gesture_threshold=9,settings_drag_none=0,settings_content_center_x=132;
 static constexpr int settings_content_h=562,settings_view_h=240,settings_scroll_max=settings_content_h-settings_view_h;
 int scroll=278,settings_scroll_add=1,settings_drag_mode=0;uint32_t settings_network_revision=0,wifi_reset_started_ms=0;
 int& settings_scroll_y=scroll;
 bool wifi_power_active=false,wifi_power_canceled=false,wifi_power_completed=false,settings_redraw_pending=false;uint8_t wifi_power_touch_id=0;bool wifi_power_target=false;uint32_t wifi_power_started_ms=0;
 bool wifi_reset_active=false,wifi_reset_canceled=false,wifi_reset_completed=false,wifi_reset_failed=false;unsigned frames=0;
 Canvas canvas;
 void drawBuildTimestamp(Canvas* c,int y){c->drawString("Build Date: test",settings_content_center_x,y);}
 void drawSettingsScreen(){++frames;auto& c=canvas;c.rows.clear();c.rects.clear();auto sy=[&](int y){return y-scroll;};
#include "wifi_settings_rows.h"
#include "wifi_settings_footer.h"
 }
#include "wifi_settings_reset.h"
#include "wifi_settings_power.h"
#include "wifi_settings_scroll.h"
};
int main(){
 (void)M5;View v;
 for(unsigned length:{0U,1U,8U,32U,63U}){
  network_service::name=std::string(length>32?32:length,'S');network_service::pass=std::string(length,'P');
  const auto name=network_service::name,pass=network_service::pass;
  for(int scroll:{0,100,180,278,322}){
   v.scroll=scroll;v.drawSettingsScreen();assert(v.canvas.rows.size()<=8);
   int previous=-1000;for(const auto& row:v.canvas.rows){assert(row.width<=240&&row.font==&fonts::Font0);assert(row.y-previous>=18);previous=row.y;assert(row.x-row.width/2>=12&&row.x+row.width/2<=252);}
   if(scroll==278){assert(v.canvas.rows.size()==7);assert(v.canvas.rows[1].text=="WIFI Ready - Clients 3/4");assert(v.canvas.rows[2].text.find("SSID: ")==0);assert(v.canvas.rows[3].text.find("PASSWORD: ")==0);assert(v.canvas.rows[4].text=="IP: 192.168.4.1");assert(v.canvas.rect_y==156&&v.canvas.rect_h==50);assert(v.canvas.rows[0].x==125&&v.canvas.rows[5].x==125);assert(v.canvas.rows[1].x==132&&v.canvas.rows[2].x==132&&v.canvas.rows[3].x==132&&v.canvas.rows[4].x==132);assert(v.canvas.rows[5].text=="RESET WIFI");assert(v.canvas.rows[6].text=="RESET TAKES EFFECT AFTER REBOOT");}
  }
  assert(name==network_service::name&&pass==network_service::pass);
 }
 // RESET retains the established two-second, movement-cancel, once-per-press behavior.
 v.scroll=278;Touch t;t.x=t.base_x=30;t.y=t.base_y=180;t.state=touch_begin;now=100;
 assert(v.handleWifiResetTouch(t,t.y+v.scroll));t.state=hold;now=2099;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==0);
 now=2100;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1);now=3000;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1);
 t.state=none;v.handleWifiResetTouch(t,t.y+v.scroll);assert(!v.wifi_reset_active);
 t.state=touch_begin;now=4000;v.handleWifiResetTouch(t,t.y+v.scroll);t.state=drag;t.x=50;now=7000;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1&&v.wifi_reset_canceled);
 t.state=none;v.handleWifiResetTouch(t,t.y+v.scroll);
 network_service::value.reset_pending=true;v.drawSettingsScreen();assert(v.canvas.rows.back().text=="RESET SAVED: RESTART TO APPLY");
 network_service::value.phase=network_service::Phase::failed;network_service::value.error=257;v.drawSettingsScreen();assert(v.canvas.rows[1].text=="WIFI FAILED (257)");
 View power;power.scroll=120;Touch tap;tap.x=tap.base_x=100;tap.y=tap.base_y=180;
 auto begin=[&](uint32_t at){now=at;tap.state=touch_begin;tap.x=tap.base_x=100;tap.id=1;assert(power.handleWifiPowerTouch(tap,300));};
 auto sample=[&](uint32_t at,int state){now=at;tap.state=state;assert(power.handleWifiPowerTouch(tap,300));};
 network_service::value=network_service::Status{};
 begin(100);assert(power.wifi_power_active&&power.settings_redraw_pending);power.drawSettingsScreen();
 assert(power.canvas.rows[0].text=="KEEP HOLDING..."&&power.canvas.rects[0].color==TFT_YELLOW);
 sample(150,touch_end);assert(network_service::toggles==0&&!power.wifi_power_active); // Short tap.
 begin(1000);sample(1999,hold);assert(network_service::toggles==0);sample(2000,hold_end);assert(network_service::toggles==0); // Release at deadline is not a held sample.
 begin(4000);sample(4999,hold);assert(network_service::toggles==0);sample(5000,hold);
 assert(network_service::toggles==1&&!network_service::requested);sample(10000,hold);assert(network_service::toggles==1);
 sample(10001,hold_end);assert(!power.wifi_power_active);
 network_service::value.radio_on=false;network_service::value.phase=network_service::Phase::disabled;begin(11000);sample(11999,hold);assert(network_service::toggles==1);sample(12000,hold);
 assert(network_service::toggles==2&&network_service::requested);power.drawSettingsScreen();assert(power.canvas.rects[0].color==TFT_WHITE&&power.canvas.rows[0].text=="Wi-Fi: OFF");sample(16000,hold);assert(network_service::toggles==2);sample(16001,hold_end);
 // Early release, movement out/back in, excess displacement and identity change all latch cancellation.
 begin(17000);sample(17500,hold_end);assert(network_service::toggles==2);
 begin(18000);tap.x=260;sample(18500,drag);tap.x=100;sample(21000,hold);assert(network_service::toggles==2&&power.wifi_power_canceled);sample(21001,drag_end);
 begin(22000);tap.x=110;sample(23000,drag);tap.x=100;sample(25000,hold);assert(network_service::toggles==2);sample(25001,drag_end);
 begin(26000);tap.id=2;sample(29000,hold);assert(network_service::toggles==2);sample(29001,hold_end);
 // Unsigned elapsed-time arithmetic is safe across millis() wrap.
 begin(UINT32_MAX-999);sample(UINT32_MAX,hold);assert(network_service::toggles==2);sample(0,hold);assert(network_service::toggles==3&&network_service::requested);sample(1,hold_end);
 // STARTING/STOPPING is not a new actionable hold, even if READY arrives before its deadline.
 network_service::value.phase=network_service::Phase::starting;begin(30000);network_service::value.phase=network_service::Phase::ready;sample(33000,hold);assert(network_service::toggles==3);sample(33001,hold_end);
 // Capture the original request direction if the radio fails during an OFF hold.
 network_service::value.radio_on=true;begin(34000);network_service::value.radio_on=false;network_service::value.phase=network_service::Phase::failed;sample(36000,hold);
 assert(network_service::toggles==4&&!network_service::requested);sample(36001,hold_end);
 // A rejected request is never resubmitted while the same finger remains down.
 network_service::request_ok=false;begin(37000);sample(39000,hold);sample(42000,hold);assert(network_service::toggles==5);sample(42001,hold_end);network_service::request_ok=true;
 // A new point's press cannot steal an active/canceled hold or restart its timer.
 begin(43000);tap.id=2;sample(44000,touch_begin);sample(47000,hold);assert(network_service::toggles==5&&power.wifi_power_canceled);sample(47001,hold_end);
 network_service::value=network_service::Status{};power.drawSettingsScreen();assert(power.canvas.rows[0].text=="Wi-Fi: ON"&&power.canvas.rects[0].color==TFT_GREEN);
 network_service::value.radio_on=false;power.drawSettingsScreen();assert(power.canvas.rows[0].text=="Wi-Fi: OFF"&&power.canvas.rects[0].color==TFT_WHITE);
 for(auto phase:{network_service::Phase::starting,network_service::Phase::stopping}){network_service::value.phase=phase;power.drawSettingsScreen();assert(power.canvas.rects[0].color==TFT_DARKGRAY);assert(power.canvas.rows[0].text.find("HOLD")==std::string::npos);}
 network_service::value.phase=network_service::Phase::failed;network_service::value.error=257;power.drawSettingsScreen();assert(power.canvas.rects[0].color==TFT_WHITE&&power.canvas.rows[1].text=="WIFI FAILED (257)");
 network_service::value.power_error=1;power.drawSettingsScreen();assert(power.canvas.rows[1].text=="WIFI SAVE FAILED (1)");
 View other;tap.state=touch_begin;assert(!other.handleWifiPowerTouch(tap,250));tap.x=260;assert(!other.handleWifiPowerTouch(tap,300));
 // Every painted edge matches the touch target, including newly enlarged edges.
 for(int y:{288,337,434,483})for(int x:{10,240}){
  View edge;Touch e;e.x=e.base_x=x;e.y=e.base_y=y-200;e.state=touch_begin;
  assert(y<400?edge.handleWifiPowerTouch(e,y):edge.handleWifiResetTouch(e,y));
 }
 for(int y:{287,338,433,484}){View edge;Touch e;e.x=e.base_x=100;e.state=touch_begin;assert(!edge.handleWifiPowerTouch(e,y));assert(!edge.handleWifiResetTouch(e,y));}
 for(int x:{9,241,255}){View edge;Touch e;e.x=e.base_x=x;e.state=touch_begin;assert(!edge.handleWifiPowerTouch(e,300));assert(!edge.handleWifiResetTouch(e,450));}
 View footer;footer.scroll=322;footer.drawSettingsScreen();assert(footer.canvas.rows.back().text=="Build Date: test"&&footer.canvas.rows.back().y==218);
 assert(footer.canvas.rects.size()==2);assert(footer.canvas.rects[0].y==288-322&&footer.canvas.rects[1].y==434-322);
 View inertia;inertia.scroll=320;inertia.settings_scroll_add=10;inertia.serviceSettingsKineticScroll(false);
 assert(inertia.scroll==322&&inertia.settings_scroll_add==0&&inertia.canvas.rows.back().y==218);
 inertia.scroll=2;inertia.settings_scroll_add=-10;inertia.serviceSettingsKineticScroll(false);assert(inertia.scroll==0&&inertia.settings_scroll_add==0);
 inertia.scroll=200;inertia.settings_scroll_add=4;unsigned frames=inertia.frames;inertia.serviceSettingsKineticScroll(true);assert(inertia.scroll==200&&inertia.frames==frames);
 M5.Speaker.playing=true;inertia.serviceSettingsKineticScroll(false);assert(inertia.scroll==200&&inertia.frames==frames);M5.Speaker.playing=false;
 inertia.serviceSettingsKineticScroll(false);assert(inertia.scroll==204&&inertia.settings_scroll_add==3);
 puts("PASS actual physical Wi-Fi rows: 32/63-character inputs display-fitted without mutation, x10..240 aligned render/hitboxes, footer at scroll limit, exact 1s ON/OFF hold/no tap/release/movement/identity/wrap/one-shot, authoritative border colors and unchanged reset and next-boot/error notices");
}
