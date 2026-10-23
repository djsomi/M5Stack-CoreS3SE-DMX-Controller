#pragma once

#include <stddef.h>
#include <stdint.h>

struct DmxChannelValue {
    int32_t channel;
    int32_t value;
};

constexpr size_t dmx_max_patch_channels = 8;

enum class CommandType : uint8_t {
    set_channel, adjust_channel, set_channels, set_blackout,
    load_preset, save_preset, copy_preset, clear_preset,
    reset_sender, invalid
};

enum class CommandSource : uint8_t { touch, external, system };

// Owned payloads only: no producer pointers or dynamic allocations in the FIFO.
struct ControllerCommand {
    static constexpr size_t max_patch_channels = dmx_max_patch_channels;
    CommandType type = CommandType::invalid;
    CommandSource source = CommandSource::touch;
    uint32_t request_id = 0;
    union Payload {
        DmxChannelValue channel;
        int32_t blackout;
        struct { int32_t slot; int32_t target; } preset;
        struct { uint8_t count; DmxChannelValue values[max_patch_channels]; } patch;
        Payload() : patch{} {}
    } payload;

    static ControllerCommand channelCommand(CommandType type, int channel, int value) {
        ControllerCommand c;
        c.type = type;
        c.payload.channel = {channel, value};
        return c;
    }
    static ControllerCommand presetCommand(CommandType type, int slot, int target = 0) {
        ControllerCommand c;
        c.type = type;
        c.payload.preset = {slot, target};
        return c;
    }
    static ControllerCommand blackoutCommand(bool active) {
        ControllerCommand c;
        c.type = CommandType::set_blackout;
        c.payload.blackout = active ? 1 : 0;
        return c;
    }
    static ControllerCommand resetSenderCommand() {
        ControllerCommand c;
        c.type = CommandType::reset_sender;
        c.source = CommandSource::system;
        return c;
    }
};

enum class CommandStatus : uint8_t {
    accepted, applied, no_change, invalid, busy, storage_error,
    unsupported_mode, rejected, transport_error
};
