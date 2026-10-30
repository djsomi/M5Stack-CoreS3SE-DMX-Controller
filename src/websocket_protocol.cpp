#include "websocket_bridge.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "wifi_wire.h"

namespace websocket_bridge {
namespace {
// Deliberately small flat JSON grammar: ASCII keys/type, unsigned integers,
// booleans. No arrays, nesting, escapes, floats, duplicate or unknown fields.
struct Parser {
    const char* p; const char* end;
    void space() { while(p<end && (*p==' '||*p=='\r'||*p=='\n'||*p=='\t')) ++p; }
    bool eat(char c) {space();if(p==end||*p!=c)return false;++p;return true;}
    bool string(char* out,size_t cap) {
        if(!eat('"'))return false;
        size_t n=0;
        while(p<end && *p!='"') {
            const unsigned char c=*p++;
            if(c<32||c>126||c=='\\'||n+1>=cap)return false;
            out[n++]=char(c);
        }
        out[n]=0;return eat('"');
    }
    bool number(uint32_t& value) {
        space();value=0;
        if(p==end||*p<'0'||*p>'9')return false;
        const char* start=p;
        while(p<end&&*p>='0'&&*p<='9') {
            const unsigned n=unsigned(*p++-'0');
            if(value>(UINT32_MAX-n)/10)return false;
            value=value*10+n;
        }
        return p-start==1||*start!='0';
    }
    bool boolean(uint32_t& value) {
        space();
        if(end-p>=4&&!memcmp(p,"true",4)){p+=4;value=1;return true;}
        if(end-p>=5&&!memcmp(p,"false",5)){p+=5;value=0;return true;}
        return false;
    }
};
struct Writer {
    char* data;size_t capacity,used=0;bool ok=true;
    Writer(char* p,size_t c):data(p),capacity(c){}
    void add(const char* format,...) {
        if(!ok)return;
        va_list args;va_start(args,format);
        const int n=vsnprintf(data+used,capacity-used,format,args);va_end(args);
        if(n<0||size_t(n)>=capacity-used){ok=false;return;}used+=size_t(n);
    }
};
}
bool decode(const char* data,size_t length,Request& r,const char*& error) {
    error="invalid";
    if(!data||!length||length>max_message)return false;
    Parser p{data,data+length};
    uint32_t nums[15]{};unsigned fields=0;char type[24]{},setting[16]{},base[21]{},action[16]{},wifi_ssid[45]{},wifi_password[85]{};
    struct Clear {char* p;~Clear(){WifiConfiguration::erase(p,85);}} clear{wifi_password};
    if(!p.eat('{'))return false;
    do {
        char key[16];if(!p.string(key,sizeof(key))||!p.eat(':'))return false;
        const char* keys[]={"v","id","type","channel","value","active","slot","target","setting","gesture","base","action","request","ssid","password"};
        unsigned k=0;while(k<15&&strcmp(key,keys[k]))++k;
        if(k==15||(fields&(1U<<k)))return false;
        fields|=1U<<k;
        if(k==2){if(!p.string(type,sizeof(type)))return false;}
        else if(k==8){if(!p.string(setting,sizeof(setting)))return false;}
        else if(k==11){if(!p.string(action,sizeof(action)))return false;}
        else if(k==10){if(!p.string(base,sizeof(base)))return false;}
        else if(k==13){if(!p.string(wifi_ssid,sizeof(wifi_ssid)))return false;}
        else if(k==14){if(!p.string(wifi_password,sizeof(wifi_password)))return false;}
        else if(k==5){if(!p.boolean(nums[k]))return false;}
        else if(!p.number(nums[k]))return false;
        p.space();if(p.p==p.end)return false;
        if(*p.p=='}')break;
        if(!p.eat(','))return false;
    }while(true);
    if(!p.eat('}'))return false;p.space();if(p.p!=p.end)return false;
    if((fields&7)!=7||!nums[1])return false;
    r.id=nums[1];
    if(nums[0]!=1){error="version";return false;}
    unsigned expected=7;
    if(!strcmp(type,"wifi.get"))r.wifi=1;
    else if(!strcmp(type,"wifi.propose")) {
        expected|=1024|8192|16384;r.wifi=2;
        if(!base[0]||base[0]=='0')return false;
        for(const char* c=base;*c;++c){if(*c<'0'||*c>'9'||r.wifi_base>(UINT64_MAX-unsigned(*c-'0'))/10)return false;r.wifi_base=r.wifi_base*10+unsigned(*c-'0');}
        if(!wifi_wire::decode(wifi_ssid,r.wifi_ssid,sizeof(r.wifi_ssid))||!r.wifi_ssid[0]||
           !wifi_wire::decode(wifi_password,r.wifi_password,sizeof(r.wifi_password))||
           (r.wifi_password[0]&&!WifiConfiguration::text(r.wifi_password,sizeof(r.wifi_password),8,63)))return false;
    } else if(!strcmp(type,"wifi.cancel")){expected|=4096;r.wifi=3;r.maintenance_request=nums[12];if(!nums[12])return false;}
    else if(!strcmp(type,"maintenance.get"))r.maintenance=1;
    else if(!strcmp(type,"maintenance.request")) {
        expected|=2048;r.maintenance=2;
        if(!strcmp(action,"restart"))r.maintenance_action=DeviceMaintenance::Action::restart;
        else if(!strcmp(action,"power_off"))r.maintenance_action=DeviceMaintenance::Action::power_off;
        else return false;
    } else if(!strcmp(type,"maintenance.cancel")) {
        expected|=4096;r.maintenance=3;r.maintenance_request=nums[12];if(!nums[12])return false;
    }
    else if(!strcmp(type,"settings.request"))r.settings=true;
    else if(!strcmp(type,"settings.set")||!strcmp(type,"settings.preview")||
            !strcmp(type,"settings.commit")||!strcmp(type,"settings.cancel")) {
        expected|=256;r.settings=r.settings_write=true;
        if(!strcmp(type,"settings.preview"))r.settings_edit=DeviceSettings::Edit::preview;
        if(!strcmp(type,"settings.commit"))r.settings_edit=DeviceSettings::Edit::commit;
        if(!strcmp(type,"settings.cancel"))r.settings_edit=DeviceSettings::Edit::cancel;
        if(r.settings_edit==DeviceSettings::Edit::none||r.settings_edit==DeviceSettings::Edit::preview)expected|=16;
        if(r.settings_edit!=DeviceSettings::Edit::none) {
            expected|=1536;if(!nums[9]||nums[9]>r.id||!base[0]||base[0]=='0')return false;
            r.settings_gesture=nums[9];r.settings_base=0;
            for(const char* c=base;*c;++c){if(*c<'0'||*c>'9'||r.settings_base>(UINT64_MAX-unsigned(*c-'0'))/10)return false;r.settings_base=r.settings_base*10+unsigned(*c-'0');}
        }
        if(!strcmp(setting,"brightness"))r.setting=DeviceSetting::brightness;
        else if(!strcmp(setting,"timeout"))r.setting=DeviceSetting::timeout;
        else if(!strcmp(setting,"volume"))r.setting=DeviceSetting::volume;
        else return false;
        if((expected&16)&&!DeviceSettings::valid(r.setting,nums[4]))return false;r.setting_value=nums[4];
    }
    else if(!strcmp(type,"state.request"))r.snapshot=true;
    else if(!strcmp(type,"channel.set")) {
        expected|=24;
        if(nums[3]<1||nums[3]>512||nums[4]>255)return false;
        r.command=ControllerCommand::channelCommand(CommandType::set_channel,int(nums[3]),int(nums[4]));
    } else if(!strcmp(type,"blackout.set")) {
        expected|=32;r.command=ControllerCommand::blackoutCommand(nums[5]!=0);
    } else {
        expected|=64;CommandType command=CommandType::invalid;
        if(!strcmp(type,"preset.load"))command=CommandType::load_preset;
        if(!strcmp(type,"preset.save"))command=CommandType::save_preset;
        if(!strcmp(type,"preset.clear"))command=CommandType::clear_preset;
        if(!strcmp(type,"preset.copy")){command=CommandType::copy_preset;expected|=128;}
        if(command==CommandType::invalid){error="type";return false;}
        if(nums[6]<1||nums[6]>8||(command==CommandType::copy_preset&&(nums[7]<1||nums[7]>8)))return false;
        r.command=ControllerCommand::presetCommand(command,int(nums[6])-1,nums[7]?int(nums[7])-1:0);
    }
    r.command.source=CommandSource::external;
    return fields==expected;
}
bool validFrame(unsigned opcode,bool final,size_t length) {
    return final&&((opcode==1&&length>0&&length<=max_message)||
        ((opcode==8||opcode==9||opcode==10)&&length<=125&&(opcode!=8||length!=1)));
}
const char* statusName(CommandStatus s) {
    switch(s) {
    case CommandStatus::accepted:return "accepted";case CommandStatus::applied:return "applied";
    case CommandStatus::no_change:return "no_change";case CommandStatus::busy:return "busy";
    case CommandStatus::storage_error:return "storage_error";case CommandStatus::unsupported_mode:return "unsupported_mode";
    case CommandStatus::rejected:return "rejected";case CommandStatus::transport_error:return "transport_error";
    default:return "invalid";
    }
}
const char* maintenanceAction(DeviceMaintenance::Action value) {
    const char* names[]={"none","restart","power_off"};return names[unsigned(value)];
}
const char* maintenancePhase(DeviceMaintenance::Phase value) {
    const char* names[]={"idle","countdown","zeroing","quiescing","executing","failed"};return names[unsigned(value)];
}
const char* maintenanceReason(DeviceMaintenance::Reason value) {
    const char* names[]={"none","busy","cancelled","transport","deadline","returned"};return names[unsigned(value)];
}
const char* maintenanceResult(DeviceMaintenance::Result value) {
    const char* names[]={"invalid","accepted","cancelled","unauthorized","busy","unsupported"};return names[unsigned(value)];
}
size_t encode(const Event& e,char* out,size_t capacity) {
    Writer w(out,capacity);
    if(e.kind==Kind::wifi_snapshot||e.kind==Kind::wifi_update||e.kind==Kind::wifi_result) {
        const auto& c=e.wifi;
        const char* phases[]={"idle","approval","applying","failed"};
        const char* reasons[]={"none","cancelled","expired","disconnected","storage","radio","rollback","unavailable"};
        const char* origins[]={"legacy","factory","custom"};
        const char* results[]={"accepted","cancelled","no_change","invalid","conflict","busy","unauthorized","unavailable"};
        w.add("{\"v\":1,\"type\":\"%s\",\"id\":%lu,\"revision\":\"%llu\",\"seq\":\"%llu\",\"base\":\"%llu\",\"ssid\":",
            e.kind==Kind::wifi_snapshot?"wifi.snapshot":e.kind==Kind::wifi_update?"wifi.update":"wifi.result",
            (unsigned long)e.id,(unsigned long long)e.revision,(unsigned long long)e.sequence,(unsigned long long)c.base);
        const auto text=[&w](const char* value){w.add("\"");for(;*value;++value){if(*value=='\"'||*value=='\\')w.add("\\");w.add("%c",*value);}w.add("\"");};
        text(c.ssid);w.add(",\"proposed\":");text(c.proposed);
        w.add(",\"origin\":\"%s\",\"phase\":\"%s\",\"reason\":\"%s\",\"request\":%lu,\"remaining_ms\":%lu,\"password_change\":%s,\"available\":%s",
            origins[unsigned(c.origin)],phases[unsigned(c.phase)],reasons[unsigned(c.reason)],(unsigned long)c.request,(unsigned long)c.remaining_ms,c.password_change?"true":"false",c.available?"true":"false");
        if(e.kind==Kind::wifi_result)w.add(",\"status\":\"%s\"",results[unsigned(e.wifi_result)]);
        w.add("}");return w.ok?w.used:0;
    }
    if(e.kind==Kind::maintenance_snapshot||e.kind==Kind::maintenance_update||e.kind==Kind::maintenance_result) {
        const auto& s=e.maintenance;
        const char* kind=e.kind==Kind::maintenance_snapshot?"maintenance.snapshot":e.kind==Kind::maintenance_update?"maintenance.update":"maintenance.result";
        w.add("{\"v\":1,\"type\":\"%s\",\"id\":%lu,\"revision\":\"%llu\",\"seq\":\"%llu\",\"action\":\"%s\",\"phase\":\"%s\",\"request\":%lu,\"reason\":\"%s\",\"remaining_ms\":%lu",kind,
            (unsigned long)e.id,(unsigned long long)e.revision,(unsigned long long)e.sequence,maintenanceAction(s.action),maintenancePhase(s.phase),
            (unsigned long)s.request,maintenanceReason(s.reason),(unsigned long)s.remaining_ms);
        if(e.kind==Kind::maintenance_result)w.add(",\"status\":\"%s\"",maintenanceResult(e.maintenance_result));
        w.add("}");return w.ok?w.used:0;
    }
    if(e.kind==Kind::settings_snapshot||e.kind==Kind::settings_update||e.kind==Kind::settings_result) {
        const char* kind=e.kind==Kind::settings_snapshot?"settings.snapshot":e.kind==Kind::settings_update?"settings.update":"settings.result";
        w.add("{\"v\":1,\"type\":\"%s\",\"id\":%lu,\"revision\":\"%llu\",\"seq\":\"%llu\",\"brightness\":%u,\"timeout\":%u,\"volume\":%u,\"persisted\":%u",kind,
            (unsigned long)e.id,(unsigned long long)e.revision,(unsigned long long)e.sequence,unsigned(e.values[0]),unsigned(e.values[1]),unsigned(e.values[2]),unsigned(e.domain));
        if(e.kind==Kind::settings_result) {
            const char* keys[]={"brightness","timeout","volume"};
            const char* results[]={"applied","no_change","storage_error","invalid","conflict"};
            w.add(",\"setting\":\"%s\",\"status\":\"%s\"",keys[unsigned(e.setting)],results[unsigned(e.settings_result)]);
            if(e.settings_edit!=DeviceSettings::Edit::none) {
                const char* phases[]={"","preview","commit","cancel"};
                w.add(",\"phase\":\"%s\",\"gesture\":%lu",phases[unsigned(e.settings_edit)],(unsigned long)e.settings_gesture);
            }
        }
        w.add("}");return w.ok?w.used:0;
    }
    const char* kinds[]={"hello","state.snapshot","state.update","state.update","state.update","command.ack","command.error"};
    w.add("{\"v\":1,\"type\":\"%s\",\"id\":%lu,\"revision\":\"%llu\",\"seq\":\"%llu\"",kinds[unsigned(e.kind)],
        (unsigned long)e.id,(unsigned long long)e.revision,(unsigned long long)e.sequence);
    if(e.kind==Kind::hello) w.add(",\"channels\":512,\"max_message\":256,\"max_clients\":4,\"max_outstanding\":4");
    else if(e.kind==Kind::ack||e.kind==Kind::error) {
        w.add(",\"status\":\"%s\",\"domain\":%u",statusName(e.result),unsigned(e.domain));
        if(e.reason)w.add(",\"reason\":\"%s\"",e.reason);
    }
    else {
        w.add(",\"blackout\":%s,\"sender\":%s",e.blackout?"true":"false",e.sender?"true":"false");
        w.add(",\"loaded\":%u,\"modified\":%s",unsigned(e.loaded),e.modified?"true":"false");
        if(e.kind==Kind::channel)w.add(",\"channel\":%u,\"value\":%u",unsigned(e.channel),unsigned(e.value));
        if(e.kind==Kind::snapshot||e.kind==Kind::universe) {
            w.add(",\"values\":[");
            for(unsigned i=0;i<512;++i)w.add("%s%u",i?",":"",unsigned(e.values[i]));
            w.add("],\"presets\":[");
            for(unsigned i=0;i<8;++i)w.add("%s%u",i?",":"",unsigned(e.presets[i]));
            w.add("]");
        }
    }
    w.add("}");return w.ok?w.used:0;
}
}
