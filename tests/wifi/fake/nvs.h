#pragma once
#include <stddef.h>
#include <stdint.h>
using esp_err_t=int;
using nvs_handle_t=uint32_t;
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NVS_NOT_FOUND=10,ESP_ERR_INVALID_SIZE=11,
      ESP_ERR_NVS_TYPE_MISMATCH=12,NVS_READWRITE=1};
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
esp_err_t nvs_erase_key(nvs_handle_t,const char*);
esp_err_t nvs_commit(nvs_handle_t);
