#pragma once
#include "nvs.h"
class Preferences {nvs_handle_t h=0;public:bool begin(const char* s,bool ro){return nvs_open(s,ro?NVS_READONLY:NVS_READWRITE,&h)==0;}void end(){nvs_close(h);}size_t putBytes(const char* k,const void* p,size_t n){return nvs_set_blob(h,k,p,n)==0?n:0;}size_t getBytes(const char* k,void* p,size_t n){return nvs_get_blob(h,k,p,&n)==0?n:0;}size_t putUChar(const char* k,uint8_t v){return nvs_set_u8(h,k,v)==0?1:0;}bool remove(const char* k){return nvs_erase_key(h,k)==0;}};
