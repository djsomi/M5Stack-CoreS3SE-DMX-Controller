#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
extern uint32_t fake_us;
inline uint32_t millis(){return fake_us/1000;}
struct FakeSerial {
    std::string rx,tx;size_t position=0,max_write=0;
    bool connected=true,unavailable=false,read_error=false,write_zero=false;int space=40;
    explicit operator bool()const{return connected;}
    int available(){return unavailable?-1:int(rx.size()-position);}
    int read(){return !read_error && position<rx.size()?uint8_t(rx[position++]):-1;}
    int availableForWrite(){return space;}
    size_t write(const uint8_t* data,size_t n){
        if(!connected || space<=0 || write_zero)return 0;
        if(n>size_t(space) || n>40)__builtin_trap();
        tx.append(reinterpret_cast<const char*>(data),n);if(n>max_write)max_write=n;return n;
    }
};
extern FakeSerial Serial;
