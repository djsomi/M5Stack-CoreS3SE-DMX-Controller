#pragma once
#include <stddef.h>
#include <stdint.h>

namespace wifi_wire {
// Canonical padded Base64 transports printable ASCII without expanding the
// deliberately escape-free inbound JSON grammar or its 256-byte frame budget.
inline int digit(char c) {
    if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;return c=='+'?62:c=='/'?63:-1;
}
inline bool decode(const char* text,char* output,size_t capacity) {
    size_t n=0,length=0;while(text[length])++length;
    if(length%4)return false;
    for(size_t i=0;i<length;i+=4) {
        const int a=digit(text[i]),b=digit(text[i+1]),c=text[i+2]=='='?0:digit(text[i+2]),d=text[i+3]=='='?0:digit(text[i+3]);
        const unsigned pad=text[i+2]=='='?2:text[i+3]=='='?1:0;
        if(a<0||b<0||c<0||d<0||(pad&&i+4!=length)||(pad==2&&text[i+3]!='=')||
           (pad==2&&(b&15))||(pad==1&&(c&3)))return false;
        const uint32_t word=(uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6)|uint32_t(d);
        for(unsigned j=0;j<3-pad;++j) {
            const uint8_t value=uint8_t(word>>(16-j*8));
            if(n+1>=capacity||value<32||value>126)return false;
            output[n++]=char(value);
        }
    }
    output[n]=0;return true;
}
}
