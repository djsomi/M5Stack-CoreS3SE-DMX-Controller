// Compile the actual Settings rows/reset handler with deterministic font/touch fakes.
// Display-only width/geometry checks; not real LCD rasterization or hardware reset.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <vector>
struct Touch {int x=0,y=0,base_x=0,base_y=0;bool state=false,pressed=false;bool wasPressed()const{return pressed;}};
struct {struct T{Touch getDetail(){return {};}}Touch;}M5;
uint32_t now=0;uint32_t millis(){return now;}
namespace network_service {
 enum class Phase{ready,failed};struct Status{Phase phase=Phase::ready;unsigned clients=3,max_clients=4,ui_revision=7;int error=0;bool reset_pending=false;char ip[16]="192.168.4.1";}value;
 std::string name,pass;unsigned resets=0;bool reset_ok=true;
 Status status(){return value;}const char* ssid(){return name.c_str();}const char* password(){return pass.c_str();}
 const char* phaseName(Phase){return "FAILED";}bool requestFactoryReset(){++resets;return reset_ok;}
}
namespace fonts {const int Font0=0;}
enum class textdatum_t{middle_center};enum{TFT_WHITE,TFT_BLACK,TFT_GREEN};
struct Canvas {
 struct Text{std::string text;int x,y,width;};std::vector<Text> rows;int rect_y=0,rect_h=0;
 void setFont(const int*){}void setTextDatum(textdatum_t){}void setTextColor(int,int){}
 int textWidth(const char* s){return int(strlen(s))*6;}
 void drawString(const char* s,int x,int y){rows.push_back({s,x,y,textWidth(s)});}
 void drawRoundRect(int x,int y,int w,int h,int,int){assert(x==24&&w==216);rect_y=y;rect_h=h;}
};
struct View {
 static constexpr int settings_wifi_reset_y=394,settings_gesture_threshold=9,settings_drag_none=0,settings_content_center_x=132;
 int scroll=222,settings_scroll_add=1,settings_drag_mode=0;uint32_t settings_network_revision=0,wifi_reset_started_ms=0;
 bool wifi_reset_active=false,wifi_reset_canceled=false,wifi_reset_completed=false,wifi_reset_failed=false;unsigned frames=0;
 Canvas canvas;
 void drawSettingsScreen(){++frames;auto& c=canvas;c.rows.clear();auto sy=[&](int y){return y-scroll;};
#include "wifi_settings_rows.h"
 }
#include "wifi_settings_reset.h"
};
int main(){
 (void)M5;View v;
 for(unsigned length:{0U,1U,8U,32U,63U}){
  network_service::name=std::string(length>32?32:length,'S');network_service::pass=std::string(length,'P');
  const auto name=network_service::name,pass=network_service::pass;
  for(int scroll:{0,100,180,222}){
   v.scroll=scroll;v.drawSettingsScreen();assert(v.canvas.rows.size()<=6);
   int previous=-1000;for(const auto& row:v.canvas.rows){assert(row.width<=240);assert(row.y-previous>=18);previous=row.y;assert(row.x-row.width/2>=12&&row.x+row.width/2<=252);}
   if(scroll==222){assert(v.canvas.rows.size()==6);assert(v.canvas.rows[0].text=="WIFI Ready - Clients 3/4");assert(v.canvas.rows[1].text.find("SSID: ")==0);assert(v.canvas.rows[2].text.find("PASSWORD: ")==0);assert(v.canvas.rows[3].text=="IP: 192.168.4.1");assert(v.canvas.rect_y==172&&v.canvas.rect_h==28);assert(v.canvas.rows[5].text=="WIFI RESET APPLY NEXT BOOT");}
  }
  assert(name==network_service::name&&pass==network_service::pass);
 }
 v.scroll=222;Touch t;t.x=t.base_x=30;t.y=t.base_y=180;t.state=t.pressed=true;now=100;
 assert(v.handleWifiResetTouch(t,t.y+v.scroll));t.pressed=false;now=2099;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==0);
 now=2100;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1);now=3000;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1);
 t.state=false;v.handleWifiResetTouch(t,t.y+v.scroll);assert(!v.wifi_reset_active);
 t.state=t.pressed=true;now=4000;v.handleWifiResetTouch(t,t.y+v.scroll);t.pressed=false;t.x=50;now=7000;v.handleWifiResetTouch(t,t.y+v.scroll);assert(network_service::resets==1&&v.wifi_reset_canceled);
 t.state=false;v.handleWifiResetTouch(t,t.y+v.scroll);
 network_service::value.reset_pending=true;v.drawSettingsScreen();assert(v.canvas.rows.back().text=="RESET SAVED: RESTART TO APPLY");
 network_service::value.phase=network_service::Phase::failed;network_service::value.error=257;v.drawSettingsScreen();assert(v.canvas.rows.front().text=="WIFI FAILED (257)");
 puts("PASS actual physical Wi-Fi rows: 32/63-character inputs display-fitted without mutation, six bounded rows, compact geometry, redraw, exact 2s reset/no repeat/movement cancel and next-boot/error notices");
}
