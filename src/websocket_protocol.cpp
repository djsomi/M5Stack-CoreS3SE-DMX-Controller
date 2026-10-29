#include "websocket_bridge.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

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
    uint32_t nums[11]{};unsigned fields=0;char type[24]{},setting[16]{},base[21]{};
    if(!p.eat('{'))return false;
    do {
        char key[16];if(!p.string(key,sizeof(key))||!p.eat(':'))return false;
        const char* keys[]={"v","id","type","channel","value","active","slot","target","setting","gesture","base"};
        unsigned k=0;while(k<11&&strcmp(key,keys[k]))++k;
        if(k==11||(fields&(1U<<k)))return false;
        fields|=1U<<k;
        if(k==2){if(!p.string(type,sizeof(type)))return false;}
        else if(k==8){if(!p.string(setting,sizeof(setting)))return false;}
        else if(k==10){if(!p.string(base,sizeof(base)))return false;}
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
    if(!strcmp(type,"settings.request"))r.settings=true;
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
size_t encode(const Event& e,char* out,size_t capacity) {
    Writer w(out,capacity);
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
