#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_FLASH_OP_FAIL 0x6002
#define ESP_ERR_FLASH_OP_TIMEOUT 0x6001
inline const char* esp_err_to_name(int rc){static char s[20];snprintf(s,sizeof(s),"0x%x",rc);return s;}
#define ESP_ERROR_CHECK(x) do {int rc=(x);if(rc){fprintf(stderr,"CHECK %x\n",rc);abort();}}while(0)
