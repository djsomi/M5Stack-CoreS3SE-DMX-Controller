#pragma once
#include <stddef.h>
#include <stdint.h>

// Immutable flash-backed data. Stage 4B does not start a server or allocate RAM.
namespace web_assets {
struct Asset {
    const char* path;
    const char* content_type;
    const char* content_encoding;
    const uint8_t* data;
    size_t size;
    size_t original_size;
    const char* sha256;
};
struct Bundle {
    const Asset* assets;
    size_t count;
    const char* sha256;
    const char* source_sha256;
    const char* profile;
    const char* layout;
};
const Bundle& bundle();
const Asset* find(const char* path);
}
