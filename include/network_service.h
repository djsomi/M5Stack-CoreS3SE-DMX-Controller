#pragma once
#include <stdint.h>
#include "wifi_configuration.h"
#include "wifi_startup_trace.h"
namespace network_service {
enum class Phase : uint8_t { disabled, prepared, starting, ready, failed };
struct Memory {
    uint32_t at_ms, internal, minimum, largest, dma, psram;
};
struct Status {
    Phase phase;
    int error;
    uint32_t clients, requests, rejected, max_loop_gap_us, tx_errors, worker_stack, http_stack;
    bool tx_stopped;
    char ip[16];
    Memory before, after, current;
    uint32_t ui_revision, max_clients;
    bool router_option_disabled, reset_pending;
};
// Owner/setup only, before M5/ADC/Wi-Fi/DMX initialization. No credentials logged.
void prepare();
WifiConfiguration& configuration();
void setConfigurationBusy(bool busy);
// Owner/setup only. UI buffers must already be reserved. Worker owns all radio/HTTP.
void launch(bool canvas_reserved);
void ownerTick(int64_t now, uint32_t tx_errors, bool tx_stopped);
Status status();
const char* ssid();
const char* password();
const char* credentialMode();
uint32_t uiRevision();
// Local application owner only: mark a Wi-Fi-only factory reset for NEXT boot.
// Existing credentials remain active until the user invokes the existing Restart.
bool requestFactoryReset();
const char* phaseName(Phase phase);
}
