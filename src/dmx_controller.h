#pragma once

#include <stddef.h>
#include <stdint.h>

class DmxPresetStorage {
 public:
    static constexpr size_t packet_size = 513;
    static constexpr uint8_t preset_count = 8;
    enum class SlotState { missing, valid, corrupt, error };
    virtual ~DmxPresetStorage() = default;
    virtual SlotState inspect(uint8_t slot) = 0;
    virtual bool load(uint8_t slot, uint8_t* packet) = 0;
    virtual bool save(uint8_t slot, const uint8_t* packet) = 0;
    virtual bool loadClearedMask(uint8_t& mask) = 0;
    virtual bool saveClearedMask(uint8_t mask) = 0;
};

// All access, including observers and the storage adapter, belongs to the
// application task. This class deliberately has no transport or UI dependency.
class DmxController {
 public:
    static constexpr size_t channel_count = 512;
    static constexpr size_t packet_size = DmxPresetStorage::packet_size;
    static constexpr uint8_t preset_count = DmxPresetStorage::preset_count;
    enum class Result {
        ok, invalid_channel, invalid_value, invalid_slot, empty, same_slot,
        occupied, corrupt, read_failed, write_failed, metadata_failed
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
    const uint8_t* workingPacket() const { return working_; }
    const uint8_t* outputPacket() const;
    bool blackoutActive() const { return blackout_; }
    void setBlackout(bool active);
    uint64_t revision() const { return revision_; }

    DmxPresetStorage::SlotState presetState(uint8_t slot);
    bool presetExists(uint8_t slot);
    Result loadPreset(uint8_t slot);
    Result savePreset(uint8_t slot);
    Result copyPreset(uint8_t source, uint8_t target);
    Result clearPreset(uint8_t slot);

 private:
    bool loadMetadata();
    bool markCleared(uint8_t slot, bool cleared);
    Result readablePreset(uint8_t slot);
    void changed(ChangeKind kind, uint16_t channel = 0, uint8_t slot = 0);

    DmxPresetStorage& storage_;
    uint8_t working_[packet_size] = {};
    uint8_t restore_[packet_size] = {};
    bool blackout_ = false;
    bool metadata_loaded_ = false;
    uint8_t cleared_mask_ = 0;
    uint64_t revision_ = 0;
    Observer observer_ = nullptr;
    void* observer_context_ = nullptr;
};
