#pragma once

#include "dmx_controller.h"
#include <string.h>

struct FakeStorage : DmxPresetStorage {
    uint8_t packets[preset_count][packet_size] = {};
    SlotState states[preset_count];
    uint8_t mask = 0;
    bool fail_load = false;
    bool fail_save = false;
    bool fail_metadata_read = false;
    bool fail_metadata_write = false;
    unsigned saves = 0;
    unsigned metadata_writes = 0;

    FakeStorage() {
        for (auto& state : states) state = SlotState::missing;
    }
    SlotState inspect(uint8_t slot) override { return states[slot]; }
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
    bool loadClearedMask(uint8_t& value) override {
        if (fail_metadata_read) return false;
        value = mask;
        return true;
    }
    bool saveClearedMask(uint8_t value) override {
        if (fail_metadata_write) return false;
        mask = value;
        ++metadata_writes;
        return true;
    }
};
