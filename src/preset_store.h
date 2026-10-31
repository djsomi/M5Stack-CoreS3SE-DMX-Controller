#pragma once

#include <Preferences.h>
#include <esp_dmx.h>
#include <nvs.h>
#include "dmx_controller.h"
#include "preset_archive_store.h"

class PresetArchiveKv final : public preset_archive::Kv {
    nvs_handle_t handle_=0;esp_err_t opened_;esp_err_t* error_;
    static const char* bank(unsigned index){return index?"xbank1":"xbank0";}
    preset_archive::Io io(esp_err_t e){if(e!=ESP_OK&&e!=ESP_ERR_NVS_NOT_FOUND&&error_)*error_=e;if(e==ESP_OK)return preset_archive::Io::ok;if(e==ESP_ERR_NVS_NOT_FOUND)return preset_archive::Io::missing;log_e("preset archive NVS: %s",esp_err_to_name(e));return preset_archive::Io::error;}
 public:
    explicit PresetArchiveKv(bool write=false,esp_err_t* error=nullptr):opened_(nvs_open("dmxpreset",write?NVS_READWRITE:NVS_READONLY,&handle_)),error_(error){if(error_){*error_=ESP_OK;io(opened_);}}
    ~PresetArchiveKv(){if(opened_==ESP_OK)nvs_close(handle_);}
    preset_archive::Io selector(uint8_t& value) override {return opened_==ESP_OK?io(nvs_get_u8(handle_,"xactive",&value)):io(opened_);}
    bool select(uint8_t value) override {return opened_==ESP_OK&&io(nvs_set_u8(handle_,"xactive",value))==preset_archive::Io::ok;}
    preset_archive::Io read(unsigned b,uint8_t* out,size_t& size) override {return opened_==ESP_OK?io(nvs_get_blob(handle_,bank(b),out,&size)):io(opened_);}
    bool write(unsigned b,const uint8_t* data,size_t size) override {return opened_==ESP_OK&&io(nvs_set_blob(handle_,bank(b),data,size))==preset_archive::Io::ok;}
    bool erase(unsigned b) override {if(opened_!=ESP_OK)return false;const auto e=nvs_erase_key(handle_,bank(b));return io(e)!=preset_archive::Io::error;}
    bool reclaimLegacy() override {
        if(opened_!=ESP_OK)return false;
        bool changed=false;
        for(unsigned i=1;i<=8;++i){
            char key[4];snprintf(key,sizeof(key),"p%u",i);
            const auto rc=nvs_erase_key(handle_,key);
            if(io(rc)==preset_archive::Io::error)return false;
            changed|=rc==ESP_OK;
        }
        // No repeated flash write when there is nothing left to reclaim.
        return !changed||commit();
    }
    bool commit() override {return opened_==ESP_OK&&io(nvs_commit(handle_))==preset_archive::Io::ok;}
};

class PresetStore : public DmxPresetStorage {
   public:
    static constexpr uint8_t PRESET_COUNT = 8;
    bool noSpace() const override {return last_error_==ESP_ERR_NVS_NOT_ENOUGH_SPACE;}
    static_assert(DMX_MAX_PACKET_SIZE == packet_size, "Preset packet format must remain 513 bytes");

    SlotState captureCollection(preset_archive::Collection& out) override {
        PresetArchiveKv archive;const auto state=archive_.load(archive);
        if(state==preset_archive::Store::State::corrupt)return SlotState::corrupt;
        if(state==preset_archive::Store::State::error)return SlotState::error;
        if(state==preset_archive::Store::State::active){
            out.present=archive_.mask();
            for(unsigned i=0;i<PRESET_COUNT;++i)if(out.present&(1U<<i))memcpy(out.values[i],archive_.values(i),512);
            return SlotState::valid;
        }
        uint8_t cleared=0;if(!loadClearedMask(cleared))return SlotState::error;
        nvs_handle_t handle;const auto opened=nvs_open("dmxpreset",NVS_READONLY,&handle);
        if(opened==ESP_ERR_NVS_NOT_FOUND)return SlotState::valid;
        if(opened!=ESP_OK){log_e("preset snapshot open: %s",esp_err_to_name(opened));return SlotState::error;}
        struct Close {nvs_handle_t h;~Close(){nvs_close(h);}} close{handle};
        for(uint8_t slot=0;slot<PRESET_COUNT;++slot){
            if(cleared&(1U<<slot))continue;
            char key[8];makeKey(slot,key,sizeof(key));size_t size=0;auto rc=nvs_get_blob(handle,key,nullptr,&size);
            if(rc==ESP_ERR_NVS_NOT_FOUND)continue;
            if(rc!=ESP_OK){log_e("preset snapshot %s: %s",key,esp_err_to_name(rc));return rc==ESP_ERR_NVS_TYPE_MISMATCH?SlotState::corrupt:SlotState::error;}
            if(size!=packet_size)return SlotState::corrupt;
            uint8_t packet[packet_size];rc=nvs_get_blob(handle,key,packet,&size);
            if(rc!=ESP_OK){log_e("preset snapshot %s read: %s",key,esp_err_to_name(rc));return SlotState::error;}
            if(size!=packet_size)return SlotState::corrupt;
            memcpy(out.values[slot],packet+1,512);out.present|=uint8_t(1U<<slot);
        }
        return SlotState::valid;
    }

    bool save(uint8_t slot, const uint8_t* packet) override {
        if (slot >= PRESET_COUNT || packet == nullptr) {
            return false;
        }

        PresetArchiveKv archive(true,&last_error_);const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){
            if(state!=preset_archive::Store::State::active)return false;
            if((archive_.mask()&(1U<<slot))&&!memcmp(archive_.values(slot),packet+1,512))return true;
            archive_.value(slot,packet+1);return archive_.publish(archive,false,{})==preset_archive::Commit::ok;
        }
        char key[8];makeKey(slot,key,sizeof(key));
        return persist("dmxpreset",key,packet,packet_size);
    }

    bool load(uint8_t slot, uint8_t* packet) override {
        if (slot >= PRESET_COUNT || packet == nullptr) {
            return false;
        }

        PresetArchiveKv archive;const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){
            if(state!=preset_archive::Store::State::active||!(archive_.mask()&(1U<<slot)))return false;
            packet[0]=0;memcpy(packet+1,archive_.values(slot),512);return true;
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
        PresetArchiveKv archive;const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){
            if(state==preset_archive::Store::State::corrupt)return SlotState::corrupt;
            if(state!=preset_archive::Store::State::active)return SlotState::error;
            return archive_.mask()&(1U<<slot)?SlotState::valid:SlotState::missing;
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
        PresetArchiveKv archive;const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){if(state!=preset_archive::Store::State::active)return false;mask=uint8_t(~archive_.mask());return true;}
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
        PresetArchiveKv archive(true,&last_error_);const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){
            if(state!=preset_archive::Store::State::active)return false;
            if(archive_.mask()==uint8_t(~mask))return true;
            archive_.mask(uint8_t(~mask));return archive_.publish(archive,false,{})==preset_archive::Commit::ok;
        }
        return persist("dmxmeta","cleared",&mask,0);
    }

    bool erase(uint8_t slot) {
        if (slot >= PRESET_COUNT) {
            return false;
        }

        PresetArchiveKv archive(true,&last_error_);const auto state=archive_.load(archive);
        if(state!=preset_archive::Store::State::legacy){
            if(state!=preset_archive::Store::State::active||!(archive_.mask()&(1U<<slot)))return false;
            archive_.mask(uint8_t(archive_.mask()&~(1U<<slot)));return archive_.publish(archive,false,{})==preset_archive::Commit::ok;
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

    preset_archive::Commit replaceAll(const preset_archive::Collection& collection,const preset_archive::Gate& gate) override {
        PresetArchiveKv kv(true,&last_error_);return archive_.replace(kv,collection,gate);
    }
   private:
    preset_archive::Store archive_;
    esp_err_t last_error_=ESP_OK;
    bool persist(const char* ns,const char* key,const uint8_t* data,size_t size){
        nvs_handle_t handle;
        last_error_=nvs_open(ns,NVS_READWRITE,&handle);
        if(last_error_==ESP_OK){
            last_error_=size?nvs_set_blob(handle,key,data,size):nvs_set_u8(handle,key,*data);
            if(last_error_==ESP_OK)last_error_=nvs_commit(handle);
            nvs_close(handle);
        }
        if(last_error_!=ESP_OK)log_e("preset %s/%s write: %s",ns,key,esp_err_to_name(last_error_));
        return last_error_==ESP_OK;
    }
    static void makeKey(uint8_t slot, char* key, size_t keySize) {
        // User-visible slots are P1–P8.
        snprintf(key, keySize, "p%u",
                 static_cast<unsigned int>(slot + 1));
    }
};
