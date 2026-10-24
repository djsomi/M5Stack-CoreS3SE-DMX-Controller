esp_err_t dmx_set_mode(dmx_port_t n, dmx_mode_t mode) {
  if (n < 0 || n >= DMX_NUM_MAX || mode < 0 || mode >= DMX_MODE_MAX)
    return ESP_ERR_INVALID_ARG;
  dmx_obj_t *o = p_dmx_obj[n];
  if (!o || xPortInIsrContext() || o->tx_owner != xTaskGetCurrentTaskHandle()) return ESP_ERR_INVALID_STATE;
  if (mode == DMX_MODE_WRITE && o->intr_io_num != -1) dmx_sniffer_disable(n);
  portENTER_CRITICAL(&dmx_context[n].spinlock);
  if (o->mode == mode) {
    portEXIT_CRITICAL(&dmx_context[n].spinlock);
    return ESP_OK;
  }
  if (dmx_tx_owned(&o->tx) || !dmx_tx_idle(n)) {
    portEXIT_CRITICAL(&dmx_context[n].spinlock);
    return ESP_ERR_INVALID_STATE;
  }
  /* The ISR takes this same lock before inspecting mode or reading any buffer. */
  dmx_hal_disable_intr_mask(&dmx_context[n].hal, DMX_INTR_RX_ALL | DMX_INTR_TX_ALL);
  dmx_hal_clr_intsts_mask(&dmx_context[n].hal, DMX_ALL_INTR_MASK);
  o->buf_idx = 0;
  o->slot_idx = mode == DMX_MODE_READ ? (uint16_t)-1 : 0;
  o->mode = mode;
  o->tx.force_break = true;
  if (mode == DMX_MODE_READ) {
    dmx_hal_rxfifo_rst(&dmx_context[n].hal);
    dmx_hal_set_rts(&dmx_context[n].hal, 1);
    dmx_hal_ena_intr_mask(&dmx_context[n].hal, DMX_INTR_RX_ALL);
  } else {
    dmx_hal_txfifo_rst(&dmx_context[n].hal);
    dmx_hal_set_rts(&dmx_context[n].hal, 0);
  }
  portEXIT_CRITICAL(&dmx_context[n].spinlock);
  if (mode == DMX_MODE_WRITE) bzero(o->buffer[0], o->buf_size);
  return ESP_OK;
}
