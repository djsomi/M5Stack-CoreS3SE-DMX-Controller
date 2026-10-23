#include "dmx_controller.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <type_traits>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

using Result = DmxController::Result;
using SlotState = DmxPresetStorage::SlotState;

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

// Same binding as the sender view: selection stays UI-local; each notification
// refreshes its cached selected value from authoritative state.
struct Selection {
    DmxController& controller;
    int channel = 21;
    int value = 0;
    unsigned notifications = 0;
    uint64_t revision = 0;
    explicit Selection(DmxController& c) : controller(c) {
        controller.setObserver(onChange, this);
    }
    static void onChange(void* context, const DmxController::Change& change) {
        auto& self = *static_cast<Selection*>(context);
        CHECK(change.revision > self.revision);
        CHECK(change.revision == self.controller.revision());
        self.revision = change.revision;
        self.value = self.controller.channel(self.channel);
        ++self.notifications;
    }
};

static void checkStartCode(const DmxController& controller) {
    CHECK(controller.workingPacket()[0] == 0);
    CHECK(controller.outputPacket()[0] == 0);
}

static void boundsAndRevision() {
    FakeStorage storage;
    DmxController controller(storage);
    Selection selected(controller);
    static_assert(std::is_same<decltype(controller.workingPacket()), const uint8_t*>::value,
                  "External code must not mutate the universe directly");
    CHECK(controller.setChannel(1, 255) == Result::ok);
    CHECK(controller.setChannel(512, 255) == Result::ok);
    const auto revision = controller.revision();
    const auto notifications = selected.notifications;
    CHECK(controller.setChannel(1, 255) == Result::ok);
    CHECK(controller.revision() == revision);
    CHECK(selected.notifications == notifications);
    for (int channel : {-1, 0, 513, INT_MAX}) {
        CHECK(controller.setChannel(channel, 100) == Result::invalid_channel);
        CHECK(controller.adjustChannel(channel, 1) == Result::invalid_channel);
        CHECK(controller.channel(channel) == 0);
    }
    for (int value : {INT_MIN, -1, 256, INT_MAX}) {
        CHECK(controller.setChannel(1, value) == Result::invalid_value);
    }
    CHECK(controller.revision() == revision);
    CHECK(controller.channel(1) == 255);
    CHECK(controller.adjustChannel(1, INT_MIN) == Result::ok);
    CHECK(controller.channel(1) == 0);
    CHECK(controller.adjustChannel(1, INT_MAX) == Result::ok);
    CHECK(controller.channel(1) == 255);
    CHECK(controller.setChannel(21, 82) == Result::ok);
    CHECK(selected.value == 82);
    CHECK(controller.setChannel(22, 41) == Result::ok);
    CHECK(selected.channel == 21 && selected.value == 82);
    checkStartCode(controller);
    controller.reset();
    for (int ch = 1; ch <= 512; ++ch) CHECK(controller.channel(ch) == 0);
    CHECK(selected.value == 0 && !controller.blackoutActive());
    checkStartCode(controller);
    puts("PASS channel/value bounds, start code, revisions, selected-channel binding");
}

static void everySlotAndValue() {
    FakeStorage storage;
    DmxController controller(storage);
    for (int value = 0; value <= 255; ++value) {
        CHECK(controller.setChannel(512, value) == Result::ok);
        CHECK(controller.channel(512) == value);
        checkStartCode(controller);
    }
    for (uint8_t slot = 0; slot < DmxController::preset_count; ++slot) {
        CHECK(controller.setChannel(21, slot + 1) == Result::ok);
        CHECK(controller.savePreset(slot) == Result::ok);
        CHECK(controller.clearPreset(slot) == Result::ok);
        CHECK(!controller.presetExists(slot));
    }
    CHECK(storage.mask == 0xff);
    for (uint8_t slot = 0; slot < DmxController::preset_count; ++slot) {
        CHECK(controller.savePreset(slot) == Result::ok);
        CHECK(controller.presetExists(slot));
        CHECK(controller.loadPreset(slot) == Result::ok);
        checkStartCode(controller);
    }
    CHECK(storage.mask == 0);
    puts("PASS all 256 values and all eight preset/mask positions");
}

static void blackoutRestore() {
    FakeStorage storage;
    DmxController controller(storage);
    Selection selected(controller);
    uint8_t entry[DmxController::packet_size] = {};
    for (int ch = 1; ch <= 512; ++ch) {
        entry[ch] = static_cast<uint8_t>(ch % 256);
        CHECK(controller.setChannel(ch, entry[ch]) == Result::ok);
    }
    controller.setBlackout(true);
    CHECK(controller.blackoutActive());
    CHECK(controller.setChannel(21, 200) == Result::ok);
    CHECK(selected.value == 200);
    const auto revision = controller.revision();
    controller.setBlackout(true);
    CHECK(controller.revision() == revision);
    CHECK(controller.savePreset(0) == Result::ok);
    CHECK(storage.packets[0][21] == 200); // Save working values, not zero output.
    storage.states[1] = SlotState::valid;
    memset(storage.packets[1], 77, DmxController::packet_size);
    CHECK(controller.loadPreset(1) == Result::ok);
    CHECK(selected.value == 77 && controller.blackoutActive());
    for (size_t i = 0; i < DmxController::packet_size; ++i) {
        CHECK(controller.outputPacket()[i] == 0);
    }
    checkStartCode(controller);
    controller.setBlackout(false);
    CHECK(!controller.blackoutActive());
    CHECK(memcmp(controller.workingPacket(), entry, sizeof(entry)) == 0);
    CHECK(memcmp(controller.outputPacket(), entry, sizeof(entry)) == 0);
    CHECK(selected.value == entry[21]);
    const auto restored_revision = controller.revision();
    controller.setBlackout(false);
    CHECK(controller.revision() == restored_revision);
    puts("PASS blackout entry snapshot, zero output, discard edits/loads on release");
}

static void presetsAndOccupancy() {
    FakeStorage storage;
    DmxController controller(storage);
    Selection selected(controller);
    CHECK(!controller.presetExists(0));
    CHECK(controller.loadPreset(0) == Result::empty);
    CHECK(controller.clearPreset(0) == Result::empty);
    CHECK(controller.setChannel(21, 123) == Result::ok);
    CHECK(controller.savePreset(0) == Result::ok);
    CHECK(controller.presetExists(0));
    CHECK(storage.packets[0][0] == 0 && storage.packets[0][21] == 123);
    CHECK(controller.copyPreset(0, 0) == Result::same_slot);
    CHECK(controller.copyPreset(0, 7) == Result::ok);
    CHECK(controller.copyPreset(0, 7) == Result::occupied);
    CHECK(controller.channel(21) == 123);
    CHECK(controller.clearPreset(7) == Result::ok);
    CHECK(storage.states[7] == SlotState::valid); // CLEAR retains the legacy blob.
    CHECK(storage.mask == 0x80 && !controller.presetExists(7));
    CHECK(controller.loadPreset(7) == Result::empty);
    {
        DmxController rebooted(storage);
        CHECK(!rebooted.presetExists(7)); // Logical occupancy survives reboot.
        CHECK(rebooted.copyPreset(0, 7) == Result::ok);
        CHECK(rebooted.presetExists(7) && storage.mask == 0);
    }
    // Use a fresh instance after simulated reboot; there are no external writers.
    DmxController rebooted(storage);
    Selection rebooted_selection(rebooted);
    CHECK(rebooted.loadPreset(7) == Result::ok);
    CHECK(rebooted_selection.value == 123);
    CHECK(rebooted.clearPreset(0) == Result::ok);
    CHECK(rebooted.setChannel(21, 66) == Result::ok);
    CHECK(rebooted.savePreset(0) == Result::ok);
    CHECK(storage.mask == 0 && rebooted.presetExists(0));
    storage.packets[0][0] = 99; // Old data must never introduce a nonzero start code.
    CHECK(rebooted.loadPreset(0) == Result::ok);
    checkStartCode(rebooted);
    CHECK(rebooted.copyPreset(0, 1) == Result::ok);
    CHECK(storage.packets[1][0] == 0);
    CHECK(rebooted.channel(21) == 66 && rebooted_selection.value == 66);
    puts("PASS preset save/load/copy/clear, legacy start code, persisted occupancy");
}

static void presetFailures() {
    FakeStorage storage;
    DmxController controller(storage);
    CHECK(controller.setChannel(21, 12) == Result::ok);
    for (uint8_t slot : {uint8_t(8), uint8_t(255)}) {
        CHECK(controller.savePreset(slot) == Result::invalid_slot);
        CHECK(controller.loadPreset(slot) == Result::invalid_slot);
        CHECK(controller.clearPreset(slot) == Result::invalid_slot);
        CHECK(controller.copyPreset(slot, 0) == Result::invalid_slot);
        CHECK(controller.copyPreset(0, slot) == Result::invalid_slot);
    }
    const auto revision = controller.revision();
    storage.states[0] = SlotState::corrupt;
    CHECK(controller.loadPreset(0) == Result::corrupt);
    CHECK(controller.clearPreset(0) == Result::corrupt);
    storage.states[0] = SlotState::error;
    CHECK(controller.loadPreset(0) == Result::read_failed);
    storage.states[0] = SlotState::valid;
    storage.fail_load = true;
    CHECK(controller.loadPreset(0) == Result::read_failed);
    CHECK(controller.copyPreset(0, 1) == Result::read_failed);
    storage.fail_load = false;
    storage.states[1] = SlotState::corrupt;
    CHECK(controller.copyPreset(0, 1) == Result::corrupt);
    storage.states[1] = SlotState::error;
    CHECK(controller.copyPreset(0, 1) == Result::read_failed);
    storage.states[1] = SlotState::missing;
    storage.fail_save = true;
    CHECK(controller.savePreset(1) == Result::write_failed);
    CHECK(controller.copyPreset(0, 1) == Result::write_failed);
    storage.fail_save = false;
    storage.fail_metadata_write = true;
    CHECK(controller.clearPreset(0) == Result::metadata_failed);
    CHECK(storage.mask == 0 && controller.presetExists(0));
    CHECK(controller.revision() == revision && controller.channel(21) == 12);
    checkStartCode(controller);
    {
        FakeStorage failed;
        failed.fail_metadata_read = true;
        DmxController other(failed);
        CHECK(other.presetState(0) == SlotState::error);
        CHECK(other.savePreset(0) == Result::metadata_failed);
        CHECK(failed.saves == 0);
        failed.fail_metadata_read = false;
        CHECK(other.savePreset(0) == Result::ok); // Read errors are not cached as empty.
        CHECK(other.clearPreset(0) == Result::ok);
        failed.fail_metadata_write = true;
        CHECK(other.savePreset(0) == Result::metadata_failed);
        CHECK(!other.presetExists(0)); // Failed reactivation must not change RAM occupancy.
    }
    puts("PASS invalid slots, corrupt/read/write/metadata errors, retry semantics");
}

int main() {
    boundsAndRevision();
    everySlotAndValue();
    blackoutRestore();
    presetsAndOccupancy();
    presetFailures();
    puts("All Controller host tests passed.");
}
