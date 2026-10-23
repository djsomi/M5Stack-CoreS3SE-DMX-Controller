#pragma once

#include <Preferences.h>
#include <esp_dmx.h>
#include <nvs.h>
#include "dmx_controller.h"

class PresetStore : public DmxPresetStorage {
   public:
    static constexpr uint8_t PRESET_COUNT = 8;
    static_assert(DMX_MAX_PACKET_SIZE == packet_size, "Preset packet format must remain 513 bytes");

    bool save(uint8_t slot, const uint8_t* packet) override {
        if (slot >= PRESET_COUNT || packet == nullptr) {
            return false;
        }

        Preferences preferences;

        if (!preferences.begin("dmxpreset", false)) {
            return false;
        }

        char key[8];
        makeKey(slot, key, sizeof(key));

        // Save the complete DMX packet:
        // byte 0 = DMX start code
        // bytes 1–512 = channel values
        size_t written =
            preferences.putBytes(key, packet, DMX_MAX_PACKET_SIZE);

        preferences.end();

        return written == DMX_MAX_PACKET_SIZE;
    }

    bool load(uint8_t slot, uint8_t* packet) override {
        if (slot >= PRESET_COUNT || packet == nullptr) {
            return false;
        }

        if (inspect(slot) != SlotState::valid) {
            return false;
        }

        Preferences preferences;

        if (!preferences.begin("dmxpreset", true)) {
            return false;
        }

        char key[8];
        makeKey(slot, key, sizeof(key));

        size_t loaded =
            preferences.getBytes(key, packet, DMX_MAX_PACKET_SIZE);

        preferences.end();

        return loaded == DMX_MAX_PACKET_SIZE;
    }

    bool exists(uint8_t slot) {
        return inspect(slot) == SlotState::valid;
    }

    SlotState inspect(uint8_t slot) override {
        if (slot >= PRESET_COUNT) {
            return SlotState::error;
        }
        nvs_handle_t handle;
        esp_err_t result = nvs_open("dmxpreset", NVS_READONLY, &handle);
        if (result == ESP_ERR_NVS_NOT_FOUND) return SlotState::missing;
        if (result != ESP_OK) {
            log_e("preset namespace read failed: %s", esp_err_to_name(result));
            return SlotState::error;
        }
        char key[8];
        makeKey(slot, key, sizeof(key));
        size_t size = 0;
        result = nvs_get_blob(handle, key, nullptr, &size);
        nvs_close(handle);
        // Missing slots are normal. Do not suppress type/size or storage errors.
        if (result == ESP_ERR_NVS_NOT_FOUND) return SlotState::missing;
        if (result != ESP_OK) {
            log_e("preset %s query failed: %s", key, esp_err_to_name(result));
            return result == ESP_ERR_NVS_TYPE_MISMATCH ? SlotState::corrupt : SlotState::error;
        }
        if (size != packet_size) {
            log_e("preset %s corrupt length: %u (expected %u)", key,
                  static_cast<unsigned>(size), static_cast<unsigned>(packet_size));
            return SlotState::corrupt;
        }
        return SlotState::valid;
    }

    bool loadClearedMask(uint8_t& mask) override {
        nvs_handle_t handle;
        esp_err_t result = nvs_open("dmxmeta", NVS_READONLY, &handle);
        if (result == ESP_ERR_NVS_NOT_FOUND) {
            mask = 0;
            return true;
        }
        if (result != ESP_OK) {
            log_e("preset metadata open failed: %s", esp_err_to_name(result));
            return false;
        }
        result = nvs_get_u8(handle, "cleared", &mask);
        nvs_close(handle);
        if (result == ESP_ERR_NVS_NOT_FOUND) {
            mask = 0;
            return true;
        }
        if (result != ESP_OK) {
            log_e("preset metadata read failed: %s", esp_err_to_name(result));
            return false;
        }
        return true;
    }

    bool saveClearedMask(uint8_t mask) override {
        Preferences preferences;
        if (!preferences.begin("dmxmeta", false)) return false;
        const bool success = preferences.putUChar("cleared", mask) == sizeof(mask);
        preferences.end();
        return success;
    }

    bool erase(uint8_t slot) {
        if (slot >= PRESET_COUNT) {
            return false;
        }

        Preferences preferences;

        if (!preferences.begin("dmxpreset", false)) {
            return false;
        }

        char key[8];
        makeKey(slot, key, sizeof(key));

        bool removed = preferences.remove(key);

        preferences.end();

        return removed;
    }

   private:
    static void makeKey(uint8_t slot, char* key, size_t keySize) {
        // User-visible slots are P1–P8.
        snprintf(key, keySize, "p%u",
                 static_cast<unsigned int>(slot + 1));
    }
};
