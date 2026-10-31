#pragma once
#include <stdint.h>
#include <stddef.h>
extern uint32_t fake_us;
inline uint32_t millis(){return fake_us/1000;}
inline void delay(unsigned) {}
struct FakeSerial {
    char output[60000]={};size_t used=0,writes=0,max_write=0;int space=40;bool connected=true;
    explicit operator bool() const {return connected;}
    int availableForWrite() const {return space;}
    size_t write(const uint8_t* p,size_t n) {
        if(n>size_t(space)||n>40||used+n>=sizeof(output))__builtin_trap();
        for(size_t i=0;i<n;++i)output[used++]=char(p[i]);output[used]=0;
        ++writes;if(n>max_write)max_write=n;return n;
    }
};
extern FakeSerial Serial;
