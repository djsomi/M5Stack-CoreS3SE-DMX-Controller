#include "touch_configuration_snapshot.h"
#if DMX_TOUCH_CONFIG_DIAGNOSTICS
#include <M5Unified.hpp>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

namespace touch_configuration {
namespace {
struct Register { uint8_t address; const char* name; };
// Read the established device identity first. An unknown/failed tuple prevents
// configuration reads; failed reads never become a fabricated zero/default.
const Register registers[register_count] = {
    {0xA3, "CIPHER_HIGH"}, {0xA6, "FIRMID"}, {0xA8, "VENDOR_ID"},
    {0x80, "THGROUP"}, {0x85, "THDIFF"}, {0x86, "CTRL"},
    {0x87, "TIMEENTERMONITOR"}, {0x88, "PERIODACTIVE"},
    {0x89, "PERIODMONITOR"}, {0xA5, "PMODE"},
    {0xA1, "LIB_VERSION_H"}, {0xA2, "LIB_VERSION_L"}
};
Snapshot snapshot;
size_t output_row = 0;

const char* admissionName(Admission value) {
    switch (value) {
        case Admission::ready: return "ready";
        case Admission::other_board: return "other_board";
        case Admission::touch_disabled: return "touch_disabled";
        case Admission::no_driver: return "no_driver";
        case Admission::bus_mismatch: return "bus_mismatch";
    }
    return "unknown";
}
const char* statusName(Status value) {
    switch (value) {
        case Status::unread: return "unread";
        case Status::ok: return "ok";
        case Status::error: return "read_error";
        case Status::skipped_context: return "skipped_context";
        case Status::skipped_identity: return "skipped_identity";
    }
    return "unknown";
}
uint32_t nowUs() { return static_cast<uint32_t>(esp_timer_get_time()); }
ReadResult readByte(void* context, uint8_t reg) {
    const auto& c = *static_cast<const Context*>(context);
    // Same backend and per-port mutex as FT5x06::_read_reg and M5.In_I2C.
    // readRegister8 preserves its result/error; M5's byte-only wrapper does not.
    const auto result = lgfx::i2c::readRegister8(c.port, c.address, reg, c.frequency);
    ReadResult read;
    read.ok = result.has_value();
    if (read.ok) read.value = result.value();
    else read.error = static_cast<int>(result.error());
    return read;
}
}

bool Snapshot::capture(const Context& incoming, Reader read, Clock clock, void* user) {
    if (attempted) return false;
    attempted = true;
    context = incoming;
    start_us = clock();
    for (size_t i = 0; i < register_count; ++i) {
        auto& entry = entries[i];
        entry.address = registers[i].address;
        if (context.admission != Admission::ready) {
            entry.status = Status::skipped_context;
            continue;
        }
        if (i >= 3 && !identity_ok) {
            entry.status = Status::skipped_identity;
            continue;
        }
        entry.start_us = clock();
        const auto result = read(user, entry.address);
        entry.end_us = clock();
        entry.status = result.ok ? Status::ok : Status::error;
        if (result.ok) entry.value = result.value;
        else entry.error = result.error;
        if (i == 2) {
            identity_ok = entries[0].status == Status::ok && entries[0].value == 0x64
                       && entries[1].status == Status::ok && entries[1].value == 0x03
                       && entries[2].status == Status::ok && entries[2].value == 0x20;
        }
    }
    end_us = clock();
    return true;
}

size_t Snapshot::format(size_t row, char* out, size_t capacity) const {
    if (!attempted || row >= row_count || !capacity) return 0;
    int n;
    if (row == 0) {
        char address[12];
        if (context.address < 0) snprintf(address, sizeof(address), "NA");
        else snprintf(address, sizeof(address), "0x%02x", static_cast<unsigned>(context.address));
        n = snprintf(out, capacity,
            "[TOUCH_CFG] phase=setup.after_M5.begin.before_network_launch_and_loop task=%s board=%u operator_marking=ESP32_CORES3SE_CAP.TOUCH_rev1 marking_scope=visible_only admission=%s port=%d addr=%s sda=%d scl=%d irq=%d hz=%lu start_us=%lu end_us=%lu duration_us=%lu once=1 error_domain=lgfx_error_t\n",
            context.task, context.board, admissionName(context.admission), context.port,
            address, context.sda, context.scl, context.irq,
            static_cast<unsigned long>(context.frequency), static_cast<unsigned long>(start_us),
            static_cast<unsigned long>(end_us), static_cast<unsigned long>(end_us-start_us));
    } else if (row <= register_count) {
        const auto& e = entries[row-1];
        if (e.status == Status::ok) {
            n = snprintf(out, capacity,
                "[TOUCH_CFG] reg=0x%02x name=%s status=ok hex=0x%02x dec=%u start_us=%lu end_us=%lu duration_us=%lu\n",
                e.address, registers[row-1].name, e.value, e.value,
                static_cast<unsigned long>(e.start_us), static_cast<unsigned long>(e.end_us),
                static_cast<unsigned long>(e.end_us-e.start_us));
        } else {
            n = snprintf(out, capacity,
                "[TOUCH_CFG] reg=0x%02x name=%s status=%s hex=NA dec=NA rc=%d start_us=%lu end_us=%lu\n",
                e.address, registers[row-1].name, statusName(e.status), e.error,
                static_cast<unsigned long>(e.start_us), static_cast<unsigned long>(e.end_us));
        }
    } else {
        unsigned ok=0, errors=0, skipped=0;
        for (const auto& e : entries) {
            ok += e.status == Status::ok;
            errors += e.status == Status::error;
            skipped += e.status == Status::skipped_context || e.status == Status::skipped_identity;
        }
        n = snprintf(out, capacity,
            "[TOUCH_CFG_END] identity_match=%u ok=%u errors=%u skipped=%u attempts=%u repeat=0 writes=0 reset=0\n",
            identity_ok ? 1U : 0U, ok, errors, skipped, ok+errors);
    }
    return n > 0 ? (static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : capacity-1) : 0;
}

void captureStartup() {
    if (snapshot.attempted) return;
    Context c;
    c.board = static_cast<unsigned>(M5.getBoard());
    c.task = pcTaskGetName(nullptr);
    if (M5.getBoard() == m5::board_t::board_M5StackCoreS3SE) {
        c.admission = Admission::touch_disabled;
        if (M5.Touch.isEnabled()) {
            c.admission = Admission::no_driver;
            const auto* panel = M5.Display.panel();
            const auto* touch = panel ? panel->getTouch() : nullptr;
            if (touch) {
                const auto cfg = touch->config();
                c.port=cfg.i2c_port; c.address=cfg.i2c_addr; c.sda=cfg.pin_sda;
                c.scl=cfg.pin_scl; c.irq=cfg.pin_int; c.frequency=cfg.freq;
                c.admission = Admission::bus_mismatch;
                if (cfg.pin_cs < 0 && c.address == 0x38 && c.frequency && c.frequency <= 400000
                    && c.port == M5.In_I2C.getPort() && c.sda == M5.In_I2C.getSDA()
                    && c.scl == M5.In_I2C.getSCL()) c.admission = Admission::ready;
            }
        }
    }
    snapshot.capture(c, readByte, nowUs, &c);
}
bool pending() { return snapshot.attempted && output_row < Snapshot::row_count; }
const Snapshot& startupSnapshot() { return snapshot; }
size_t formatNext(char* out, size_t capacity) {
    if (!pending() || !capacity) return 0;
    return snapshot.format(output_row++, out, capacity);
}
}
#endif
