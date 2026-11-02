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
    bool dirty = blackout_ || loaded_preset_ || restore_loaded_preset_;
    for (size_t i = 0; i < packet_size; ++i) dirty = dirty || working_[i] || restore_[i];
    memset(working_, 0, sizeof(working_));
    memset(restore_, 0, sizeof(restore_));
    blackout_ = false;
    loaded_preset_ = restore_loaded_preset_ = 0;
    loaded_modified_ = restore_loaded_modified_ = false;
    if (dirty) changed(ChangeKind::reset);
}

uint8_t DmxController::channel(int index) const {
    return index >= 1 && index <= static_cast<int>(channel_count) ? working_[index] : 0;
}

DmxController::Result DmxController::setChannel(int index, int value) {
    if (index < 1 || index > static_cast<int>(channel_count)) return Result::invalid_channel;
    if (value < 0 || value > 255) return Result::invalid_value;
    if (working_[index] != value) {
        working_[index] = static_cast<uint8_t>(value);
        if (loaded_preset_) loaded_modified_ = true;
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

DmxController::Result DmxController::setChannels(const DmxChannelValue* values, size_t count) {
    if (!values || count == 0 || count > dmx_max_patch_channels) return Result::invalid_patch;
    for (size_t i = 0; i < count; ++i) {
        if (values[i].channel < 1 || values[i].channel > static_cast<int>(channel_count))
            return Result::invalid_channel;
        if (values[i].value < 0 || values[i].value > 255) return Result::invalid_value;
        for (size_t j = 0; j < i; ++j) {
            if (values[i].channel == values[j].channel) return Result::invalid_patch;
        }
    }
    bool dirty = false;
    for (size_t i = 0; i < count; ++i) {
        dirty = dirty || working_[values[i].channel] != values[i].value;
        working_[values[i].channel] = static_cast<uint8_t>(values[i].value);
    }
    if (dirty) {
        if (loaded_preset_) loaded_modified_ = true;
        changed(ChangeKind::universe);
    }
    return Result::ok;
}

const uint8_t* DmxController::outputPacket() const {
    static const uint8_t zero_packet[packet_size] = {};
    return blackout_ ? zero_packet : working_;
}

void DmxController::setBlackout(bool active) {
    if (active == blackout_) return;
    if (active) {
        memcpy(restore_, working_, sizeof(working_));
        restore_loaded_preset_ = loaded_preset_;
        restore_loaded_modified_ = loaded_modified_;
    } else {
        memcpy(working_, restore_, sizeof(working_));
        working_[0] = 0;
        loaded_preset_ = restore_loaded_preset_;
        loaded_modified_ = restore_loaded_modified_;
    }
    blackout_ = active;
    changed(ChangeKind::blackout);
}

DmxPresetStorage::SlotState DmxController::presetState(uint8_t slot) {
    if (slot >= preset_count) return DmxPresetStorage::SlotState::error;
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
    const bool dirty = memcmp(working_, packet, sizeof(working_)) != 0 ||
        loaded_preset_ != slot + 1 || loaded_modified_;
    loaded_preset_ = slot + 1;
    loaded_modified_ = false;
    if (dirty) {
        memcpy(working_, packet, sizeof(working_));
        changed(ChangeKind::universe);
    }
    return Result::ok;
}

DmxController::Result DmxController::savePreset(uint8_t slot) {
    if (slot >= preset_count) return Result::invalid_slot;
    const auto state = presetState(slot);
    if (state == DmxPresetStorage::SlotState::error) return Result::read_failed;
    if (state == DmxPresetStorage::SlotState::valid) {
        uint8_t saved[packet_size] = {};
        if (!storage_.load(slot, saved)) return Result::read_failed;
        if (memcmp(saved, working_, packet_size) == 0) return Result::ok;
    }
    // Even a failed partial replacement invalidates old load provenance.
    ++preset_revision_; // Invalidate uploads even if a storage API fails after writing.
    const bool invalidated = invalidateLoaded(slot);
    if (!storage_.save(slot, working_)) {
        if (invalidated) changed(ChangeKind::preset, 0, slot);
        return storage_.noSpace()?Result::storage_full:Result::write_failed;
    }
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
    ++preset_revision_;
    if (!storage_.save(target, packet)) return storage_.noSpace()?Result::storage_full:Result::write_failed;
    changed(ChangeKind::preset, 0, target);
    return Result::ok;
}

DmxController::Result DmxController::clearPreset(uint8_t slot) {
    Result result = readablePreset(slot);
    if (result != Result::ok) return result;
    ++preset_revision_;
    const bool invalidated = invalidateLoaded(slot);
    if (!storage_.erase(slot)) {
        if (invalidated) changed(ChangeKind::preset, 0, slot);
        return storage_.noSpace()?Result::storage_full:Result::write_failed;
    }
    changed(ChangeKind::preset, 0, slot);
    return Result::ok;
}

bool DmxController::invalidateLoaded(uint8_t slot) {
    bool dirty = false;
    if (loaded_preset_ == slot + 1) {
        loaded_preset_ = 0; loaded_modified_ = false; dirty = true;
    }
    if (restore_loaded_preset_ == slot + 1) {
        restore_loaded_preset_ = 0; restore_loaded_modified_ = false; dirty = true;
    }
    return dirty;
}

DmxController::Result DmxController::captureArchive() {
    discardArchive();archive_.present=0;memset(archive_.values,0,sizeof(archive_.values));
    // One owner turn; storage can read/verify an active generation just once.
    const auto state=storage_.captureCollection(archive_);
    return state==DmxPresetStorage::SlotState::valid?Result::ok:state==DmxPresetStorage::SlotState::corrupt?Result::corrupt:Result::read_failed;
}
DmxController::Result DmxController::stageArchive(const char* data,size_t length,uint64_t base,const preset_archive::Gate& gate) {
    discardArchive();if(base!=preset_revision_)return Result::archive_conflict;
    if(!gate.allowed()||!preset_archive::parse(data,length,archive_))return Result::invalid_patch;
    archive_base_=base;archive_gate_=gate;if(++archive_counter_==0)++archive_counter_;archive_token_=archive_counter_;return Result::ok;
}
DmxController::Result DmxController::replaceArchive(uint32_t token) {
    if(!archiveValid(token))return Result::invalid_patch;
    discardArchive();if(archive_base_!=preset_revision_)return Result::archive_conflict;
    if(!archive_gate_.allowed())return Result::invalid_patch;
    const auto result=storage_.replaceAll(archive_,archive_gate_);
    if(result==preset_archive::Commit::failed)return storage_.noSpace()?Result::storage_full:Result::write_failed;
    ++preset_revision_;
    // Only stored-slot provenance changes. Working/output/restore packets and
    // blackout state remain byte-for-byte untouched; transport is not published.
    loaded_preset_=restore_loaded_preset_=0;loaded_modified_=restore_loaded_modified_=false;
    changed(ChangeKind::preset);
    return result==preset_archive::Commit::ok?Result::ok:Result::archive_uncertain;
}
