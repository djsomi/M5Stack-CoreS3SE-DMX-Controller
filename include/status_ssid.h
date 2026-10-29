#pragma once
#include <stddef.h>
#include <stdint.h>

namespace status_telemetry {
// Current credential policy accepts printable ASCII SSIDs, 1..32 bytes.
// Fail closed on an unsupported name; never serialize arbitrary driver memory.
inline bool ssidJson(const uint8_t* bytes, size_t length, char (&out)[67]) {
    out[0]=0;
    if(!bytes || length==0 || length>32) return false;
    size_t n=0;out[n++]='"';
    for(size_t i=0;i<length;++i) {
        const uint8_t c=bytes[i];
        if(c<32 || c>126) {out[0]=0;return false;}
        if(c=='"' || c=='\\') out[n++]='\\';
        out[n++]=char(c);
    }
    out[n++]='"';out[n]=0;return true;
}
}
