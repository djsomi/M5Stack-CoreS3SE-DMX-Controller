#include "command_dispatcher.h"

namespace {
bool validChannel(int32_t channel) {
    return channel >= 1 && channel <= static_cast<int32_t>(DmxController::channel_count);
}
bool validValue(int32_t value) { return value >= 0 && value <= 255; }
bool validSlot(int32_t slot) { return slot >= 0 && slot < DmxController::preset_count; }
}

void CommandDispatcher::setPublisher(Publisher publisher, void* context) {
    publisher_ = publisher;
    publisher_context_ = context;
}

void CommandDispatcher::setCompletion(Completion completion, void* context) {
    completion_ = completion;
    completion_context_ = context;
}

CommandStatus CommandDispatcher::validate(const ControllerCommand& c) const {
    if (c.source != CommandSource::touch && c.source != CommandSource::external &&
        c.source != CommandSource::system) return CommandStatus::invalid;
    switch (c.type) {
        case CommandType::set_channel:
            if (!validChannel(c.payload.channel.channel) || !validValue(c.payload.channel.value))
                return CommandStatus::invalid;
            break;
        case CommandType::adjust_channel:
            if (!validChannel(c.payload.channel.channel)) return CommandStatus::invalid;
            break;
        case CommandType::set_channels:
            if (c.payload.patch.count == 0 ||
                c.payload.patch.count > ControllerCommand::max_patch_channels) return CommandStatus::invalid;
            for (size_t i = 0; i < c.payload.patch.count; ++i) {
                const auto& value = c.payload.patch.values[i];
                if (!validChannel(value.channel) || !validValue(value.value)) return CommandStatus::invalid;
                for (size_t j = 0; j < i; ++j) {
                    if (value.channel == c.payload.patch.values[j].channel) return CommandStatus::invalid;
                }
            }
            break;
        case CommandType::set_blackout:
            if (c.payload.blackout != 0 && c.payload.blackout != 1) return CommandStatus::invalid;
            break;
        case CommandType::load_preset:
        case CommandType::save_preset:
        case CommandType::clear_preset:
            if (!validSlot(c.payload.preset.slot)) return CommandStatus::invalid;
            break;
        case CommandType::copy_preset:
            if (!validSlot(c.payload.preset.slot) || !validSlot(c.payload.preset.target))
                return CommandStatus::invalid;
            break;
        case CommandType::reset_sender:
            if (c.source != CommandSource::system) return CommandStatus::invalid;
            break;
        default: return CommandStatus::invalid;
    }
    return sender_active_ ? CommandStatus::accepted : CommandStatus::unsupported_mode;
}

CommandDispatcher::Submission CommandDispatcher::submit(const ControllerCommand& command) {
    if (processing_) return {CommandStatus::busy, 0};
    const auto status = validate(command);
    if (status != CommandStatus::accepted) return {status, 0};
    if (count_ == capacity) return {CommandStatus::busy, 0};
    if (++next_ticket_ == 0) ++next_ticket_;
    auto& entry = queue_[(head_ + count_) % capacity];
    entry.command = command;
    entry.ticket = next_ticket_;
    ++count_;
    return {CommandStatus::accepted, next_ticket_};
}

CommandDispatcher::Result CommandDispatcher::apply(const ControllerCommand& c, uint32_t ticket) {
    Result result;
    result.ticket = ticket;
    result.revision_before = result.revision_after = controller_.revision();
    const auto status = validate(c); // Mode may have changed since admission.
    if (status != CommandStatus::accepted) {
        result.status = status;
        return result;
    }
    switch (c.type) {
        case CommandType::set_channel:
            result.domain = controller_.setChannel(c.payload.channel.channel, c.payload.channel.value);
            break;
        case CommandType::adjust_channel:
            result.domain = controller_.adjustChannel(c.payload.channel.channel, c.payload.channel.value);
            break;
        case CommandType::set_channels:
            result.domain = controller_.setChannels(c.payload.patch.values, c.payload.patch.count);
            break;
        case CommandType::set_blackout:
            controller_.setBlackout(c.payload.blackout != 0);
            break;
        case CommandType::load_preset:
            result.domain = controller_.loadPreset(static_cast<uint8_t>(c.payload.preset.slot));
            break;
        case CommandType::save_preset:
            result.domain = controller_.savePreset(static_cast<uint8_t>(c.payload.preset.slot));
            break;
        case CommandType::copy_preset:
            result.domain = controller_.copyPreset(static_cast<uint8_t>(c.payload.preset.slot),
                                                   static_cast<uint8_t>(c.payload.preset.target));
            break;
        case CommandType::clear_preset:
            result.domain = controller_.clearPreset(static_cast<uint8_t>(c.payload.preset.slot));
            break;
        case CommandType::reset_sender:
            controller_.reset();
            break;
        default: break; // Already rejected by validation.
    }
    result.revision_after = controller_.revision();
    switch (result.domain) {
        case DmxController::Result::ok:
            result.status = result.revision_after == result.revision_before
                ? CommandStatus::no_change : CommandStatus::applied;
            break;
        case DmxController::Result::corrupt:
        case DmxController::Result::read_failed:
        case DmxController::Result::write_failed:
        case DmxController::Result::metadata_failed:
            result.status = CommandStatus::storage_error;
            break;
        default:
            result.status = CommandStatus::rejected;
            break;
    }
    const bool changed = result.revision_after != result.revision_before;
    const bool publish = result.domain == DmxController::Result::ok &&
        (c.type == CommandType::load_preset ||
         (changed && (c.type == CommandType::set_channel || c.type == CommandType::adjust_channel ||
                      c.type == CommandType::set_channels || c.type == CommandType::set_blackout)));
    if (publish && publisher_) {
        result.transport_result = publisher_(publisher_context_, c);
        if (result.transport_result != 0) result.status = CommandStatus::transport_error;
    }
    return result;
}

CommandDispatcher::Result CommandDispatcher::processOne(bool local) {
    const Entry entry = queue_[head_];
    head_ = (head_ + 1) % capacity;
    --count_;
    local_execution_ = local;
    const Result result = apply(entry.command, entry.ticket);
    if (completion_) completion_(completion_context_, entry.command, result);
    local_execution_ = false;
    return result;
}

size_t CommandDispatcher::process(size_t budget) {
    if (processing_) return 0;
    processing_ = true;
    size_t done = 0;
    if (budget > capacity) budget = capacity;
    while (count_ && done < budget) {
        const auto& command = queue_[head_].command;
        if (validate(command) == CommandStatus::accepted && admission_ &&
            !admission_(admission_context_, command)) break;
        processOne(false);
        ++done;
    }
    processing_ = false;
    return done;
}

CommandDispatcher::Result CommandDispatcher::executeLocal(const ControllerCommand& command) {
    Result result;
    result.revision_before = result.revision_after = controller_.revision();
    result.status = processing_ ? CommandStatus::busy : validate(command);
    if (result.status != CommandStatus::accepted) return result;
    // Preserve FIFO barriers. At most 32 older commands plus this local command;
    // no waiting, coalescing, heap allocation, or second application path.
    process(capacity);
    if (count_ || (admission_ && !admission_(admission_context_, command))) {
        result.status = CommandStatus::busy;
        result.revision_after = controller_.revision();
        return result;
    }
    const Submission admission = submit(command);
    if (admission.status != CommandStatus::accepted) {
        result.status = admission.status;
        result.revision_after = controller_.revision();
        return result;
    }
    processing_ = true;
    result = processOne(true);
    processing_ = false;
    return result;
}
