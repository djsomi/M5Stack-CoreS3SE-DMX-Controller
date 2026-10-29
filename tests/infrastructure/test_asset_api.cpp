#include "web_assets.h"
#include <assert.h>
#include <string.h>

int main() {
    const auto& bundle = web_assets::bundle();
    assert(bundle.count == 4);
    unsigned scripts = 0;
    assert(strlen(bundle.sha256) == 64);
    assert(strlen(bundle.source_sha256) == 64);
    assert(strcmp(bundle.layout, "cores3se-16m-ota6-v1") == 0);
    assert(web_assets::find(nullptr) == nullptr);
    assert(web_assets::find("/missing") == nullptr);
    for (size_t i = 0; i < bundle.count; ++i) {
        const auto& asset = bundle.assets[i];
        assert(web_assets::find(asset.path) == &asset);
        assert(strcmp(asset.content_encoding, "gzip") == 0);
        assert(asset.size > 18 && asset.original_size > 0);
        assert(asset.data[0] == 0x1f && asset.data[1] == 0x8b);
        assert(strlen(asset.sha256) == 64);
        if (strcmp(asset.content_type, "text/javascript; charset=utf-8") == 0) {
            assert(strncmp(asset.path, "/core.", 6) == 0 || strncmp(asset.path, "/ui.", 4) == 0);
            ++scripts;
        }
    }
    assert(strcmp(web_assets::find("/")->content_type, "text/html; charset=utf-8") == 0);
    assert(scripts == 2);
}
