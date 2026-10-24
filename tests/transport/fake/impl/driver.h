#pragma once
#include "esp_dmx.h"
#include "../../../../patches/esp_dmx_v202/tx_core.h"
struct uart_hal_context_t { FakeUart* dev; };
struct dmx_obj_t {
    dmx_tx_state_t tx{};
    TaskHandle_t tx_owner = 1;
    dmx_port_t dmx_num = 1;
    QueueHandle_t queue = nullptr;
    uint16_t buf_size = 513, send_size = 0, slot_idx = 0;
    uint8_t* buffer[2]{};
    uint8_t buf_idx = 0;
    dmx_mode_t mode = DMX_MODE_READ;
    int64_t rx_last_brk_ts = -1250000, tx_last_brk_ts = -1000000;
    int intr_io_num = -1, rx_brk_len = -1, rx_mab_len = -1;
    bool rx_is_in_brk = false;
    int64_t rx_last_pos_edge_ts = -1, rx_last_neg_edge_ts = -1;
};
struct dmx_context_t { uart_hal_context_t hal; int spinlock = 0; };
extern dmx_context_t dmx_context[DMX_NUM_MAX];
extern dmx_obj_t* p_dmx_obj[DMX_NUM_MAX];
