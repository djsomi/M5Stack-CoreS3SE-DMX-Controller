#pragma once

#define DMX_FIRMWARE_VERSION "0.3.0"
#define DMX_TARGET_RELEASE "0.3.0"

// Generated immutable identity; no runtime Git, allocation or device-state writes.
namespace firmware_build {
struct Info {
    const char* version;
    const char* commit;
    const char* environment;
    const char* source_sha256;
    const char* build_id;
    bool modified;
};
const Info& info();
}
