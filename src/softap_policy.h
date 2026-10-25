#pragma once
#include "softap_config.h"
#include <stddef.h>
#include <stdint.h>
namespace softap_policy {
enum class Origin : uint32_t { legacy=0, factory=1, custom=2 };
struct LegacyCredentials { uint32_t version; char ssid[33]; char pass[25]; };
struct Credentials { uint32_t version; Origin origin; char ssid[33]; char pass[64]; };
static_assert(sizeof(LegacyCredentials)==64,"Preserve original NVS blob layout");
static_assert(sizeof(Credentials)==108,"Version 2 NVS blob layout");
enum class Decision { invalid, keep, write_factory };
bool validPassword(const char* value,size_t capacity);
void factory(const uint8_t ap_mac[6],Credentials& output);
Decision select(const void* blob,size_t size,bool exists,bool reset_requested,
                const uint8_t ap_mac[6],Credentials& output);

// Fixed MAC set: duplicate connect/disconnect events cannot inflate/underflow it.
class Stations {
public:
    bool connect(const uint8_t mac[6]);
    bool disconnect(const uint8_t mac[6]);
    void clear();
    unsigned count() const {return count_;}
private:
    uint8_t macs_[DMX_WIFI_MAX_STATIONS][6]{};
    unsigned count_=0;
};
}
