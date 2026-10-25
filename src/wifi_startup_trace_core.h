#pragma once
#include <stddef.h>
#include <stdint.h>
namespace wifi_startup_trace {
struct Snapshot {
    uint32_t ms, total, internal, largest, dma_free, dma_largest, minimum, psram;
};
struct Entry {
    const char* step;
    Snapshot memory;
    const void* pointer;
    uint32_t detail;
    int result;
    bool failed, before;
};
// Append-only, never overwrite the earliest evidence. Locking belongs to adapter.
template<size_t Capacity> struct History {
    Entry entries[Capacity]{};
    Entry first{};
    size_t count=0, dropped=0;
    bool first_present=false;
    void append(const Entry& e) {
        if(e.failed && !first_present) { first=e; first_present=true; }
        if(count<Capacity) entries[count++]=e;
        else ++dropped;
    }
};
}
