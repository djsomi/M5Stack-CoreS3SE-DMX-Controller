#include "preset_double_tap.h"
#include "dmx_controller.h"
#include "../support/fake_preset_storage.h"

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

static void atomicPatchesAndNoOps() {
    FakeStorage storage;
    DmxController controller(storage);
    Selection selected(controller);
    CHECK(controller.setChannels(nullptr, 1) == Result::invalid_patch);
    DmxChannelValue values[] = {{1, 100}, {512, 256}};
    CHECK(controller.setChannels(values, 0) == Result::invalid_patch);
    CHECK(controller.setChannels(values, 9) == Result::invalid_patch);
    CHECK(controller.setChannels(values, 2) == Result::invalid_value);
    CHECK(controller.channel(1) == 0 && controller.revision() == 0);
    values[1] = {0, 255};
    CHECK(controller.setChannels(values, 2) == Result::invalid_channel);
    values[1] = {1, 255};
    CHECK(controller.setChannels(values, 2) == Result::invalid_patch);
    values[1] = {512, 255};
    CHECK(controller.setChannels(values, 2) == Result::ok);
    CHECK(controller.channel(1) == 100 && controller.channel(512) == 255);
    CHECK(controller.revision() == 1 && selected.notifications == 1);
    CHECK(controller.setChannels(values, 2) == Result::ok && controller.revision() == 1);
    CHECK(controller.savePreset(0) == Result::ok);
    const auto revision = controller.revision();
    const auto saves = storage.saves;
    CHECK(controller.savePreset(0) == Result::ok);
    CHECK(controller.revision() == revision && storage.saves == saves);
    controller.reset();
    const auto reset_revision = controller.revision();
    controller.reset();
    CHECK(controller.revision() == reset_revision);
    checkStartCode(controller);
    puts("PASS Controller atomic patch validation and unchanged save/reset revisions");
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


void loadedProvenance() {
    FakeStorage storage; DmxController c(storage);
    CHECK(c.loadedPreset() == 0 && !c.loadedPresetModified());
    c.setChannel(1, 12); c.savePreset(2); c.savePreset(4);
    CHECK(c.loadedPreset() == 0); // Saving is never loading.
    auto revision = c.revision(); c.loadPreset(2);
    CHECK(c.loadedPreset() == 3 && !c.loadedPresetModified() && c.revision() == revision + 1);
    revision = c.revision(); c.loadPreset(2); c.setChannel(1, 12);
    CHECK(c.revision() == revision && !c.loadedPresetModified());
    c.setChannel(1, 13); c.setChannel(1, 12);
    CHECK(c.loadedPreset() == 3 && c.loadedPresetModified()); // Conservative history, not matching.
    c.loadPreset(4); CHECK(c.loadedPreset() == 5 && !c.loadedPresetModified());
    c.setBlackout(true); c.loadPreset(2); c.setChannel(1, 99);
    CHECK(c.loadedPreset() == 3 && c.loadedPresetModified() && c.outputPacket()[1] == 0);
    c.setBlackout(false); CHECK(c.channel(1) == 12 && c.loadedPreset() == 5 && !c.loadedPresetModified());
    c.setBlackout(true); c.loadPreset(2); c.clearPreset(4); c.setBlackout(false);
    CHECK(c.loadedPreset() == 0 && c.channel(1) == 12); // Deleted restore identity cannot resurrect.
    c.loadPreset(2); c.setBlackout(true); c.setChannel(1, 80); c.savePreset(2);
    CHECK(c.loadedPreset() == 0); c.setBlackout(false); CHECK(c.loadedPreset() == 0 && c.channel(1) == 12);
    c.loadPreset(2); CHECK(c.channel(1) == 80 && c.loadedPreset() == 3);
    c.savePreset(2); CHECK(c.loadedPreset() == 3 && !c.loadedPresetModified()); // Identical save is a no-op.
    c.setChannel(1, 81); storage.fail_save = true; c.savePreset(2);
    CHECK(c.loadedPreset() == 0 && c.channel(1) == 81); // Failed partial overwrite cannot claim provenance.
    storage.fail_save = false; c.loadPreset(2); storage.fail_load = true;
    CHECK(c.loadPreset(4) != Result::ok && c.loadedPreset() == 3);
    storage.fail_load = false;
    c.savePreset(6); CHECK(c.loadedPreset() == 3); // Saving another slot does not claim or clear a load.
    storage.fail_metadata_write = true;
    CHECK(c.clearPreset(2) == Result::metadata_failed && c.loadedPreset() == 0);
    storage.fail_metadata_write = false;
    c.loadPreset(2); c.setBlackout(true); c.setChannel(1,81); c.setBlackout(false);
    CHECK(c.loadedPreset() == 3 && !c.loadedPresetModified());
    DmxChannelValue patch[]={{1,82},{512,1}}; c.setChannels(patch,2);
    CHECK(c.loadedPreset() == 3 && c.loadedPresetModified());
    CHECK(c.setChannel(513,1) == Result::invalid_channel && c.loadedPreset() == 3);
    c.reset(); CHECK(c.loadedPreset() == 0 && !c.loadedPresetModified());
    DmxController reboot(storage); CHECK(reboot.loadedPreset() == 0);
    puts("PASS transient loaded/modified provenance, same-value loads, save/delete/failure, blackout restore and reboot");
}
void presetGestures() {
    PresetDoubleTap g;
    auto tap = [&](int slot, uint32_t t, int x=20, int y=20) {
        CHECK(!g.sample(slot, true, true, false, x, y, t));
        return g.sample(slot, false, false, true, x, y, t+30);
    };
    CHECK(!tap(2,100)); CHECK(tap(2,220)); CHECK(!tap(2,320)); // Triple cannot load twice.
    CHECK(!tap(4,450)); CHECK(!tap(2,550)); CHECK(!tap(2,1000));
    CHECK(!g.sample(2,true,true,false,20,20,1100));
    CHECK(!g.sample(2,false,true,false,40,20,1120));
    CHECK(!g.sample(2,false,false,true,20,20,1130)); CHECK(!tap(2,1200));
    CHECK(!g.sample(-1,true,true,false,20,180,1300)); CHECK(!tap(2,1400));
    CHECK(!g.sample(2,true,true,false,20,20,1500));
    CHECK(!g.sample(2,false,false,true,20,20,2301)); CHECK(!tap(2,2400));
    g.reset(); CHECK(!tap(0,UINT32_MAX-70)); CHECK(tap(0,20)); // Clock wrap.
    g.reset(); CHECK(!tap(7,100)); CHECK(!tap(7,200,40)); CHECK(tap(7,300,40));
    puts("PASS physical release-qualified preset pair, movement/hold/cancel/other-control/reset/triple/wrap guards");
}

int main() {
    loadedProvenance();
    presetGestures();
    boundsAndRevision();
    everySlotAndValue();
    atomicPatchesAndNoOps();
    blackoutRestore();
    presetsAndOccupancy();
    presetFailures();
    puts("All Controller host tests passed.");
}
