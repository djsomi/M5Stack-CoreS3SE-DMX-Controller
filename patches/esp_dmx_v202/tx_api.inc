/* STAGE3_TX: task-side API; no frame-length waits in start/poll/cancel. */
static bool dmx_tx_owner(dmx_port_t n) {
  return !xPortInIsrContext() && n >= 0 && n < DMX_NUM_MAX && p_dmx_obj[n] &&
         p_dmx_obj[n]->tx_owner == xTaskGetCurrentTaskHandle();
}

esp_err_t dmx_write_packet(dmx_port_t n, const void *packet, uint16_t size) {
  if (!dmx_tx_owner(n)) return ESP_ERR_INVALID_STATE;
  dmx_obj_t *o = p_dmx_obj[n];
  if (!packet || size > o->buf_size) return ESP_ERR_INVALID_ARG;
  if (o->mode != DMX_MODE_WRITE || o->tx.phase == DMX_TX_PREPARING)
    return ESP_ERR_INVALID_STATE;
  memcpy(o->buffer[0], packet, size);
  return ESP_OK;
}
esp_err_t dmx_write_slot(dmx_port_t n, uint16_t slot, uint8_t value) {
  if (!dmx_tx_owner(n)) return ESP_ERR_INVALID_STATE;
  dmx_obj_t *o = p_dmx_obj[n];
  if (slot >= o->buf_size) return ESP_ERR_INVALID_ARG;
  if (o->mode != DMX_MODE_WRITE || o->tx.phase == DMX_TX_PREPARING)
    return ESP_ERR_INVALID_STATE;
  o->buffer[0][slot] = value;
  return ESP_OK;
}
esp_err_t dmx_tx_poll(dmx_port_t n, dmx_tx_status_t *status) {
  if (!dmx_tx_owner(n) || !status) return ESP_ERR_INVALID_STATE;
  dmx_obj_t *o = p_dmx_obj[n];
  portENTER_CRITICAL(&dmx_context[n].spinlock);
  if (o->mode == DMX_MODE_WRITE) dmx_tx_poll_locked(o, esp_timer_get_time());
  status->tx = o->tx;
  status->write_mode = o->mode == DMX_MODE_WRITE;
  status->can_start = status->write_mode && o->tx.phase == DMX_TX_READY && dmx_tx_idle(n);
  portEXIT_CRITICAL(&dmx_context[n].spinlock);
  return ESP_OK;
}
esp_err_t dmx_tx_cancel(dmx_port_t n) {
  if (!dmx_tx_owner(n)) return ESP_ERR_INVALID_STATE;
  dmx_obj_t *o = p_dmx_obj[n];
  portENTER_CRITICAL(&dmx_context[n].spinlock);
  if (o->tx.phase == DMX_TX_PREPARING) {
    portEXIT_CRITICAL(&dmx_context[n].spinlock);
    return ESP_ERR_INVALID_STATE;
  }
  if (o->mode == DMX_MODE_WRITE && o->tx.phase != DMX_TX_READY &&
      o->tx.phase != DMX_TX_FAULT_STOPPED) {
    dmx_tx_fail(&o->tx, DMX_TX_CANCELLED, esp_timer_get_time());
    dmx_tx_masks(o);
  }
  portEXIT_CRITICAL(&dmx_context[n].spinlock);
  return ESP_OK;
}
esp_err_t dmx_tx_start(dmx_port_t n, uint16_t size, uint32_t *attempt) {
  if (!dmx_tx_owner(n)) return ESP_ERR_INVALID_STATE;
  dmx_obj_t *o = p_dmx_obj[n];
  if (!size || size > o->buf_size) return ESP_ERR_INVALID_ARG;
  portENTER_CRITICAL(&dmx_context[n].spinlock);
  dmx_tx_poll_locked(o, esp_timer_get_time());
  if (o->mode != DMX_MODE_WRITE || o->tx.phase != DMX_TX_READY || !dmx_tx_idle(n)) {
    portEXIT_CRITICAL(&dmx_context[n].spinlock);
    return ESP_ERR_INVALID_STATE;
  }
  dmx_hal_disable_intr_mask(&dmx_context[n].hal, DMX_INTR_TX_ALL);
  dmx_hal_clr_intsts_mask(&dmx_context[n].hal, DMX_INTR_TX_ALL);
  o->tx.phase = DMX_TX_PREPARING;
  const bool force_break = o->tx.force_break;
  portEXIT_CRITICAL(&dmx_context[n].spinlock);

  /* RX is disabled; PREPARING reserves both packet roles. Owner task only. */
  memcpy(o->buffer[1], o->buffer[0], size);
  const int64_t now = esp_timer_get_time();
  uint32_t baud, brk, mab;
  baud = dmx_hal_get_baudrate(&dmx_context[n].hal);
  brk = dmx_hal_get_break_num(&dmx_context[n].hal);
  mab = dmx_hal_get_idle_num(&dmx_context[n].hal);
  if (force_break || now - o->tx_last_brk_ts >= DMX_TX_MAX_BRK_TO_BRK_US) {
    /* Explicitly retained legacy bounded framing exception. No new timer. */
    dmx_hal_inverse_signal(&dmx_context[n].hal, UART_SIGNAL_TXD_INV);
    ets_delay_us(get_brk_us(baud, brk));
    dmx_hal_inverse_signal(&dmx_context[n].hal, 0);
    ets_delay_us(get_mab_us(baud, mab));
  }
  portENTER_CRITICAL(&dmx_context[n].spinlock);
  if (++o->tx.attempt == 0) ++o->tx.attempt;
  o->tx.phase = DMX_TX_FEEDING;
  o->tx.result = DMX_TX_PENDING;
  o->tx.requested = size;
  o->tx.queued = 0;
  o->tx.done_seen = false;
  o->tx.first_fifo_us = -1;
  o->tx.force_break = false;
  o->tx.frame_us = ((uint64_t)(size * 11 + brk + mab) * 1000000 + baud - 1) / baud;
  o->tx.started_us = esp_timer_get_time();
  /* Conservative break age: first FIFO start, not delayed completion observation. */
  o->tx_last_brk_ts = o->tx.started_us;
  dmx_tx_fill_locked(o, o->tx.started_us);
  if (attempt) *attempt = o->tx.attempt;
  portEXIT_CRITICAL(&dmx_context[n].spinlock);
  return ESP_OK;
}
esp_err_t dmx_send_packet(dmx_port_t n, uint16_t size) {
  return dmx_tx_start(n, size, NULL);
}
esp_err_t dmx_wait_send_done(dmx_port_t n, TickType_t ticks) {
  const TickType_t start = xTaskGetTickCount();
  do {
    dmx_tx_status_t s;
    esp_err_t err = dmx_tx_poll(n, &s);
    if (err != ESP_OK || !s.write_mode) return ESP_ERR_INVALID_STATE;
    if (s.tx.phase == DMX_TX_FAULT_STOPPED) return ESP_FAIL;
    if (s.can_start) return s.tx.result == DMX_TX_PENDING || s.tx.result == DMX_TX_COMPLETE
                              ? ESP_OK : ESP_FAIL;
    if (!ticks || (TickType_t)(xTaskGetTickCount() - start) >= ticks) break;
    vTaskDelay(1); /* Legacy shutdown only; normal transport always polls once. */
  } while (true);
  return ESP_ERR_TIMEOUT;
}
