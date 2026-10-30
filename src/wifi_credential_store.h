#pragma once
#include "wifi_credential_transaction.h"
#include <nvs.h>

// Opened only by setup/recovery or the network worker. Never from a callback.
class WifiCredentialStore final : public wifi_credentials::Store {
 public:
    WifiCredentialStore() {error_=nvs_open("dmxwifi",NVS_READWRITE,&handle_);}
    ~WifiCredentialStore() override {if(handle_)nvs_close(handle_);}
    Read read(const char* key,void* output,size_t capacity,size_t& size) override {
        if(!handle_)return Read::error;
        size=0;auto rc=nvs_get_blob(handle_,key,nullptr,&size);
        if(rc==ESP_ERR_NVS_NOT_FOUND)return Read::missing;
        if(rc!=ESP_OK||!size||size>capacity){error_=rc==ESP_OK?ESP_ERR_INVALID_SIZE:rc;return Read::error;}
        error_=nvs_get_blob(handle_,key,output,&size);return error_==ESP_OK?Read::found:Read::error;
    }
    bool write(const char* key,const void* data,size_t size) override {
        if(!handle_)return false;
        error_=nvs_set_blob(handle_,key,data,size);if(error_==ESP_OK)error_=nvs_commit(handle_);return error_==ESP_OK;
    }
    bool remove(const char* key) override {
        if(!handle_)return false;
        error_=nvs_erase_key(handle_,key);if(error_==ESP_ERR_NVS_NOT_FOUND)return true;
        if(error_==ESP_OK)error_=nvs_commit(handle_);return error_==ESP_OK;
    }
    esp_err_t error() const {return error_;}
 private:
    nvs_handle_t handle_=0;esp_err_t error_=ESP_OK;
};
