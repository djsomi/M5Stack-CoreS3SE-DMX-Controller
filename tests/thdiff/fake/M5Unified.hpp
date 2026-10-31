#pragma once
#include <stddef.h>
#include "../../touch_config/fake/M5Unified.hpp"
namespace lgfx {namespace i2c {
cpp::Result transactionWrite(int,int,const uint8_t*,uint8_t,uint32_t);
cpp::Result writeRegister8(int,int,uint8_t,uint8_t,uint8_t,uint32_t);
}}
