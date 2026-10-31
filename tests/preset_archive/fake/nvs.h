#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>
#include <vector>
#include <string.h>
#include <stdio.h>
using esp_err_t=int;using nvs_handle_t=unsigned;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_ERR_NVS_TYPE_MISMATCH=2,ESP_ERR_NVS_NOT_ENOUGH_SPACE=0x1105,NVS_READONLY=0,NVS_READWRITE=1;
namespace fake_nvs {extern std::map<std::string,std::vector<uint8_t>> data;extern std::map<unsigned,std::string> handles;extern unsigned next,reads,bank_reads;extern bool fail_write;}
inline const char* esp_err_to_name(int){return "NVS_TEST_ERROR";}
#define log_e(...) ((void)0)
inline int nvs_open(const char* name,int mode,nvs_handle_t* h){auto& d=fake_nvs::data;bool exists=false;for(auto& p:d)if(p.first.find(std::string(name)+"/")==0)exists=true;if(!mode&&!exists)return ESP_ERR_NVS_NOT_FOUND;*h=++fake_nvs::next;fake_nvs::handles[*h]=name;return 0;}
inline void nvs_close(nvs_handle_t h){fake_nvs::handles.erase(h);}
inline std::string key(nvs_handle_t h,const char* k){return fake_nvs::handles[h]+"/"+k;}
inline int nvs_get_blob(nvs_handle_t h,const char* k,void* out,size_t* size){++fake_nvs::reads;if(out&&(!strcmp(k,"xbank0")||!strcmp(k,"xbank1")))++fake_nvs::bank_reads;auto it=fake_nvs::data.find(key(h,k));if(it==fake_nvs::data.end())return ESP_ERR_NVS_NOT_FOUND;if(out&&*size<it->second.size())return 3;if(out)memcpy(out,it->second.data(),it->second.size());*size=it->second.size();return 0;}
inline int nvs_set_blob(nvs_handle_t h,const char* k,const void* data,size_t size){if(fake_nvs::fail_write)return 3;auto p=static_cast<const uint8_t*>(data);fake_nvs::data[key(h,k)]={p,p+size};return 0;}
inline int nvs_get_u8(nvs_handle_t h,const char* k,uint8_t* v){size_t n=1;auto e=nvs_get_blob(h,k,v,&n);return e?e:n==1?0:ESP_ERR_NVS_TYPE_MISMATCH;}
inline int nvs_set_u8(nvs_handle_t h,const char* k,uint8_t v){return nvs_set_blob(h,k,&v,1);}
inline int nvs_commit(nvs_handle_t){return fake_nvs::fail_write?3:0;}
inline int nvs_erase_key(nvs_handle_t h,const char* k){return fake_nvs::data.erase(key(h,k))?0:ESP_ERR_NVS_NOT_FOUND;}
