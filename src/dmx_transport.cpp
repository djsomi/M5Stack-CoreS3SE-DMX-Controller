#include "dmx_transport.h"
#include <string.h>

bool DmxTransport::canStage(bool barrier) const {
    return want_sender_ && (!barrier || count_ < barrier_capacity) &&
        (health_.requested != UINT64_MAX || (!inflight_ && !count_));
}

bool DmxTransport::stage(const uint8_t* packet, size_t size, bool barrier) {
    if (!packet || size != packet_size || packet[0] != 0 || !canStage(barrier)) return false;
    if (health_.requested == UINT64_MAX) {
        health_.requested = health_.published = health_.accepted = health_.started = health_.completed = 0;
    }
    ++health_.requested;
    memcpy(latest_, packet, packet_size);
    have_packet_ = true;
    if (barrier) {
        auto& entry = barriers_[(head_ + count_) % barrier_capacity];
        memcpy(entry.packet, packet, packet_size);
        entry.generation = health_.requested;
        ++count_;
    }
    return true;
}

void DmxTransport::requestSender(bool sender) {
    if (want_sender_ == sender) return;
    want_sender_ = sender;
    if (!sender) {
        // An explicit mode departure cancels obligations; never counts them complete.
        health_.cancelled_barriers += count_;
        count_ = head_ = 0;
        have_packet_ = false;
    }
}

void DmxTransport::failed(dmx_tx_result_t result, int64_t now) {
    ++health_.errors;
    health_.last_error = result;
    if (++health_.consecutive_failures >= max_failed_attempts) health_.fault_stopped = true;
    // One nominal frame before retry; three consecutive failures exhaust the budget.
    retry_after_us_ = now + nominal_frame_us;
}

void DmxTransport::poll(int64_t now) {
    if (!driver_.poll(status_)) {
        if (!health_.fault_stopped) failed(DMX_TX_START_FAILED, now);
        health_.fault_stopped = true;
        return;
    }
    health_.driver_phase = status_.tx.phase;
    if (inflight_ && status_.tx.attempt != active_attempt_) {
        if (!health_.fault_stopped) failed(DMX_TX_START_FAILED, now);
        health_.fault_stopped = true;
        return;
    }
    if (inflight_ && status_.tx.queued && status_.tx.first_fifo_us >= 0)
        health_.started = active_generation_;
    if (status_.tx.phase == DMX_TX_FAULT_STOPPED) {
        if (!health_.fault_stopped) failed(status_.tx.result, now);
        health_.fault_stopped = true;
        return;
    }
    if (inflight_ && status_.tx.attempt == active_attempt_ && status_.tx.phase == DMX_TX_READY) {
        inflight_ = false;
        if (status_.tx.result == DMX_TX_COMPLETE) {
            health_.completed = active_generation_;
            health_.consecutive_failures = 0;
            if (active_barrier_ && count_ && barriers_[head_].generation == active_generation_) {
                head_ = (head_ + 1) % barrier_capacity;
                --count_;
            }
        } else if (want_sender_) failed(status_.tx.result, now);
    }
    if (!want_sender_) {
        if (status_.tx.phase == DMX_TX_FEEDING || status_.tx.phase == DMX_TX_DRAINING) {
            if (!driver_.cancel()) {
                failed(DMX_TX_CANCELLED, now);
                health_.fault_stopped = true;
            }
        } else if (!inflight_ && status_.tx.phase == DMX_TX_READY && status_.write_mode) {
            if (!driver_.setSender(false)) {
                failed(DMX_TX_START_FAILED, now);
                health_.fault_stopped = true;
            }
            else status_.write_mode = false;
        }
    } else if (!health_.fault_stopped && !status_.write_mode && status_.tx.phase == DMX_TX_READY) {
        if (!driver_.setSender(true)) { failed(DMX_TX_START_FAILED, now); health_.fault_stopped = true; }
        // Re-poll on the next turn; do not manufacture driver availability here.
    }
}

bool DmxTransport::serviceTx(int64_t now) {
    poll(now);
    if (!want_sender_ || !have_packet_ || inflight_ || health_.fault_stopped ||
        !status_.can_start || now < retry_after_us_) return false;
    const uint8_t* packet = count_ ? barriers_[head_].packet : latest_;
    const uint64_t generation = count_ ? barriers_[head_].generation : health_.requested;
    uint32_t attempt = 0;
    if (!driver_.write(packet)) {
        failed(DMX_TX_START_FAILED, now);
        return false;
    }
    health_.published = generation;
    if (!driver_.start(attempt) || !attempt) {
        failed(DMX_TX_START_FAILED, now);
        return false;
    }
    inflight_ = true;
    active_barrier_ = count_ != 0;
    active_generation_ = generation;
    active_attempt_ = attempt;
    health_.accepted = generation;
    return true;
}
