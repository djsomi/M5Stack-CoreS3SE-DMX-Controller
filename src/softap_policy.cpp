#include "softap_policy.h"
#include <string.h>
#include <stdio.h>
namespace softap_policy {
namespace {
bool text(const char* p,size_t capacity,size_t minimum,size_t maximum) {
    if(!p) return false;
    const auto* end=static_cast<const char*>(memchr(p,0,capacity));
    if(!end || size_t(end-p)<minimum || size_t(end-p)>maximum) return false;
    for(;p!=end;++p) if(static_cast<unsigned char>(*p)<32 || static_cast<unsigned char>(*p)>126) return false;
    return true;
}
}
bool validPassword(const char* p,size_t capacity) {return text(p,capacity,8,63);}
void factory(const uint8_t mac[6],Credentials& c) {
    memset(&c,0,sizeof(c));c.version=2;c.origin=Origin::factory;
    snprintf(c.ssid,sizeof(c.ssid),"CoreS3SE-DMX-%02X%02X",mac[4],mac[5]);
    snprintf(c.pass,sizeof(c.pass),"espdmx-%02x%02x",mac[4],mac[5]);
}
Decision select(const void* blob,size_t size,bool exists,bool reset,const uint8_t mac[6],Credentials& out) {
    memset(&out,0,sizeof(out));
    if(reset || !exists) {factory(mac,out);return Decision::write_factory;}
    if(!blob) return Decision::invalid;
    if(size==sizeof(LegacyCredentials)) {
        LegacyCredentials old;memcpy(&old,blob,sizeof(old));
        if(old.version!=1 || !text(old.ssid,sizeof(old.ssid),1,32) || !validPassword(old.pass,sizeof(old.pass))) return Decision::invalid;
        out.version=2;out.origin=Origin::legacy;
        memcpy(out.ssid,old.ssid,sizeof(old.ssid));memcpy(out.pass,old.pass,sizeof(old.pass));
        return Decision::keep; // No origin marker: NEVER automatically replace this blob.
    }
    if(size!=sizeof(Credentials)) return Decision::invalid;
    Credentials current;memcpy(&current,blob,sizeof(current));
    if(current.version!=2 || !text(current.ssid,sizeof(current.ssid),1,32) || !validPassword(current.pass,sizeof(current.pass))) return Decision::invalid;
    if(current.origin==Origin::factory) {
        Credentials expected;factory(mac,expected);
        if(strcmp(current.ssid,expected.ssid) || strcmp(current.pass,expected.pass)) return Decision::invalid;
    } else if(current.origin!=Origin::custom) return Decision::invalid;
    out=current;return Decision::keep;
}
bool Stations::connect(const uint8_t mac[6]) {
    if(!mac) return false;
    for(unsigned i=0;i<count_;++i) if(!memcmp(macs_[i],mac,6)) return false;
    if(count_==DMX_WIFI_MAX_STATIONS) return false;
    memcpy(macs_[count_++],mac,6);return true;
}
bool Stations::disconnect(const uint8_t mac[6]) {
    if(!mac) return false;
    for(unsigned i=0;i<count_;++i) if(!memcmp(macs_[i],mac,6)) {
        --count_;memcpy(macs_[i],macs_[count_],6);return true;
    }
    return false;
}
void Stations::clear() {count_=0;memset(macs_,0,sizeof(macs_));}
}
