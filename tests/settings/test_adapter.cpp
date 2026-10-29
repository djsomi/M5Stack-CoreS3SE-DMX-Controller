#include "device_settings.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
uint8_t g_display_normal_brightness=0,g_ui_speaker_volume=0;
uint32_t g_display_timeout_ms=0;bool display_sleeping=false,battery_warning_volume_override=false;
struct {struct {unsigned value=0,calls=0;void setBrightness(uint8_t v){value=v;++calls;}} Display;
struct {unsigned value=0,calls=0;void setVolume(uint8_t v){value=v;++calls;}} Speaker;} M5;
enum {PT_U8=1};
struct Preferences {
    static unsigned writes;static bool available,success;static uint8_t values[3];
    bool begin(const char* ns,bool){assert(!strcmp(ns,"dmxui"));return available;}
    unsigned index(const char* key){return !strcmp(key,"bright")?0:!strcmp(key,"timeout")?1:(!strcmp(key,"volume")?2:99);}
    bool isKey(const char* key){return index(key)<3;}int getType(const char*){return PT_U8;}
    uint8_t getUChar(const char* key,uint8_t){return values[index(key)];}
    unsigned putUChar(const char* key,uint8_t value){++writes;if(success)values[index(key)]=value;return success?1:0;}
    void end(){}
};
unsigned Preferences::writes=0;bool Preferences::available=true,Preferences::success=true;uint8_t Preferences::values[3]{3,3,2};
#include "settings_adapter.h"
int main(){
    ApplicationSettingsBackend backend;DeviceSettings s(backend);s.begin();
    assert(M5.Display.value==128&&M5.Speaker.value==40&&g_display_timeout_ms==180000);
    const unsigned brightness[]={32,64,128,192,255},volume[]={0,20,40,100,200};
    for(unsigned i=0;i<5;++i){assert(brightnessFromLevel(i+1)==brightness[i]);assert(speakerVolumeFromLevel(i)==volume[i]);}
    display_sleeping=true;const auto count=M5.Display.calls;s.set(DeviceSetting::brightness,5);
    assert(M5.Display.calls==count&&g_display_normal_brightness==255);
    const auto writes=Preferences::writes;
    s.edit(DeviceSettings::Edit::preview,1,1,DeviceSetting::brightness,1,s.snapshot().revision,1,true);
    assert(M5.Display.calls==count&&g_display_normal_brightness==32&&Preferences::writes==writes);
    s.edit(DeviceSettings::Edit::cancel,1,1,DeviceSetting::brightness,0,s.snapshot().revision,2);
    assert(M5.Display.calls==count&&g_display_normal_brightness==255);
    display_sleeping=false;M5.Display.setBrightness(g_display_normal_brightness);assert(M5.Display.value==255);
    battery_warning_volume_override=true;M5.Speaker.value=204;const auto sounds=M5.Speaker.calls;
    s.set(DeviceSetting::volume,0);assert(M5.Speaker.value==204&&M5.Speaker.calls==sounds&&g_ui_speaker_volume==0);
    s.edit(DeviceSettings::Edit::preview,1,2,DeviceSetting::volume,4,s.snapshot().revision,3,true);
    assert(M5.Speaker.value==204&&M5.Speaker.calls==sounds&&g_ui_speaker_volume==200);
    s.edit(DeviceSettings::Edit::cancel,1,2,DeviceSetting::volume,0,s.snapshot().revision,4);
    // The unchanged warning service restores this current derived normal value.
    M5.Speaker.setVolume(g_ui_speaker_volume);assert(M5.Speaker.value==0);
    s.set(DeviceSetting::timeout,0);assert(g_display_timeout_ms==0);
    Preferences::success=false;assert(s.set(DeviceSetting::timeout,4)==DeviceSettings::Result::storage_error);assert(g_display_timeout_ms==240000);
    Preferences::available=false;uint8_t v=0;assert(!backend.read(DeviceSetting::brightness,v));assert(!backend.write(DeviceSetting::brightness,3));
    puts("PASS actual adapter mappings, dmxui-only single-key commits, failure reporting, asleep brightness and active-warning volume priority");
}
