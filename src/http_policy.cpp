#include "http_policy.h"
#include <string.h>
#include <ctype.h>
namespace stage5 {
namespace {
bool token(const char* a, const char* b, const char* end) {
    while (a < end && *b && tolower(static_cast<unsigned char>(*a)) == *b) { ++a; ++b; }
    return a == end && !*b;
}
void spaces(const char*& p) { while (*p == ' ' || *p == '\t') ++p; }
}
bool acceptsGzip(const char* value) {
    if (!value || strlen(value) > max_header_value) return false;
    int gzip = -1, star = -1;
    const char* p = value;
    while (*p) {
        spaces(p); const char* start = p;
        while (*p && *p != ';' && *p != ',' && *p != ' ' && *p != '\t') ++p;
        const char* end = p; spaces(p); int q = 1000;
        if (*p == ';') {
            ++p; spaces(p);
            if (tolower(static_cast<unsigned char>(*p)) != 'q') return false;
            ++p; if (*p != '=') return false; ++p;
            if (*p != '0' && *p != '1') return false;
            bool one = *p++ == '1'; q = one ? 1000 : 0;
            if (*p == '.') {
                ++p; int scale = 100, count = 0;
                while (isdigit(static_cast<unsigned char>(*p))) {
                    if (++count > 3 || (one && *p != '0')) return false;
                    if (!one) q += (*p - '0') * scale;
                    scale /= 10; ++p;
                }
                if (!count) return false;
            }
            spaces(p);
        }
        if (start == end || (*p && *p != ',')) return false;
        if (token(start, "gzip", end)) { if (gzip != -1) return false; gzip = q; }
        if (token(start, "*", end)) { if (star != -1) return false; star = q; }
        if (*p == ',') { ++p; if (!*p) return false; }
    }
    return (gzip >= 0 ? gzip : star) > 0;
}
bool allowedHost(const char* host) {
    return host && (!strcmp(host, "192.168.4.1") || !strcmp(host, "192.168.4.1:80"));
}
bool allowedOrigin(const char* origin) {
    return !origin || !*origin || !strcmp(origin, "http://192.168.4.1") || !strcmp(origin, "http://192.168.4.1:80");
}
Response assetResponse(const char* uri,const char* encoding) {
    const auto* asset=uri?web_assets::find(uri):nullptr;
    if(!asset) return {404,nullptr,"no-store"};
    if(!acceptsGzip(encoding)) return {406,nullptr,"no-store"};
    if(asset->size>max_asset_bytes) return {503,nullptr,"no-store"};
    return {200,asset,!strcmp(uri,"/")?"no-store":"public, max-age=31536000, immutable"};
}
bool Budget::allow(int64_t now, size_t bytes, bool writing) {
    if (now < opened || now - opened >= session_us) return false;
    size_t& used = writing ? sent : received;
    const size_t limit = writing ? max_tx_bytes : max_rx_bytes;
    if (used > limit || bytes > limit - used) return false;
    used += bytes; return true;
}
}
