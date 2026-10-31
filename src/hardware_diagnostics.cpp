#include "hardware_diagnostics.h"
#include "firmware_version.h"
#include "physical_fader_diagnostics.h"
#include "touch_configuration_snapshot.h"
#include "thdiff_experiment.h"

#if DMX_HW_DIAGNOSTICS
#include <Arduino.h>
#include <M5Unified.hpp>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <stdio.h>

#if !ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MODE != 1
#error "Stage 0 diagnostics require the tested USB Serial/JTAG build configuration"
#endif

namespace hardware_diagnostics {
namespace {
constexpr size_t snapshot_capacity = 32;
constexpr size_t tx_chunk_bytes = 40;
constexpr uint32_t tx_interval_ms = 4;

struct Snapshot {
    const char* event;
    const char* screen;
    const char* owner;
    const void* canvas_buffer;
    uint32_t at_ms;
    uint32_t psram_total;
    uint32_t psram_free;
    uint32_t internal_free;
    uint32_t minimum_free;
    uint32_t internal_minimum_free;
    uint32_t largest_internal;
    uint32_t largest_dma;
    int result;
    uint8_t preset;
    bool canvas_ready;
    bool blackout;
};

Snapshot snapshots[snapshot_capacity];
size_t read_index = 0;
size_t queued = 0;
uint32_t dropped = 0;
uint32_t sequence = 0;
uint32_t last_tx_ms = 0;
char line[768];
size_t line_size = 0;
size_t line_sent = 0;
bool banner_pending = true;
bool thdiff_line = false;
uint32_t configured_flash = 0;
uint32_t physical_flash = 0;
uint32_t flash_id = 0;
esp_err_t configured_flash_result = ESP_FAIL;
esp_err_t physical_flash_result = ESP_FAIL;
esp_err_t flash_id_result = ESP_FAIL;

const char* boardName() {
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StackCoreS3SE: return "CoreS3SE";
        case m5::board_t::board_M5StackCoreS3: return "CoreS3";
        default: return "other";
    }
}

const char* profileName() {
#if defined(DMX_HARDWARE_UNIT)
    return "unit";
#elif defined(DMX_HARDWARE_BASE)
    return "base";
#else
    return "unknown";
#endif
}

void prepareLine() {
    int length = 0;
    if (banner_pending) {
        banner_pending = false;
        length = snprintf(
            line, sizeof(line),
            "[S0] build=%s commit=%s modified=%u environment=%s inputs=%s date=%s_%s board=%s(%u) profile=%s idf=%s baud=115200 units=bytes configured_flash=%lu cfg_rc=%d physical_flash=%lu physical_rc=%d jedec=%06lx id_rc=%d diag_buffers_bytes=%u\n",
            firmware_build::info().build_id, firmware_build::info().commit,
            firmware_build::info().modified ? 1U : 0U, firmware_build::info().environment,
            firmware_build::info().source_sha256,
            __DATE__, __TIME__, boardName(), static_cast<unsigned>(M5.getBoard()),
            profileName(), esp_get_idf_version(),
            static_cast<unsigned long>(configured_flash), configured_flash_result,
            static_cast<unsigned long>(physical_flash), physical_flash_result,
            static_cast<unsigned long>(flash_id), flash_id_result,
            static_cast<unsigned>(sizeof(snapshots) + sizeof(line))
        );
    } else if (thdiff::pending()) {
        length = static_cast<int>(thdiff::formatNext(line, sizeof(line)));
        thdiff_line = true;
    } else if (touch_configuration::pending()) {
        length = static_cast<int>(touch_configuration::formatNext(line, sizeof(line)));
    } else if (queued != 0) {
        const Snapshot& s = snapshots[read_index];
        length = snprintf(
            line, sizeof(line),
            "[S0] n=%lu t_ms=%lu event=%s screen=%s owner=%s canvas=%s ptr=%p psram_total=%lu psram_free=%lu internal_free=%lu minimum_free=%lu internal_min=%lu largest_internal8=%lu largest_dma=%lu preset=%u rc=%d blackout=%u dropped=%lu\n",
            static_cast<unsigned long>(++sequence),
            static_cast<unsigned long>(s.at_ms), s.event, s.screen, s.owner,
            s.canvas_ready ? "ready" : (s.canvas_buffer ? "inconsistent" : "none"),
            s.canvas_buffer, static_cast<unsigned long>(s.psram_total),
            static_cast<unsigned long>(s.psram_free),
            static_cast<unsigned long>(s.internal_free),
            static_cast<unsigned long>(s.minimum_free),
            static_cast<unsigned long>(s.internal_minimum_free),
            static_cast<unsigned long>(s.largest_internal),
            static_cast<unsigned long>(s.largest_dma), s.preset, s.result,
            s.blackout ? 1U : 0U, static_cast<unsigned long>(dropped)
        );
        read_index = (read_index + 1) % snapshot_capacity;
        --queued;
    }
    line_size = length > 0 ? static_cast<size_t>(length) : 0;
    if (line_size >= sizeof(line)) {
        line_size = sizeof(line) - 1;
    }
    line_sent = 0;
}
}

void begin() {
    // These two environments use the installed non-waiting USB Serial/JTAG API.
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    configured_flash_result = esp_flash_get_size(nullptr, &configured_flash);
    physical_flash_result = esp_flash_get_physical_size(nullptr, &physical_flash);
    flash_id_result = esp_flash_read_id(nullptr, &flash_id);
    touch_configuration::captureStartup();
    thdiff::begin();
}

void capture(const char* event, const char* screen, const char* owner,
             bool canvas_ready, const void* canvas_buffer,
             uint8_t preset, int result, bool blackout) {
    // All callers run on the application task. Labels have static lifetime.
    if (queued == snapshot_capacity) {
        ++dropped;
        return;
    }
    Snapshot& s = snapshots[(read_index + queued) % snapshot_capacity];
    s.event = event;
    s.screen = screen;
    s.owner = owner;
    s.canvas_ready = canvas_ready;
    s.canvas_buffer = canvas_buffer;
    s.preset = preset;
    s.result = result;
    s.blackout = blackout;
    s.at_ms = millis();
    s.psram_total = ESP.getPsramSize();
    s.psram_free = ESP.getFreePsram();
    constexpr uint32_t internal8 = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    s.internal_free = heap_caps_get_free_size(internal8);
    s.minimum_free = ESP.getMinFreeHeap();
    s.internal_minimum_free = heap_caps_get_minimum_free_size(internal8);
    s.largest_internal = heap_caps_get_largest_free_block(internal8);
    s.largest_dma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    ++queued;
}

void service() {
    thdiff::poll(); // RX/deadline/rollback run even while diagnostic TX is paused.
#if DMX_FADER_DIAGNOSTICS
    // Pause all diagnostic output during touch. Finish any S0 line before FADER
    // owns this same non-waiting USB path; never interleave partial lines.
    if (physical_fader::touchActive()) return;
    if (line_sent == line_size && !thdiff::pending() && physical_fader::service()) return;
#endif
    if ((!banner_pending && queued == 0 && line_sent == line_size && !touch_configuration::pending() && !thdiff::pending()) || !Serial) {
        return;
    }
    uint32_t now = millis();
    if (static_cast<uint32_t>(now - last_tx_ms) < tx_interval_ms) {
        return;
    }
    last_tx_ms = now;
    int available = Serial.availableForWrite();
    if (available <= 0) {
        return;
    }
    if (line_sent == line_size) {
        prepareLine();
    }
    size_t count = line_size - line_sent;
    if (count > tx_chunk_bytes) count = tx_chunk_bytes;
    if (count > static_cast<size_t>(available)) count = available;
    // Never wait for a host, flush, or send beyond the currently free TX space.
    const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(line + line_sent), count);
    if (!written || written > count) thdiff::outputError();
    if (written <= count) line_sent += written;
    if (line_sent == line_size && thdiff_line) {
        thdiff_line = false;
        thdiff::lineSent();
    }
}
}
#endif
