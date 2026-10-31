#pragma once
#include <stdint.h>
extern uint32_t fake_us;
inline int64_t esp_timer_get_time() { return fake_us++; }
