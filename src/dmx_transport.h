#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../patches/esp_dmx_v202/tx_core.h"

// Single application-task owner. No method may be called from a Wi-Fi callback.
class DmxTransport {
 public:
    static constexpr size_t packet_size = 513;
    static constexpr size_t barrier_capacity = 4;
    static constexpr uint32_t nominal_frame_us = 513 * 44 + 176 + 12;
    static constexpr unsigned max_failed_attempts = 3;
    struct Status { dmx_tx_state_t tx{}; bool write_mode = false; bool can_start = false; };
    class Driver {
     public:
        virtual ~Driver() = default;
        virtual bool poll(Status& status) = 0;
        virtual bool setSender(bool sender) = 0;
        virtual bool write(const uint8_t* packet) = 0;
        virtual bool start(uint32_t& attempt) = 0;
        virtual bool cancel() = 0;
    };
    struct Health {
        uint64_t requested = 0, published = 0, accepted = 0, started = 0, completed = 0;
        uint32_t errors = 0, cancelled_barriers = 0;
        dmx_tx_result_t last_error = DMX_TX_PENDING;
        dmx_tx_phase_t driver_phase = DMX_TX_READY;
        unsigned consecutive_failures = 0;
        bool fault_stopped = false;
    };
    explicit DmxTransport(Driver& driver) : driver_(driver) {}
    bool canStage(bool barrier) const;
    bool stage(const uint8_t* packet, size_t size, bool barrier = false);
    void requestSender(bool sender);
    void poll(int64_t now_us);           // maintenance only; never starts output
    bool serviceTx(int64_t now_us);      // at most one start at an existing opportunity
    bool receiverReady() const { return !want_sender_ && !inflight_ && !status_.write_mode && !health_.fault_stopped; }
    const Health& health() const { return health_; }
    size_t pendingBarriers() const { return count_; }

 private:
    struct Barrier { uint8_t packet[packet_size]; uint64_t generation; };
    void failed(dmx_tx_result_t result, int64_t now_us);
    Driver& driver_;
    uint8_t latest_[packet_size]{};
    Barrier barriers_[barrier_capacity]{};
    size_t head_ = 0, count_ = 0;
    Health health_;
    Status status_;
    uint64_t active_generation_ = 0;
    uint32_t active_attempt_ = 0;
    int64_t retry_after_us_ = 0;
    bool want_sender_ = false, have_packet_ = false, inflight_ = false;
    bool active_barrier_ = false;
};
