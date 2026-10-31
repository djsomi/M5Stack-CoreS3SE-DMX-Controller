#pragma once
#include "esp_err.h"
#include <stddef.h>
#define SPI_FLASH_SEC_SIZE 4096
#define ESP_ENCRYPT_BLOCK_SIZE 16
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2
struct esp_partition_t{uint32_t address,size;char label[17];bool encrypted;};
extern "C" {
const esp_partition_t* esp_partition_find_first(int,int,const char*);
int esp_partition_read(const esp_partition_t*,size_t,void*,size_t);
int esp_partition_read_raw(const esp_partition_t*,size_t,void*,size_t);
int esp_partition_write(const esp_partition_t*,size_t,const void*,size_t);
int esp_partition_write_raw(const esp_partition_t*,size_t,const void*,size_t);
int esp_partition_erase_range(const esp_partition_t*,size_t,size_t);
}
