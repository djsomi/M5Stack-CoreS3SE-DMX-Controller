#pragma once

#ifndef DMX_HW_DIAGNOSTICS
#define DMX_HW_DIAGNOSTICS 0
#endif

#if DMX_HW_DIAGNOSTICS
#include <stdint.h>

namespace hardware_diagnostics {
void begin();
void capture(const char* event, const char* screen, const char* owner,
             bool canvas_ready, const void* canvas_buffer,
             uint8_t preset = 0, int result = 0, bool blackout = false);
void service();
}
#endif
