#pragma once
#include "dmx_transport.h"
#include <dmx_tx_contract.h>
#if DMX_TX_CONTRACT_VERSION != 1
#error "The reviewed esp_dmx TX ownership patch is required"
#endif

class EspDmxTransportDriver final : public DmxTransport::Driver {
 public:
    explicit EspDmxTransportDriver(dmx_port_t port) : port_(port) {}
    bool poll(DmxTransport::Status& status) override {
        dmx_tx_status_t s;
        if (dmx_tx_poll(port_, &s) != ESP_OK) return false;
        status.tx = s.tx;
        status.write_mode = s.write_mode;
        status.can_start = s.can_start;
        return true;
    }
    bool setSender(bool sender) override { return dmx_set_mode(port_, sender ? DMX_MODE_WRITE : DMX_MODE_READ) == ESP_OK; }
    bool write(const uint8_t* packet) override { return dmx_write_packet(port_, packet, DmxTransport::packet_size) == ESP_OK; }
    bool start(uint32_t& attempt) override { return dmx_tx_start(port_, DmxTransport::packet_size, &attempt) == ESP_OK; }
    bool cancel() override { return dmx_tx_cancel(port_) == ESP_OK; }
 private:
    dmx_port_t port_;
};
