#pragma once
#include <stdio.h>
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
#define log_e(...) do{fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");}while(0)
