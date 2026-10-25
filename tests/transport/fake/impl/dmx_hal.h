#pragma once
#include "driver.h"
static inline bool uart_ll_is_tx_idle(FakeUart* u) { return u->idle && u->fifo.empty() && !u->inverted; }
static inline uint32_t dmx_hal_get_intsts_mask(uart_hal_context_t* h) {
    uint32_t flags = h->dev->int_raw.val;
    if (h->dev->fifo.size() < 8) flags |= (1u << 1);
    return flags & h->dev->enabled;
}
static inline void dmx_hal_disable_intr_mask(uart_hal_context_t* h, uint32_t m) { h->dev->enabled &= ~m; }
static inline void dmx_hal_ena_intr_mask(uart_hal_context_t* h, uint32_t m) {
    h->dev->enabled |= m;
    if ((m & (1u << 14)) && h->dev->inject_done_on_enable) {
        h->dev->int_raw.val |= (1u << 14);
        h->dev->inject_done_on_enable = false;
    }
}
static inline void dmx_hal_clr_intsts_mask(uart_hal_context_t* h, uint32_t m) { h->dev->int_raw.val &= ~m; }
static inline void dmx_hal_write_txfifo(uart_hal_context_t* h, const uint8_t* data, uint32_t n, uint32_t* written) {
    ++h->dev->snapshot_reads;
    if (h->dev->inject_clash_in_write) {
        h->dev->int_raw.val |= (1u << 17);
        h->dev->inject_clash_in_write = false;
    }
    if (h->dev->fail_fifo_write) { *written = 0; return; }
    if (h->dev->inject_done_in_write) {
        h->dev->int_raw.val |= (1u << 14);
        h->dev->inject_done_in_write = false;
    }
    *written = std::min(n, static_cast<uint32_t>(128 - h->dev->fifo.size()));
    for (unsigned i = 0; i < *written; ++i) h->dev->fifo.push_back(data[i]);
    if (*written) h->dev->idle = false;
}
static inline uint32_t dmx_hal_get_rxfifo_len(uart_hal_context_t*) { return 0; }
static inline void dmx_hal_read_rxfifo(uart_hal_context_t*, uint8_t*, unsigned) {}
static inline void dmx_hal_rxfifo_rst(uart_hal_context_t*) {}
static inline void dmx_hal_txfifo_rst(uart_hal_context_t* h) { h->dev->fifo.clear(); }
static inline void dmx_hal_set_rts(uart_hal_context_t*, int) {}
static inline uint32_t dmx_hal_get_rx_level(uart_hal_context_t*) { return 1; }
static inline unsigned dmx_hal_get_baudrate(uart_hal_context_t*) { return 250000; }
static inline unsigned dmx_hal_get_break_num(uart_hal_context_t*) { return 44; }
static inline unsigned dmx_hal_get_idle_num(uart_hal_context_t*) { return 3; }
static inline void dmx_hal_inverse_signal(uart_hal_context_t* h, uint32_t m) { h->dev->inverted = m != 0; }
