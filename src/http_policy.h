#pragma once
#include <stddef.h>
#include <stdint.h>
#include "web_assets.h"
namespace stage5 {
constexpr size_t max_header_value = 256;
constexpr size_t max_asset_bytes = 16384;
constexpr size_t max_rx_bytes = 2048;
constexpr size_t max_tx_bytes = 20480;
constexpr int64_t session_us = 3000000;
bool acceptsGzip(const char* value);
bool allowedHost(const char* host);
bool allowedOrigin(const char* origin);
struct Response { int status; const web_assets::Asset* asset; const char* cache; };
Response assetResponse(const char* uri, const char* encoding);
struct Budget {
    int64_t opened = 0;
    size_t received = 0, sent = 0;
    bool allow(int64_t now, size_t bytes, bool writing);
};
}
