#include "command_dispatcher.h"
#include "../support/fake_preset_storage.h"

#include <initializer_list>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

using Status = CommandStatus;
using Type = CommandType;
using Command = ControllerCommand;
using SlotState = DmxPresetStorage::SlotState;

static Command set(int channel, int value) { return Command::channelCommand(Type::set_channel, channel, value); }
static Command adjust(int channel, int delta) { return Command::channelCommand(Type::adjust_channel, channel, delta); }
static Command preset(Type type, int slot, int target = 0) { return Command::presetCommand(type, slot, target); }

struct Harness {
    FakeStorage storage;
    DmxController controller{storage};
    CommandDispatcher dispatcher{controller};
    struct Completion {
        Command command;
        CommandDispatcher::Result result;
        int value37;
    } completed[256];
    struct Frame { int value21; int value37; bool blackout; } frames[256];
    unsigned completion_count = 0;
    unsigned frame_count = 0;
    unsigned changes = 0;
    int selected_channel = 21;
    int selected_value = 0;
    int publish_error = 0;
    bool probe_reentrancy = false;
    bool check_patch_snapshot = false;

    Harness() {
        dispatcher.setSenderActive(true);
        dispatcher.setPublisher(publish, this);
        dispatcher.setCompletion(complete, this);
        controller.setObserver(change, this);
    }
    static void change(void* context, const DmxController::Change&) {
        auto& h = *static_cast<Harness*>(context);
        h.selected_value = h.controller.channel(h.selected_channel);
        ++h.changes;
        if (h.check_patch_snapshot) {
            CHECK(h.controller.channel(1) == 100);
            CHECK(h.controller.channel(512) == 255);
        }
    }
    static int publish(void* context, const Command&) {
        auto& h = *static_cast<Harness*>(context);
        CHECK(h.controller.workingPacket()[0] == 0);
        CHECK(h.controller.outputPacket()[0] == 0);
        if (h.controller.blackoutActive()) {
            for (size_t i = 0; i < DmxController::packet_size; ++i) CHECK(h.controller.outputPacket()[i] == 0);
        }
        CHECK(h.frame_count < 256);
        h.frames[h.frame_count++] = {h.controller.outputPacket()[21], h.controller.outputPacket()[37], h.controller.blackoutActive()};
        return h.publish_error;
    }
    static void complete(void* context, const Command& command, const CommandDispatcher::Result& result) {
        auto& h = *static_cast<Harness*>(context);
        CHECK(h.completion_count < 256);
        auto& completed = h.completed[h.completion_count++];
        completed.command = command;
        completed.result = result;
        completed.value37 = h.controller.channel(37);
        CHECK(result.ticket != 0);
        CHECK(result.revision_after >= result.revision_before);
        if (h.probe_reentrancy) {
            CHECK(h.dispatcher.submit(set(21, 99)).status == Status::busy);
            CHECK(h.dispatcher.executeLocal(set(21, 99)).status == Status::busy);
            CHECK(h.dispatcher.process() == 0);
        }
    }
    CommandDispatcher::Result execute(const Command& command) { return dispatcher.executeLocal(command); }
};

static void validation() {
    Harness h;
    const auto invalid = [&h](const Command& command) {
        CHECK(h.dispatcher.submit(command).status == Status::invalid);
        CHECK(h.execute(command).status == Status::invalid);
        CHECK(h.controller.revision() == 0 && h.dispatcher.pending() == 0);
        CHECK(h.completion_count == 0 && h.frame_count == 0 && h.storage.saves == 0);
    };
    invalid(Command{});
    auto unknown = set(1, 1);
    unknown.type = static_cast<Type>(255);
    invalid(unknown);
    unknown = set(1, 1);
    unknown.source = static_cast<CommandSource>(255);
    invalid(unknown);
    for (int channel : {INT_MIN, -1, 0, 513, INT_MAX}) {
        invalid(set(channel, 1));
        invalid(adjust(channel, 1));
    }
    for (int value : {-1, 256, INT_MIN, INT_MAX}) invalid(set(1, value));
    for (int slot : {-1, 8, 255, 256, INT_MAX}) {
        for (auto type : {Type::load_preset, Type::save_preset, Type::clear_preset}) invalid(preset(type, slot));
        invalid(preset(Type::copy_preset, slot, 0));
        invalid(preset(Type::copy_preset, 0, slot));
    }
    auto blackout = Command::blackoutCommand(true);
    blackout.payload.blackout = 2;
    invalid(blackout);
    auto reset = Command::resetSenderCommand();
    reset.source = CommandSource::touch;
    invalid(reset);
    auto patch = Command{};
    patch.type = Type::set_channels;
    invalid(patch);
    patch.payload.patch.count = 9;
    invalid(patch);
    patch.payload.patch.count = 2;
    patch.payload.patch.values[0] = {1, 100};
    patch.payload.patch.values[1] = {512, 256};
    invalid(patch);
    patch.payload.patch.values[1] = {1, 1};
    invalid(patch);
    CHECK(h.controller.channel(1) == 0);
    puts("PASS command validation and atomic rejection");
}

static void fifoAndRelativeValues() {
    Harness h;
    Command commands[] = {set(37, 100), adjust(37, 5), adjust(37, 5)};
    uint32_t tickets[3];
    for (unsigned i = 0; i < 3; ++i) {
        commands[i].request_id = i + 71;
        commands[i].source = CommandSource::external;
        const auto admission = h.dispatcher.submit(commands[i]);
        CHECK(admission.status == Status::accepted);
        tickets[i] = admission.ticket;
        commands[i].payload.channel.value = 0; // FIFO owns an independent copy.
    }
    CHECK(h.controller.channel(37) == 0);
    CHECK(h.dispatcher.process() == 3 && h.dispatcher.pending() == 0);
    CHECK(h.controller.channel(37) == 110);
    CHECK(h.completed[0].value37 == 100 && h.completed[1].value37 == 105 && h.completed[2].value37 == 110);
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(h.completed[i].command.request_id == 71 + i);
        CHECK(h.completed[i].command.source == CommandSource::external);
        CHECK(h.completed[i].result.ticket == tickets[i]);
        CHECK(h.completed[i].result.status == Status::applied);
    }
    CHECK(h.execute(set(1, 255)).status == Status::applied);
    CHECK(h.execute(set(512, 255)).status == Status::applied);
    CHECK(h.execute(adjust(1, INT_MIN)).status == Status::applied && h.controller.channel(1) == 0);
    CHECK(h.execute(adjust(512, INT_MAX)).status == Status::no_change);
    const auto revision = h.controller.revision();
    const auto frames = h.frame_count;
    CHECK(h.execute(set(1, 0)).status == Status::no_change);
    CHECK(h.controller.revision() == revision && h.frame_count == frames);
    CHECK(h.execute(set(21, 83)).status == Status::applied && h.selected_value == 83);
    // A stale display cache is not an absolute write source for relative inputs.
    h.selected_value = 2;
    CHECK(h.execute(adjust(21, 5)).status == Status::applied);
    CHECK(h.controller.channel(21) == 88 && h.selected_value == 88);
    puts("PASS FIFO, owned payloads, current-value adjustments, boundaries, no-op revisions, selection");
}

static void overflowAndLocalResponsiveness() {
    Harness h;
    for (unsigned i = 1; i <= 32; ++i) CHECK(h.dispatcher.submit(set(37, i)).status == Status::accepted);
    CHECK(h.dispatcher.submit(set(37, 255)).status == Status::busy);
    CHECK(h.dispatcher.pending() == 32 && h.controller.channel(37) == 0);
    CHECK(h.dispatcher.process(7) == 7);
    for (unsigned i = 33; i <= 35; ++i) CHECK(h.dispatcher.submit(set(37, i)).status == Status::accepted);
    CHECK(h.dispatcher.process(1000) == 28); // Budget is capped at capacity.
    CHECK(h.controller.channel(37) == 35 && h.completion_count == 35);
    for (unsigned i = 0; i < 35; ++i) CHECK(h.completed[i].value37 == static_cast<int>(i + 1));
    Harness slider;
    for (int value = 0; value < 32; ++value) CHECK(slider.dispatcher.submit(set(21, value)).status == Status::accepted);
    CHECK(slider.execute(set(21, 255)).status == Status::applied);
    CHECK(slider.dispatcher.pending() == 0 && slider.selected_value == 255);
    CHECK(slider.completion_count == 33 && slider.completed[32].command.payload.channel.value == 255);
    CHECK(slider.execute(set(21, 0)).status == Status::applied && slider.selected_value == 0);
    // No coalescing exists: every accepted slider sample, including release, completes.
    for (unsigned i = 0; i < 32; ++i) CHECK(slider.completed[i].command.payload.channel.value == static_cast<int>(i));
    puts("PASS overflow, wraparound, recovery, bounded turns, same-turn final slider value");
}

static void patches() {
    Harness h;
    Command patch;
    patch.type = Type::set_channels;
    patch.payload.patch.count = 2;
    patch.payload.patch.values[0] = {1, 100};
    patch.payload.patch.values[1] = {512, 255};
    h.check_patch_snapshot = true;
    CHECK(h.execute(patch).status == Status::applied);
    CHECK(h.changes == 1 && h.controller.revision() == 1 && h.frame_count == 1);
    CHECK(h.execute(patch).status == Status::no_change && h.changes == 1 && h.frame_count == 1);
    patch.payload.patch.values[0] = {1, 50};
    patch.payload.patch.values[1] = {0, 255};
    CHECK(h.execute(patch).status == Status::invalid);
    CHECK(h.controller.channel(1) == 100 && h.controller.revision() == 1);
    h.check_patch_snapshot = false;
    patch.payload.patch.count = 8;
    for (int i = 0; i < 8; ++i) patch.payload.patch.values[i] = {i + 1, i};
    CHECK(h.execute(patch).status == Status::applied && h.controller.channel(8) == 7);
    CHECK(h.controller.workingPacket()[0] == 0);
    puts("PASS bounded patches, one complete-state notification, no partial mutation");
}

static void blackoutAndPresets() {
    Harness h;
    CHECK(h.execute(set(21, 10)).status == Status::applied);
    CHECK(h.execute(preset(Type::save_preset, 0)).status == Status::applied);
    const auto revision = h.controller.revision();
    const auto saves = h.storage.saves;
    CHECK(h.execute(preset(Type::save_preset, 0)).status == Status::no_change);
    CHECK(h.controller.revision() == revision && h.storage.saves == saves);
    CHECK(h.dispatcher.submit(Command::blackoutCommand(true)).status == Status::accepted);
    CHECK(h.dispatcher.submit(set(21, 99)).status == Status::accepted);
    CHECK(h.dispatcher.submit(Command::blackoutCommand(true)).status == Status::accepted);
    CHECK(h.dispatcher.submit(preset(Type::save_preset, 1)).status == Status::accepted);
    CHECK(h.dispatcher.submit(preset(Type::load_preset, 0)).status == Status::accepted);
    CHECK(h.dispatcher.submit(preset(Type::load_preset, 1)).status == Status::accepted);
    CHECK(h.dispatcher.submit(Command::blackoutCommand(false)).status == Status::accepted);
    CHECK(h.dispatcher.submit(Command::blackoutCommand(false)).status == Status::accepted);
    CHECK(h.dispatcher.process(32) == 8);
    CHECK(h.controller.channel(21) == 10 && h.selected_value == 10 && !h.controller.blackoutActive());
    CHECK(h.storage.packets[1][21] == 99);
    CHECK(h.completed[5].result.status == Status::no_change); // Repeated ON after edit.
    CHECK(h.completed[10].result.status == Status::no_change); // Repeated OFF.
    CHECK(h.frame_count == 6);
    for (unsigned i = 1; i < 5; ++i) CHECK(h.frames[i].blackout && h.frames[i].value21 == 0);
    CHECK(h.frames[5].value21 == 10 && !h.frames[5].blackout);
    CHECK(h.execute(preset(Type::copy_preset, 1, 7)).status == Status::applied);
    CHECK(h.execute(preset(Type::copy_preset, 1, 7)).status == Status::rejected);
    CHECK(h.execute(preset(Type::clear_preset, 7)).status == Status::applied);
    CHECK(h.storage.erases == 1 && h.storage.states[7] == SlotState::missing);
    CHECK(h.execute(preset(Type::load_preset, 7)).status == Status::rejected);
    CHECK(h.execute(preset(Type::copy_preset, 1, 7)).status == Status::applied && h.storage.states[7] == SlotState::valid);
    CHECK(h.execute(preset(Type::load_preset, 7)).status == Status::applied && h.selected_value == 99);
    const auto frames = h.frame_count;
    CHECK(h.execute(preset(Type::load_preset, 7)).status == Status::no_change);
    CHECK(h.frame_count == frames + 1); // Keep the tested publication on successful same-value LOAD.
    puts("PASS blackout ordering/idempotence/restore, preset operations, occupancy, persistence no-ops");
}

static void failuresModesAndPages() {
    Harness h;
    h.storage.fail_save = true;
    CHECK(h.execute(preset(Type::save_preset, 0)).status == Status::storage_error);
    CHECK(h.controller.revision() == 0 && h.completion_count == 1);
    h.storage.fail_save = false;
    CHECK(h.execute(preset(Type::save_preset, 0)).status == Status::applied);
    h.storage.fail_load = true;
    const auto revision = h.controller.revision();
    CHECK(h.execute(preset(Type::load_preset, 0)).status == Status::storage_error);
    CHECK(h.controller.revision() == revision && h.frame_count == 0);
    h.storage.fail_load = false;
    h.storage.fail_erase = true;
    const auto erased = h.execute(preset(Type::clear_preset, 0));
    CHECK(erased.status == Status::storage_error && erased.domain == DmxController::Result::write_failed);
    CHECK(h.controller.presetExists(0) && h.storage.states[0] == SlotState::valid && h.storage.erases == 0);
    CHECK(h.controller.revision() == revision && h.frame_count == 0);
    h.storage.fail_erase = false;
    h.storage.states[1] = SlotState::corrupt;
    CHECK(h.execute(preset(Type::load_preset, 1)).status == Status::storage_error);
    h.storage.states[1] = SlotState::error;
    CHECK(h.execute(preset(Type::load_preset, 1)).status == Status::storage_error);
    h.publish_error = -123;
    const auto transport = h.execute(set(21, 50));
    CHECK(transport.status == Status::transport_error && transport.transport_result == -123);
    CHECK(h.controller.channel(21) == 50); // Match existing no-rollback publication failure semantics.
    h.publish_error = 0;
    CHECK(h.dispatcher.submit(set(21, 51)).status == Status::accepted);
    h.dispatcher.setSenderActive(false);
    CHECK(h.execute(set(21, 90)).status == Status::unsupported_mode);
    CHECK(h.dispatcher.submit(set(21, 90)).status == Status::unsupported_mode);
    CHECK(h.dispatcher.process() == 1 && h.controller.channel(21) == 50);
    CHECK(h.completed[h.completion_count - 1].result.status == Status::unsupported_mode);
    h.dispatcher.setSenderActive(true);
    h.probe_reentrancy = true;
    CHECK(h.execute(set(21, 52)).status == Status::applied && h.selected_value == 52);
    h.probe_reentrancy = false;
    // Dispatcher has no page dependency. Actual src.cpp servicing placement is
    // separately checked by check_integration.py; real touch/rendering is a hardware gate.
    for (const char* page : {"main", "settings", "precision", "keypad", "presets", "asleep", "wake-consumed"}) {
        (void)page;
        const int value = h.controller.channel(21) + 1;
        CHECK(h.dispatcher.submit(set(21, value)).status == Status::accepted);
        CHECK(h.dispatcher.process() == 1 && h.selected_value == value);
    }
    CHECK(h.execute(Command::resetSenderCommand()).status == Status::applied);
    CHECK(h.selected_value == 0 && h.controller.workingPacket()[0] == 0);
    CHECK(h.execute(Command::resetSenderCommand()).status == Status::no_change);
    Harness inspection;
    inspection.storage.fail_inspect = true;
    CHECK(inspection.execute(preset(Type::save_preset, 0)).status == Status::storage_error);
    CHECK(inspection.storage.saves == 0 && inspection.storage.erases == 0);
    inspection.storage.fail_inspect = false;
    CHECK(inspection.execute(preset(Type::save_preset, 0)).status == Status::applied);
    puts("PASS storage/transport failures, mode changes, reentrancy, page-independent service, reset");
}

int main() {
    static_assert(std::is_trivially_copyable<Command>::value, "Queue payload must remain an owned value");
    validation();
    fifoAndRelativeValues();
    overflowAndLocalResponsiveness();
    patches();
    blackoutAndPresets();
    failuresModesAndPages();
    printf("Host sizes: command=%zu queue=%zu dispatcher=%zu bytes\n", sizeof(Command),
           CommandDispatcher::queueStorageBytes(), sizeof(CommandDispatcher));
    puts("All Dispatcher host tests passed.");
}
