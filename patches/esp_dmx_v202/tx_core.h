#ifndef DMX_TX_CORE_H
#define DMX_TX_CORE_H
#include <stdbool.h>
#include <stdint.h>

/* Canonical state transitions: caller holds the port lock. No allocator/RTOS. */
#ifndef DMX_TX_INLINE
#define DMX_TX_INLINE static inline
#endif
typedef enum {
  DMX_TX_READY, DMX_TX_PREPARING, DMX_TX_FEEDING, DMX_TX_DRAINING,
  DMX_TX_RETIRING, DMX_TX_FAULT_STOPPED
} dmx_tx_phase_t;
typedef enum {
  DMX_TX_PENDING, DMX_TX_COMPLETE, DMX_TX_UNDERRUN, DMX_TX_TIMEOUT,
  DMX_TX_CANCELLED, DMX_TX_START_FAILED, DMX_TX_COLLISION
} dmx_tx_result_t;
typedef struct {
  dmx_tx_phase_t phase;
  dmx_tx_result_t result;
  uint32_t attempt;
  uint16_t queued, requested;
  bool done_seen;
  bool force_break;
  int64_t started_us, retired_us;
  int64_t first_fifo_us; /* Software observation of the first actual FIFO write. */
  uint32_t frame_us;
} dmx_tx_state_t;

DMX_TX_INLINE bool dmx_tx_owned(const dmx_tx_state_t *s) {
  return s->phase != DMX_TX_READY;
}
DMX_TX_INLINE void dmx_tx_fail(dmx_tx_state_t *s, dmx_tx_result_t error, int64_t now) {
  if (s->phase == DMX_TX_RETIRING || s->phase == DMX_TX_FAULT_STOPPED) return;
  s->phase = DMX_TX_RETIRING;
  s->result = error;
  s->retired_us = now;
  s->force_break = true;
}
DMX_TX_INLINE bool dmx_tx_before_fill(dmx_tx_state_t *s, bool raw_done, int64_t now) {
  if (s->phase != DMX_TX_FEEDING) return false;
  if (raw_done) { dmx_tx_fail(s, DMX_TX_UNDERRUN, now); return false; }
  return true;
}
DMX_TX_INLINE void dmx_tx_after_fill(dmx_tx_state_t *s, uint32_t written,
                                     bool raw_done, int64_t now) {
  if (s->phase != DMX_TX_FEEDING) return;
  if (!written || written > (uint32_t)(s->requested - s->queued)) {
    dmx_tx_fail(s, DMX_TX_START_FAILED, now); return;
  }
  s->queued += written;
  /* A done latched during refill is ambiguous even when this was the last slot. */
  if (raw_done) dmx_tx_fail(s, DMX_TX_UNDERRUN, now);
  else if (s->queued == s->requested) s->phase = DMX_TX_DRAINING;
}
DMX_TX_INLINE void dmx_tx_observe_done(dmx_tx_state_t *s, int64_t now) {
  if (s->phase == DMX_TX_FEEDING) dmx_tx_fail(s, DMX_TX_UNDERRUN, now);
  else if (s->phase == DMX_TX_DRAINING) s->done_seen = true;
}
DMX_TX_INLINE void dmx_tx_advance(dmx_tx_state_t *s, bool idle, int64_t now) {
  if (s->phase == DMX_TX_DRAINING && s->done_seen && idle &&
      s->queued == s->requested && s->result == DMX_TX_PENDING) {
    s->result = DMX_TX_COMPLETE;
    s->phase = DMX_TX_READY;
    return;
  }
  /* Four nominal frames allow scheduling slack, not a new wire-frame duration. */
  if ((s->phase == DMX_TX_FEEDING || s->phase == DMX_TX_DRAINING) &&
      now - s->started_us >= (int64_t)s->frame_us * 4)
    dmx_tx_fail(s, DMX_TX_TIMEOUT, now);
  if (s->phase == DMX_TX_RETIRING) {
    if (idle) s->phase = DMX_TX_READY;
    else if (now - s->retired_us >= (int64_t)s->frame_us * 2)
      s->phase = DMX_TX_FAULT_STOPPED;
  }
}
#endif
