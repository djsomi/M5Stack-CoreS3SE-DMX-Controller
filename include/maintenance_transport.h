#pragma once
#include "device_maintenance.h"
#include "../src/dmx_transport.h"

// Shutdown-only adapter over the unchanged Stage 3 transport. No driver writes,
// waits, buffer resets, new timing or Controller mutations.
class MaintenanceTransport {
 public:
    explicit MaintenanceTransport(DmxTransport& transport):transport_(transport){}
    void begin(){goal_=0;}
    DeviceMaintenance::Step zeroFrame(int64_t now) {
        if(transport_.health().fault_stopped)return DeviceMaintenance::Step::failed;
        if(goal_&&transport_.health().completed==goal_){goal_=0;return DeviceMaintenance::Step::ready;}
        if(!goal_&&!transport_.pendingBarriers()) {
            const uint8_t zero[DmxTransport::packet_size]{};
            if(!transport_.stage(zero,sizeof(zero),true))return DeviceMaintenance::Step::failed;
            goal_=transport_.health().requested;
        }
        transport_.serviceTx(now); // Old ordered barriers drain before the zero barrier.
        return transport_.health().fault_stopped?DeviceMaintenance::Step::failed:DeviceMaintenance::Step::pending;
    }
    DeviceMaintenance::Step quiesce(int64_t now) {
        transport_.requestSender(false);transport_.poll(now);
        if(transport_.health().fault_stopped)return DeviceMaintenance::Step::failed;
        return transport_.receiverReady()?DeviceMaintenance::Step::ready:DeviceMaintenance::Step::pending;
    }
 private:
    DmxTransport& transport_;uint64_t goal_=0;
};
