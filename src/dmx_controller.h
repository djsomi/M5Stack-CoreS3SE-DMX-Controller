#pragma once

#include <stddef.h>
#include <stdint.h>
#include "controller_types.h"
#include "preset_archive.h"

class DmxPresetStorage {
 public:
    static constexpr size_t packet_size = 513;
    static constexpr uint8_t preset_count = 8;
    enum class SlotState { missing, valid, corrupt, error };
    virtual ~DmxPresetStorage() = default;
    virtual SlotState inspect(uint8_t slot) = 0;
    virtual bool load(uint8_t slot, uint8_t* packet) = 0;
    virtual bool save(uint8_t slot, const uint8_t* packet) = 0;
    virtual bool erase(uint8_t slot) = 0;
    virtual bool noSpace() const {return false;}
    virtual SlotState captureCollection(preset_archive::Collection& out) {
        for(uint8_t slot=0;slot<preset_count;++slot){
            const auto state=inspect(slot);if(state==SlotState::missing)continue;
            if(state!=SlotState::valid)return state;
            uint8_t packet[packet_size]{};if(!load(slot,packet))return SlotState::error;
            memcpy(out.values[slot],packet+1,512);out.present|=uint8_t(1U<<slot);
        }
        return SlotState::valid;
    }
    virtual preset_archive::Commit replaceAll(const preset_archive::Collection&,const preset_archive::Gate&) {return preset_archive::Commit::failed;}
};

// All access, including observers and the storage adapter, belongs to the
// application task. This class deliberately has no transport or UI dependency.
class DmxController {
 public:
    static constexpr size_t channel_count = 512;
    static constexpr size_t packet_size = DmxPresetStorage::packet_size;
    static constexpr uint8_t preset_count = DmxPresetStorage::preset_count;
    // Numeric status 10 remains reserved for existing v0.3 web clients; no
    // legacy metadata operation is performed by the Controller.
    enum class Result {
        ok, invalid_channel, invalid_value, invalid_slot, empty, same_slot,
        occupied, corrupt, read_failed, write_failed, metadata_failed, invalid_patch, archive_conflict, archive_uncertain, storage_full
    };
    enum class ChangeKind { reset, channel, universe, blackout, preset };
    struct Change {
        uint64_t revision;
        ChangeKind kind;
        uint16_t channel;
        uint8_t slot;
    };
    using Observer = void (*)(void*, const Change&);

    explicit DmxController(DmxPresetStorage& storage) : storage_(storage) {}
    DmxController(const DmxController&) = delete;
    DmxController& operator=(const DmxController&) = delete;
    void setObserver(Observer observer, void* context);
    void reset();
    uint8_t channel(int channel) const;
    Result setChannel(int channel, int value);
    Result adjustChannel(int channel, int delta);
    Result setChannels(const DmxChannelValue* values, size_t count);
    const uint8_t* workingPacket() const { return working_; }
    const uint8_t* outputPacket() const;
    bool blackoutActive() const { return blackout_; }
    // Transient provenance: 0 means unknown, otherwise P1-P8.
    uint8_t loadedPreset() const { return loaded_preset_; }
    bool loadedPresetModified() const { return loaded_modified_; }
    void setBlackout(bool active);
    uint64_t revision() const { return revision_; }

    DmxPresetStorage::SlotState presetState(uint8_t slot);
    bool presetExists(uint8_t slot);
    Result loadPreset(uint8_t slot);
    Result savePreset(uint8_t slot);
    Result copyPreset(uint8_t source, uint8_t target);
    Result clearPreset(uint8_t slot);
    uint64_t presetRevision() const {return preset_revision_;}
    Result captureArchive();
    Result stageArchive(const char* data,size_t length,uint64_t base,const preset_archive::Gate& gate);
    uint32_t archiveToken() const {return archive_token_;}
    const preset_archive::Collection& archive() const {return archive_;}
    bool archiveValid(uint32_t token) const {return token&&token==archive_token_;}
    void discardArchive(){archive_token_=0;}
    Result replaceArchive(uint32_t token);

 private:
    bool invalidateLoaded(uint8_t slot);
    Result readablePreset(uint8_t slot);
    void changed(ChangeKind kind, uint16_t channel = 0, uint8_t slot = 0);

    preset_archive::Collection archive_;
    preset_archive::Gate archive_gate_;
    uint32_t archive_token_=0,archive_counter_=0;
    uint64_t archive_base_=0,preset_revision_=0;
    DmxPresetStorage& storage_;
    uint8_t working_[packet_size] = {};
    uint8_t restore_[packet_size] = {};
    bool blackout_ = false;
    uint8_t loaded_preset_ = 0, restore_loaded_preset_ = 0;
    bool loaded_modified_ = false, restore_loaded_modified_ = false;
    uint64_t revision_ = 0;
    Observer observer_ = nullptr;
    void* observer_context_ = nullptr;
};
