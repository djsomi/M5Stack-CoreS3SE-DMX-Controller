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
    uint32_t nums[8]{};unsigned fields=0;char type[24]{};
    if(!p.eat('{'))return false;
    do {
        char key[16];if(!p.string(key,sizeof(key))||!p.eat(':'))return false;
        const char* keys[]={"v","id","type","channel","value","active","slot","target"};
        unsigned k=0;while(k<8&&strcmp(key,keys[k]))++k;
        if(k==8||(fields&(1U<<k)))return false;
        fields|=1U<<k;
        if(k==2){if(!p.string(type,sizeof(type)))return false;}
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
    if(!strcmp(type,"state.request"))r.snapshot=true;
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
