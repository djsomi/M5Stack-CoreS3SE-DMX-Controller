#include "dmx_controller.h"

#include <string.h>

void DmxController::setObserver(Observer observer, void* context) {
    observer_ = observer;
    observer_context_ = context;
}

void DmxController::changed(ChangeKind kind, uint16_t channel, uint8_t slot) {
    ++revision_;
    if (observer_) {
        const Change change{revision_, kind, channel, slot};
        observer_(observer_context_, change);
    }
}

void DmxController::reset() {
    memset(working_, 0, sizeof(working_));
    memset(restore_, 0, sizeof(restore_));
    blackout_ = false;
    changed(ChangeKind::reset);
}

uint8_t DmxController::channel(int index) const {
    return index >= 1 && index <= static_cast<int>(channel_count) ? working_[index] : 0;
}

DmxController::Result DmxController::setChannel(int index, int value) {
    if (index < 1 || index > static_cast<int>(channel_count)) return Result::invalid_channel;
    if (value < 0 || value > 255) return Result::invalid_value;
    if (working_[index] != value) {
        working_[index] = static_cast<uint8_t>(value);
        changed(ChangeKind::channel, static_cast<uint16_t>(index));
    }
    return Result::ok;
}

DmxController::Result DmxController::adjustChannel(int index, int delta) {
    if (index < 1 || index > static_cast<int>(channel_count)) return Result::invalid_channel;
    int64_t value = static_cast<int64_t>(working_[index]) + delta;
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    return setChannel(index, static_cast<int>(value));
}

const uint8_t* DmxController::outputPacket() const {
    static const uint8_t zero_packet[packet_size] = {};
    return blackout_ ? zero_packet : working_;
}

void DmxController::setBlackout(bool active) {
    if (active == blackout_) return;
    if (active) {
        memcpy(restore_, working_, sizeof(working_));
    } else {
        memcpy(working_, restore_, sizeof(working_));
        working_[0] = 0;
    }
    blackout_ = active;
    changed(ChangeKind::blackout);
}

bool DmxController::loadMetadata() {
    if (metadata_loaded_) return true;
    if (!storage_.loadClearedMask(cleared_mask_)) return false;
    metadata_loaded_ = true;
    return true;
}

bool DmxController::markCleared(uint8_t slot, bool cleared) {
    if (!loadMetadata()) return false;
    const uint8_t bit = static_cast<uint8_t>(1U << slot);
    uint8_t next = cleared ? (cleared_mask_ | bit) : (cleared_mask_ & ~bit);
    if (next == cleared_mask_) return true;
    if (!storage_.saveClearedMask(next)) return false;
    cleared_mask_ = next;
    return true;
}

DmxPresetStorage::SlotState DmxController::presetState(uint8_t slot) {
    if (slot >= preset_count || !loadMetadata()) return DmxPresetStorage::SlotState::error;
    if (cleared_mask_ & (1U << slot)) return DmxPresetStorage::SlotState::missing;
    return storage_.inspect(slot);
}

bool DmxController::presetExists(uint8_t slot) {
    return presetState(slot) == DmxPresetStorage::SlotState::valid;
}

DmxController::Result DmxController::readablePreset(uint8_t slot) {
    if (slot >= preset_count) return Result::invalid_slot;
    switch (presetState(slot)) {
        case DmxPresetStorage::SlotState::valid: return Result::ok;
        case DmxPresetStorage::SlotState::missing: return Result::empty;
        case DmxPresetStorage::SlotState::corrupt: return Result::corrupt;
        default: return Result::read_failed;
    }
}

DmxController::Result DmxController::loadPreset(uint8_t slot) {
    Result result = readablePreset(slot);
    if (result != Result::ok) return result;
    uint8_t packet[packet_size] = {};
    if (!storage_.load(slot, packet)) return Result::read_failed;
    packet[0] = 0;
    if (memcmp(working_, packet, sizeof(working_)) != 0) {
        memcpy(working_, packet, sizeof(working_));
        changed(ChangeKind::universe);
    }
    return Result::ok;
}

DmxController::Result DmxController::savePreset(uint8_t slot) {
    if (slot >= preset_count) return Result::invalid_slot;
    if (!loadMetadata()) return Result::metadata_failed;
    if (!storage_.save(slot, working_)) return Result::write_failed;
    if (!markCleared(slot, false)) return Result::metadata_failed;
    changed(ChangeKind::preset, 0, slot);
    return Result::ok;
}

DmxController::Result DmxController::copyPreset(uint8_t source, uint8_t target) {
    if (source >= preset_count || target >= preset_count) return Result::invalid_slot;
    Result result = readablePreset(source);
    if (result != Result::ok) return result;
    if (source == target) return Result::same_slot;
    const auto state = presetState(target);
    if (state == DmxPresetStorage::SlotState::valid) return Result::occupied;
    if (state == DmxPresetStorage::SlotState::error) return Result::read_failed;
    if (state == DmxPresetStorage::SlotState::corrupt) return Result::corrupt;
    uint8_t packet[packet_size] = {};
    if (!storage_.load(source, packet)) return Result::read_failed;
    packet[0] = 0;
    if (!storage_.save(target, packet)) return Result::write_failed;
    if (!markCleared(target, false)) return Result::metadata_failed;
    changed(ChangeKind::preset, 0, target);
    return Result::ok;
}

DmxController::Result DmxController::clearPreset(uint8_t slot) {
    Result result = readablePreset(slot);
    if (result != Result::ok) return result;
    // Keep the legacy blob; logical occupancy is the existing cleared mask.
    if (!markCleared(slot, true)) return Result::metadata_failed;
    changed(ChangeKind::preset, 0, slot);
    return Result::ok;
}
