#pragma once
#include "hardware_diagnostics.h"
#include <stddef.h>
#include <stdint.h>

#ifndef DMX_TOUCH_CONFIG_DIAGNOSTICS
#define DMX_TOUCH_CONFIG_DIAGNOSTICS 0
#endif
#if DMX_TOUCH_CONFIG_DIAGNOSTICS && !DMX_HW_DIAGNOSTICS
#error "Touch configuration snapshot requires hardware diagnostics"
#endif

namespace touch_configuration {
#if DMX_TOUCH_CONFIG_DIAGNOSTICS
// Normal-mode readable bytes only. Documentation and cell references are in
// docs/FT6336U_READ_ONLY_CONFIGURATION.md. No factory-mode access is needed.
constexpr size_t register_count = 12;
enum class Admission : uint8_t { ready, other_board, touch_disabled, no_driver, bus_mismatch };
enum class Status : uint8_t { unread, ok, error, skipped_context, skipped_identity };
struct Context {
    Admission admission = Admission::other_board;
    const char* task = "unknown";
    uint32_t frequency = 0;
    unsigned board = 0;
    int port = -1, address = -1, sda = -1, scl = -1, irq = -1;
};
struct ReadResult { bool ok = false; uint8_t value = 0; int error = 0; };
struct Entry {
    uint32_t start_us = 0, end_us = 0;
    int error = 0;
    uint8_t address = 0, value = 0;
    Status status = Status::unread;
};
using Reader = ReadResult (*)(void*, uint8_t);
using Clock = uint32_t (*)();
struct Snapshot {
    Context context;
    Entry entries[register_count];
    uint32_t start_us = 0, end_us = 0;
    bool attempted = false, identity_ok = false;
    bool capture(const Context&, Reader, Clock, void*);
    size_t format(size_t row, char* output, size_t capacity) const;
    static constexpr size_t row_count = register_count + 2;
};
void captureStartup();
const Snapshot& startupSnapshot();
bool pending();
size_t formatNext(char* output, size_t capacity);
#else
inline void captureStartup() {}
inline bool pending() { return false; }
inline size_t formatNext(char*, size_t) { return 0; }
#endif
}
