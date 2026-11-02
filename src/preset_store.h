#pragma once

#include <esp32-hal-log.h>
#include <esp_dmx.h>
#include <nvs.h>
#include <string.h>
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
    bool erase(unsigned b) override {if(opened_!=ESP_OK)return false;return io(nvs_erase_key(handle_,bank(b)))!=preset_archive::Io::error;}
    bool commit() override {return opened_==ESP_OK&&io(nvs_commit(handle_))==preset_archive::Io::ok;}
};

class PresetStore : public DmxPresetStorage {
 public:
    static constexpr uint8_t PRESET_COUNT=8;
    static_assert(DMX_MAX_PACKET_SIZE==packet_size,"Preset packet format must remain 513 bytes");
    bool noSpace() const override {return last_error_==ESP_ERR_NVS_NOT_ENOUGH_SPACE;}
    SlotState captureCollection(preset_archive::Collection& out) override {
        PresetArchiveKv kv;const auto state=archive_.load(kv);
        if(state==preset_archive::Store::State::corrupt)return SlotState::corrupt;
        if(state==preset_archive::Store::State::error)return SlotState::error;
        out.present=archive_.mask();
        for(unsigned i=0;i<PRESET_COUNT;++i)if(out.present&(1U<<i))memcpy(out.values[i],archive_.values(i),512);
        return SlotState::valid;
    }
    bool save(uint8_t slot,const uint8_t* packet) override {
        if(slot>=PRESET_COUNT||!packet)return false;
        PresetArchiveKv kv(true,&last_error_);const auto state=archive_.load(kv);
        if(state!=preset_archive::Store::State::active&&state!=preset_archive::Store::State::empty)return false;
        if((archive_.mask()&(1U<<slot))&&!memcmp(archive_.values(slot),packet+1,512))return true;
        archive_.value(slot,packet+1);return archive_.publish(kv,{})==preset_archive::Commit::ok;
    }
    bool load(uint8_t slot,uint8_t* packet) override {
        if(slot>=PRESET_COUNT||!packet)return false;
        PresetArchiveKv kv;
        if(archive_.load(kv)!=preset_archive::Store::State::active||!(archive_.mask()&(1U<<slot)))return false;
        packet[0]=0;memcpy(packet+1,archive_.values(slot),512);return true;
    }
    SlotState inspect(uint8_t slot) override {
        if(slot>=PRESET_COUNT)return SlotState::error;
        PresetArchiveKv kv;const auto state=archive_.load(kv);
        if(state==preset_archive::Store::State::corrupt)return SlotState::corrupt;
        if(state==preset_archive::Store::State::error)return SlotState::error;
        return archive_.mask()&(1U<<slot)?SlotState::valid:SlotState::missing;
    }
    bool erase(uint8_t slot) override {
        if(slot>=PRESET_COUNT)return false;
        PresetArchiveKv kv(true,&last_error_);
        if(archive_.load(kv)!=preset_archive::Store::State::active||!(archive_.mask()&(1U<<slot)))return false;
        archive_.mask(uint8_t(archive_.mask()&~(1U<<slot)));
        return archive_.publish(kv,{})==preset_archive::Commit::ok;
    }
    preset_archive::Commit replaceAll(const preset_archive::Collection& collection,const preset_archive::Gate& gate) override {
        PresetArchiveKv kv(true,&last_error_);return archive_.replace(kv,collection,gate);
    }
 private:
    preset_archive::Store archive_;
    esp_err_t last_error_=ESP_OK;
};
