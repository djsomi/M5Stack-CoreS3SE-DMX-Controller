#pragma once
/* Included after HAL, driver and UART masks. All callers hold the port lock. */
static inline __attribute__((always_inline)) bool dmx_tx_raw_done(dmx_port_t n) {
  return (dmx_context[n].hal.dev->int_raw.val & UART_INTR_TX_DONE) != 0;
}
static inline __attribute__((always_inline)) bool dmx_tx_idle(dmx_port_t n) {
  return uart_ll_is_tx_idle(dmx_context[n].hal.dev);
}
static void IRAM_ATTR dmx_tx_masks(dmx_obj_t *o) {
  uart_hal_context_t *h = &dmx_context[o->dmx_num].hal;
  dmx_hal_disable_intr_mask(h, DMX_INTR_TX_ALL);
  if (o->tx.phase == DMX_TX_FEEDING)
    dmx_hal_ena_intr_mask(h, UART_INTR_TXFIFO_EMPTY | UART_INTR_RS485_CLASH);
  else if (o->tx.phase == DMX_TX_DRAINING && !o->tx.done_seen)
    dmx_hal_ena_intr_mask(h, UART_INTR_TX_DONE | UART_INTR_RS485_CLASH);
}
static void IRAM_ATTR dmx_tx_fill_locked(dmx_obj_t *o, int64_t now) {
  const dmx_port_t n = o->dmx_num;
  if (dmx_tx_before_fill(&o->tx, dmx_tx_raw_done(n), now)) {
    uint32_t written = 0;
    dmx_hal_write_txfifo(&dmx_context[n].hal, o->buffer[1] + o->tx.queued,
                         o->tx.requested - o->tx.queued, &written);
    if (written && o->tx.queued == 0) o->tx.first_fifo_us = esp_timer_get_time();
    dmx_tx_after_fill(&o->tx, written, dmx_tx_raw_done(n), now);
  }
  dmx_hal_clr_intsts_mask(&dmx_context[n].hal, UART_INTR_TXFIFO_EMPTY);
  /* Do not clear TX_DONE at the final refill/arming boundary. */
  dmx_tx_masks(o);
}
static void IRAM_ATTR dmx_tx_poll_locked(dmx_obj_t *o, int64_t now) {
  if (dmx_tx_raw_done(o->dmx_num)) {
    dmx_tx_observe_done(&o->tx, now);
    if (o->tx.phase == DMX_TX_DRAINING)
      dmx_hal_clr_intsts_mask(&dmx_context[o->dmx_num].hal, UART_INTR_TX_DONE);
  }
  dmx_tx_advance(&o->tx, dmx_tx_idle(o->dmx_num), now);
  dmx_tx_masks(o);
  if (o->tx.phase == DMX_TX_READY)
    dmx_hal_clr_intsts_mask(&dmx_context[o->dmx_num].hal, DMX_INTR_TX_ALL);
}
