#pragma once
#include <stdint.h>

// Optional bounded investigation tool. Standard production/Stage0 builds keep
// it OFF; enable explicitly with DMX_STAGE5A_TRACE=1 in a normal Unit build.
// It never initializes USB. /status diagnostics are independent of this flag.
#ifndef DMX_STAGE5A_TRACE
#define DMX_STAGE5A_TRACE 0
#endif

namespace wifi_startup_trace {
#if DMX_STAGE5A_TRACE
void capture(const char* step, int result=0, bool failed=false,
             const void* pointer=nullptr, uint32_t detail=0, bool before=false);
void release();
void service();
#else
inline void capture(const char*, int=0, bool=false, const void* = nullptr,
                    uint32_t=0, bool=false) {}
inline void release() {}
inline void service() {}
#endif
}
