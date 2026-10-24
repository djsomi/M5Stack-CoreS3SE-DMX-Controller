#pragma once
#include "esp_dmx.h"
#include "impl/tx_core.h"
#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(DMX_TX_HOST_TEST)
#error "This TX contract is reviewed only for ESP32-S3 / IDF 4.4"
#endif
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR != 4
#error "Re-audit raw UART and idle semantics before changing ESP-IDF major version"
#endif
#define DMX_TX_CONTRACT_VERSION 1
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  dmx_tx_state_t tx;
  bool write_mode;
  bool can_start;
} dmx_tx_status_t;
esp_err_t dmx_tx_start(dmx_port_t port, uint16_t slots, uint32_t *attempt);
esp_err_t dmx_tx_poll(dmx_port_t port, dmx_tx_status_t *status);
esp_err_t dmx_tx_cancel(dmx_port_t port);
#ifdef __cplusplus
}
#endif
