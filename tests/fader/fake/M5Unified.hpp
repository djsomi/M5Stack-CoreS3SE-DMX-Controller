#pragma once
#include "Touch_Class.hpp"
#include "Arduino.h"
struct FakeM5 {m5gfx::LGFX_Device Display;m5::Touch_Class Touch;uint32_t getUpdateMsec()const{return millis();}};
extern FakeM5 M5;
