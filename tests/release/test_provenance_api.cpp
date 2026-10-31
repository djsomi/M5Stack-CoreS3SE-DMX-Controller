#include "firmware_version.h"
#include <assert.h>
#include <string.h>
int main() {
    const auto& p = firmware_build::info();
    assert(!strcmp(p.version, DMX_FIRMWARE_VERSION));
    assert(strlen(p.commit) == 40 && strlen(p.source_sha256) == 64);
    assert(!strcmp(p.environment, "cores3se_unit"));
    assert(strstr(p.build_id, p.version) == p.build_id);
    assert((strstr(p.build_id, ".modified.") != nullptr) == p.modified);
}
