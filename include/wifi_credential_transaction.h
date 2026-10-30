#pragma once
#include "wifi_configuration.h"
#include <stddef.h>

// One durable undo journal. Until journal erasure commits, boot restores the exact
// previous ap1 bytes (including legacy v1). Never promotes an interrupted candidate.
namespace wifi_credentials {
struct Store {
    enum class Read { found, missing, error };
    virtual Read read(const char* key,void* out,size_t capacity,size_t& length)=0;
    virtual bool write(const char* key,const void* data,size_t length)=0;
    virtual bool remove(const char* key)=0;
    virtual ~Store()=default;
};
struct Journal {
    uint32_t magic=0x57494649,version=1,previous_size=0;
    uint8_t previous[108]{};
    softap_policy::Credentials candidate{};
    uint32_t checksum=0;
};
static_assert(sizeof(Journal)==232,"Stable Wi-Fi-only undo journal");
inline uint32_t checksum(const Journal& j) {
    const auto* p=reinterpret_cast<const uint8_t*>(&j);uint32_t hash=2166136261U;
    for(size_t n=0;n<offsetof(Journal,checksum);++n){hash^=p[n];hash*=16777619U;}return hash;
}
inline bool verify(Store& s,const char* key,const void* data,size_t length) {
    uint8_t copy[sizeof(Journal)]{};size_t size=0;
    const bool ok=s.read(key,copy,sizeof(copy),size)==Store::Read::found&&size==length&&!memcmp(copy,data,length);
    WifiConfiguration::erase(copy,sizeof(copy));return ok;
}
inline bool eraseVerified(Store& s,const char* key) {
    if(!s.remove(key))return false;
    uint8_t data[sizeof(Journal)]{};size_t size=0;const auto result=s.read(key,data,sizeof(data),size);
    WifiConfiguration::erase(data,sizeof(data));return result==Store::Read::missing;
}
inline bool valid(const Journal& j,const uint8_t mac[6]) {
    if(j.magic!=0x57494649||j.version!=1||j.checksum!=checksum(j)||
       (j.previous_size!=64&&j.previous_size!=108))return false;
    softap_policy::Credentials c{};
    bool ok=softap_policy::select(j.previous,j.previous_size,true,false,mac,c)==softap_policy::Decision::keep;
    ok=ok&&j.candidate.origin==softap_policy::Origin::custom&&
        softap_policy::select(&j.candidate,sizeof(j.candidate),true,false,mac,c)==softap_policy::Decision::keep;
    WifiConfiguration::erase(&c,sizeof(c));return ok;
}
inline bool rollback(Store& s,const Journal& j) {
    return s.write("ap1",j.previous,j.previous_size)&&verify(s,"ap1",j.previous,j.previous_size)&&eraseVerified(s,"ap_try");
}
inline bool recover(Store& s,const uint8_t mac[6]) {
    Journal j;size_t size=0;const auto result=s.read("ap_try",&j,sizeof(j),size);
    if(result==Store::Read::missing)return true;
    const bool ok=result==Store::Read::found&&size==sizeof(j)&&valid(j,mac)&&rollback(s,j);
    WifiConfiguration::erase(&j,sizeof(j));return ok;
}
inline bool stage(Store& s,const softap_policy::Credentials& candidate,const uint8_t mac[6],Journal& j) {
    j=Journal{};size_t size=0;
    if(s.read("ap1",j.previous,sizeof(j.previous),size)!=Store::Read::found)return false;
    j.previous_size=uint32_t(size);j.candidate=candidate;j.checksum=checksum(j);
    return valid(j,mac)&&s.write("ap_try",&j,sizeof(j))&&verify(s,"ap_try",&j,sizeof(j));
}
inline bool promote(Store& s,const Journal& j) {
    return s.write("ap1",&j.candidate,sizeof(j.candidate))&&verify(s,"ap1",&j.candidate,sizeof(j.candidate))&&eraseVerified(s,"ap_try");
}
struct Radio {
    virtual bool notifyAndRetire()=0;
    virtual bool apply(const softap_policy::Credentials&)=0;
    virtual bool ready()=0;
    virtual void committed(const softap_policy::Credentials&)=0;
    virtual ~Radio()=default;
};
// Worker-only coordinator, shared verbatim by firmware and fault-injection tests.
inline WifiConfiguration::Work execute(Store& store,Radio& radio,const softap_policy::Credentials& candidate,const uint8_t mac[6]) {
    using Work=WifiConfiguration::Work;
    Journal journal;softap_policy::Credentials previous{};
    Work result=Work::storage_error;bool staged=false,touched=false,healthy=true;
    do {
        if(!stage(store,candidate,mac,journal)){healthy=recover(store,mac);break;}
        staged=true;
        if(softap_policy::select(journal.previous,journal.previous_size,true,false,mac,previous)!=softap_policy::Decision::keep){healthy=false;break;}
        result=Work::radio_error;
        if(!radio.notifyAndRetire())break;
        touched=true;
        if(!radio.apply(candidate)||!radio.ready())break;
        result=Work::storage_error;
        if(!promote(store,journal))break;
        radio.committed(candidate);result=Work::done;
    } while(false);
    if(result!=Work::done&&staged) {
        const bool saved=rollback(store,journal);
        const bool restored=!touched||(radio.apply(previous)&&radio.ready());
        healthy=healthy&&saved&&restored;
    }
    WifiConfiguration::erase(&journal,sizeof(journal));WifiConfiguration::erase(&previous,sizeof(previous));
    return healthy?result:Work::rollback_error;
}
}
