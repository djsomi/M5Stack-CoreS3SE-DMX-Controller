#pragma once

#include <Preferences.h>
#include <esp_dmx.h>

class PresetStore {
   public:
    static constexpr uint8_t PRESET_COUNT = 8;

    bool save(uint8_t slot, const uint8_t* packet) {
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

    bool load(uint8_t slot, uint8_t* packet) {
        if (slot >= PRESET_COUNT || packet == nullptr) {
            return false;
        }

        Preferences preferences;

        if (!preferences.begin("dmxpreset", true)) {
            return false;
        }

        char key[8];
        makeKey(slot, key, sizeof(key));

        size_t storedSize = preferences.getBytesLength(key);

        if (storedSize != DMX_MAX_PACKET_SIZE) {
            preferences.end();
            return false;
        }

        size_t loaded =
            preferences.getBytes(key, packet, DMX_MAX_PACKET_SIZE);

        preferences.end();

        return loaded == DMX_MAX_PACKET_SIZE;
    }

    bool exists(uint8_t slot) {
        if (slot >= PRESET_COUNT) {
            return false;
        }

        Preferences preferences;

        if (!preferences.begin("dmxpreset", true)) {
            return false;
        }

        char key[8];
        makeKey(slot, key, sizeof(key));

        bool valid =
            preferences.getBytesLength(key) == DMX_MAX_PACKET_SIZE;

        preferences.end();

        return valid;
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