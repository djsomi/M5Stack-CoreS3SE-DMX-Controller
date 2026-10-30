#include "wifi_credential_store.h"
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
std::map<std::string,std::vector<uint8_t>> records;
bool open_fail=false,read_fail=false,set_fail=false,commit_fail=false,erase_fail=false,type_fail=false;
unsigned commits=0,closes=0;
esp_err_t nvs_open(const char* name,int mode,nvs_handle_t* handle){assert(!strcmp(name,"dmxwifi")&&mode==NVS_READWRITE);if(open_fail)return ESP_FAIL;*handle=1;return ESP_OK;}
void nvs_close(nvs_handle_t handle){assert(handle==1);++closes;}
esp_err_t nvs_get_blob(nvs_handle_t handle,const char* key,void* data,size_t* size){
 assert(handle==1);if(read_fail)return ESP_FAIL;if(type_fail)return ESP_ERR_NVS_TYPE_MISMATCH;
 auto it=records.find(key);if(it==records.end())return ESP_ERR_NVS_NOT_FOUND;
 if(data){assert(*size>=it->second.size());memcpy(data,it->second.data(),it->second.size());}
 *size=it->second.size();return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char* key,const void* data,size_t size){assert(handle==1);if(set_fail)return ESP_FAIL;const auto* p=static_cast<const uint8_t*>(data);records[key]={p,p+size};return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t handle,const char* key){assert(handle==1);if(erase_fail)return ESP_FAIL;return records.erase(key)?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_commit(nvs_handle_t handle){assert(handle==1);++commits;return commit_fail?ESP_FAIL:ESP_OK;}
int main(){
 using Read=wifi_credentials::Store::Read;
 uint8_t copy[232]{};size_t size=0;const uint8_t bytes[]={1,2,3,4};
 {open_fail=true;WifiCredentialStore unavailable;assert(unavailable.read("ap1",copy,sizeof(copy),size)==Read::error);assert(!unavailable.write("ap1",bytes,sizeof(bytes)));open_fail=false;}
 {WifiCredentialStore store;assert(store.read("ap1",copy,sizeof(copy),size)==Read::missing);
  assert(store.write("ap1",bytes,sizeof(bytes))&&commits==1);assert(wifi_credentials::verify(store,"ap1",bytes,sizeof(bytes)));
  assert(store.read("ap1",copy,1,size)==Read::error&&store.error()==ESP_ERR_INVALID_SIZE);
  read_fail=true;assert(store.read("ap1",copy,sizeof(copy),size)==Read::error);read_fail=false;
  type_fail=true;assert(store.read("ap1",copy,sizeof(copy),size)==Read::error&&store.error()==ESP_ERR_NVS_TYPE_MISMATCH);type_fail=false;
  set_fail=true;assert(!store.write("ap1",bytes,sizeof(bytes)));set_fail=false;
  commit_fail=true;assert(!store.write("ap_try",bytes,sizeof(bytes)));assert(store.error()==ESP_FAIL);commit_fail=false;
  erase_fail=true;assert(!wifi_credentials::eraseVerified(store,"ap_try"));erase_fail=false;
  assert(wifi_credentials::eraseVerified(store,"ap_try"));assert(wifi_credentials::eraseVerified(store,"ap_try"));
  assert(records.size()==1&&records.count("ap1"));
 }
 assert(closes==1);puts("PASS actual NVS adapter: Wi-Fi-only namespace, missing/type/size/open/read/set/commit/erase failures, verified removal and uncertain-write propagation; no device access");
}
