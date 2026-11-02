#pragma once

#include "dmx_controller.h"
#include <string.h>

struct FakeStorage : DmxPresetStorage {
    uint8_t packets[preset_count][packet_size] = {};
    SlotState states[preset_count];
    bool fail_inspect = false;
    bool fail_load = false;
    bool fail_save = false;
    bool fail_erase = false;
    unsigned saves = 0;
    unsigned erases = 0;

    FakeStorage() {
        for (auto& state : states) state = SlotState::missing;
    }
    SlotState inspect(uint8_t slot) override {
        return fail_inspect ? SlotState::error : states[slot];
    }
    bool load(uint8_t slot, uint8_t* packet) override {
        if (fail_load || states[slot] != SlotState::valid) return false;
        memcpy(packet, packets[slot], packet_size);
        return true;
    }
    bool save(uint8_t slot, const uint8_t* packet) override {
        if (fail_save) return false;
        memcpy(packets[slot], packet, packet_size);
        states[slot] = SlotState::valid;
        ++saves;
        return true;
    }
    bool erase(uint8_t slot) override {
        if (fail_erase) return false;
        states[slot] = SlotState::missing;
        ++erases;
        return true;
    }
};
