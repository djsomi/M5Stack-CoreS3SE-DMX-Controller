#pragma once
#include <stdint.h>
#include <stddef.h>
inline uint32_t esp_rom_crc32_le(uint32_t crc,const uint8_t* data,size_t length){crc=~crc;while(length--){crc^=*data++;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320U&uint32_t(-int32_t(crc&1)));}return ~crc;}
