#include "status_ssid.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main() {
    char out[67];
    const auto encode=[&](const char* p,size_t n){return status_telemetry::ssidJson(reinterpret_cast<const uint8_t*>(p),n,out);};
    assert(encode("Actual AP",9) && !strcmp(out,"\"Actual AP\""));
    assert(encode("A\"B\\C",5) && !strcmp(out,"\"A\\\"B\\\\C\""));
    char maximum[32];memset(maximum,'"',sizeof(maximum));
    assert(encode(maximum,32) && strlen(out)==66 && out[66]==0);
    assert(!encode(nullptr,1) && !out[0]);assert(!encode("",0) && !out[0]);
    assert(!encode(maximum,33) && !out[0]);assert(!encode("A\n",2) && !out[0]);
    const char unsupported[]={char(0xc3),char(0xa9)};
    assert(!encode(unsupported,2) && !out[0]);
    puts("PASS actual bounded SSID encoder: name/quotes/backslashes/32 bytes/empty/invalid/unsupported; no password serialization");
}
