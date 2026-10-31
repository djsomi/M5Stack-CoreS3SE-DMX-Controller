// Real Settings draw/mapping functions; primitive bounds use installed M5GFX
// semantics: fillCircle spans cx-r..cx+r; rectangles span x..x+w-1.
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <vector>
namespace fonts{int Font0=0;}
enum class textdatum_t{top_center,middle_center};
enum{TFT_DARKGRAY,TFT_GREEN,TFT_WHITE,TFT_RED,TFT_BLACK};
struct LovyanGFX{
 int left=1000,right=-1000,handle=-1;bool always_on=false;
 std::vector<int> ticks;
 void paint(int x,int last){left=std::min(left,x);right=std::max(right,last);}
 void fillRect(int x,int,int w,int,int){if(w>0)paint(x,x+w-1);}
 void fillCircle(int x,int,int radius,int){paint(x-radius,x+radius);if(radius==8)handle=x;if(radius==4)ticks.push_back(x);}
 void setFont(const int*){}void setTextDatum(textdatum_t){}void setTextColor(int,int){}
 void drawNumber(int,int x,int){paint(x-3,x+2);}
 void drawString(const char* text,int x,int){paint(x-int(strlen(text))*3,x+int(strlen(text))*3-1);always_on=!strcmp(text,"ON");}
};
struct Sliders{
#include "slider_alignment.h"
};
int main(){
 Sliders s;const int centers[]={18,71,125,178,232};
 assert(s.settings_slider_touch_x0==10&&s.settings_slider_touch_x1==240);
 assert(s.settings_wifi_button_x==10&&s.settings_wifi_button_x+s.settings_wifi_button_w-1==240);
 for(int i=0;i<5;++i){
  assert(s.sliderValueFromX(centers[i])==i+1&&s.volumeSliderValueFromX(centers[i])==i);
  for(int type=0;type<3;++type){LovyanGFX g;
   if(type==0)s.drawVolumeSlider(&g,234,i);else s.drawSettingsSlider(&g,type==1?78:156,i+1,type==2);
   assert(g.handle==centers[i]&&g.ticks.size()==5);
   for(unsigned tick=0;tick<5;++tick)assert(g.ticks[tick]==centers[tick]);
   assert(g.left>=10&&g.right<=240);
   if(i==0)assert(g.left==10);if(i==4)assert(g.right==240);
   if(type==2&&i==4)assert(g.always_on);
  }
 }
 int previous=1;
 for(int x=-30;x<=300;++x){int level=s.sliderValueFromX(x);assert(level>=1&&level<=5&&level>=previous);assert(s.volumeSliderValueFromX(x)==level-1);previous=level;}
 assert(s.sliderValueFromX(10)==1&&s.sliderValueFromX(240)==5);
 for(int value:{-20,40}){LovyanGFX g;s.drawVolumeSlider(&g,234,value);assert(g.handle==(value<0?18:232));}
 puts("PASS actual Brightness/Timeout/Volume draw+mapping: visible bounds x10..240, original radius 8, five proportional stops including both endpoints, all-x monotonic clamps, timeout Always On and shared button/hitbox edges");
}
