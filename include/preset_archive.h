#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace preset_archive {
constexpr size_t slots=8, channels=512, max_file=32768, chunk_size=96;
constexpr size_t export_chunk_size=512; // Reuse Event's existing 512-byte payload; imports remain 96 bytes.
constexpr const char* format="cores3se-dmx-presets";
struct Collection {uint8_t present=0;uint8_t values[slots][channels]{};};
struct Gate {bool (*check)(void*)=nullptr;void* context=nullptr;bool allowed()const{return !check||check(context);}};
enum class Commit {ok,failed,uncertain};
inline unsigned count(uint8_t mask){unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;}

// Fixed grammar, bounded input/depth and no allocation. Exact keys, fixed P1-P8
// identities, complete 512-byte values; JSON ASCII escapes are accepted.
class Reader {
 const char* p_;const char* end_;
 public:
 Reader(const char* p,size_t n):p_(p),end_(p+n){if(n>=3&&uint8_t(p[0])==239&&uint8_t(p[1])==187&&uint8_t(p[2])==191)p_+=3;}
 void space(){while(p_<end_&&(*p_==' '||*p_=='\r'||*p_=='\n'||*p_=='\t'))++p_;}
 bool eat(char c){space();if(p_==end_||*p_!=c)return false;++p_;return true;}
 bool at(char c){space();return p_<end_&&*p_==c;}
 bool done(){space();return p_==end_;}
 static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
 bool text(char* out,size_t cap){
  if(!eat('"'))return false;size_t n=0;
  while(p_<end_&&*p_!='"'){
   unsigned char c=uint8_t(*p_++);
   if(c=='\\'){
    if(p_==end_)return false;c=uint8_t(*p_++);
    if(c=='u'){unsigned value=0;for(unsigned i=0;i<4;++i){if(p_==end_)return false;int x=hex(*p_++);if(x<0)return false;value=value*16+unsigned(x);}if(value<32||value>126)return false;c=uint8_t(value);}
    else if(c!='"'&&c!='\\'&&c!='/')return false;
   }
   if(c<32||c>126||n+1>=cap)return false;out[n++]=char(c);
  }out[n]=0;return eat('"');
 }
 bool number(unsigned& value){space();value=0;const char* first=p_;if(p_==end_||*p_<'0'||*p_>'9')return false;
  while(p_<end_&&*p_>='0'&&*p_<='9'){unsigned x=unsigned(*p_++-'0');if(value>(UINT32_MAX-x)/10)return false;value=value*10+x;}
  return p_-first==1||*first!='0';
 }
};
inline bool parse(const char* data,size_t size,Collection& out){
 out.present=0;memset(out.values,0,sizeof(out.values));if(!data||!size||size>max_file)return false;Reader r(data,size);if(!r.eat('{'))return false;unsigned fields=0;
 do{
  char key[16];if(!r.text(key,sizeof(key))||!r.eat(':'))return false;
  unsigned field=!strcmp(key,"format")?1:!strcmp(key,"version")?2:!strcmp(key,"presets")?4:0;
  if(!field||(fields&field))return false;fields|=field;
  if(field==1){char value[32];if(!r.text(value,sizeof(value))||strcmp(value,format))return false;}
  else if(field==2){unsigned version;if(!r.number(version)||version!=1)return false;}
  else{
   if(!r.eat('['))return false;
   if(!r.at(']'))do{
    if(!r.eat('{'))return false;unsigned seen=0,slot=0;char name[4]{};uint8_t* values=nullptr;
    // A record can put values before slot; one bounded scratch packet suffices.
    uint8_t packet[channels]{};
    do{
     if(!r.text(key,sizeof(key))||!r.eat(':'))return false;
     unsigned f=!strcmp(key,"slot")?1:!strcmp(key,"name")?2:!strcmp(key,"values")?4:0;
     if(!f||(seen&f))return false;seen|=f;
     if(f==1){if(!r.number(slot)||slot<1||slot>slots)return false;}
     else if(f==2){if(!r.text(name,sizeof(name)))return false;}
     else {if(!r.eat('['))return false;for(size_t i=0;i<channels;++i){unsigned value;if(!r.number(value)||value>255)return false;packet[i]=uint8_t(value);if(i+1<channels&&!r.eat(','))return false;}if(!r.eat(']'))return false;}
     if(r.at('}'))break;if(!r.eat(','))return false;
    }while(true);
    if(!r.eat('}')||seen!=7||name[0]!='P'||name[1]!=char('0'+slot)||name[2]||(out.present&(1U<<(slot-1))))return false;
    values=out.values[slot-1];memcpy(values,packet,channels);out.present|=uint8_t(1U<<(slot-1));
    if(r.at(']'))break;if(!r.eat(','))return false;
   }while(true);
   if(!r.eat(']'))return false;
  }
  if(r.at('}'))break;if(!r.eat(','))return false;
 }while(true);
 return r.eat('}')&&fields==7&&r.done();
}
inline size_t serialize(const Collection& c,char* out,size_t capacity){
 size_t n=0;bool ok=true;
 const auto text=[&](const char* s){while(*s){if(n>=capacity){ok=false;return;}out[n++]=*s++;}};
 const auto number=[&](unsigned value){char b[4];unsigned len=0;do{b[len++]=char('0'+value%10);value/=10;}while(value);while(len){if(n>=capacity){ok=false;return;}out[n++]=b[--len];}};
 text("{\"format\":\"cores3se-dmx-presets\",\"version\":1,\"presets\":[");bool first=true;
 for(unsigned slot=0;slot<slots;++slot)if(c.present&(1U<<slot)){
  if(!first)text(",");first=false;text("{\"slot\":");number(slot+1);text(",\"name\":\"P");number(slot+1);text("\",\"values\":[");
  for(unsigned ch=0;ch<channels;++ch){if(ch)text(",");number(c.values[slot][ch]);}text("]}");
 }
 text("]}");return ok?n:0;
}
inline int digit(char c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;}
inline bool unbase64(const char* input,uint8_t* out,size_t& length){
 length=0;const size_t n=strlen(input);if(!n||n%4||n>128)return false;
 for(size_t i=0;i<n;i+=4){int a=digit(input[i]),b=digit(input[i+1]),c=input[i+2]=='='?0:digit(input[i+2]),d=input[i+3]=='='?0:digit(input[i+3]);unsigned pad=input[i+2]=='='?2:input[i+3]=='='?1:0;
  if(a<0||b<0||c<0||d<0||(pad&&i+4!=n)||(pad==2&&(input[i+3]!='='||(b&15)))||(pad==1&&(c&3)))return false;
  uint32_t word=(uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6)|uint32_t(d);
  for(unsigned j=0;j<3-pad;++j){if(length>=chunk_size)return false;out[length++]=uint8_t(word>>(16-j*8));}
 }return true;
}
inline void base64(const uint8_t* data,size_t size,char* out){const char* a="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t n=0;
 for(size_t i=0;i<size;i+=3){uint32_t w=uint32_t(data[i])<<16;if(i+1<size)w|=uint32_t(data[i+1])<<8;if(i+2<size)w|=data[i+2];out[n++]=a[w>>18];out[n++]=a[(w>>12)&63];out[n++]=i+1<size?a[(w>>6)&63]:'=';out[n++]=i+2<size?a[w&63]:'=';}out[n]=0;
}
}
