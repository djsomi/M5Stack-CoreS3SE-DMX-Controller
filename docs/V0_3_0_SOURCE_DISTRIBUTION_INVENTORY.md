# v0.3.0 source-distribution inventory and validation

This is the unchanged dependency/source-material inventory used by the cleaned
v0.3.0 configuration. The final application source is the commit recorded in
BUILD_INFO.json. Older application archive checksums and relink records below
are historical validation evidence, not the final cleaned application package.
No old archive or tag is silently relabeled.

The reviewed manifest is [licenses/source_materials/manifest.json](../licenses/source_materials/manifest.json). It inventories **112 component groups, 2,818 dependency files, 1,316 exact exclusions and 37 notice/source items**. Every retained dependency file has its exact path, length, SHA-256, component and classification. All exclusions list the original hash and a reason. They are omissions from new archives, not deletions from installed dependencies or historical archives.

## Review basis and scope

This review uses the exact installed sources, original per-file statements, pinned upstream license/NOTICE files and declared component versions. Tests prove inventory integrity, reconstruction and relinking; they do not independently establish legal rights or copyright ownership. Component license expressions summarize mixed coverage, not a claim that every file carries every listed license. Original file declarations remain authoritative. No license text or copyright holder is invented. Text copies normalize UTF-8/LF only; original source notices remain in place.

The artifact provenance for the SDK is **platformio/framework-arduinoespressif32 3.20016.0**, with exact per-file hashes and `IDF_VER=v4.4.7-dirty`. An exact clean SDK source commit is not asserted. ESP-IDF v4.4.7 and its recorded submodules supply explicitly labeled license-reference evidence; their tags are not silently assigned to opaque SDK archives. Independent revisions absent from a package remain unrecorded. Compiler executables are not redistributed.

## Distribution classifications

| Classification | Material |
|---|---|
| Required for rebuilding/relinking | Frozen application/build inputs; complete Arduino core; Preferences/SPI/Wire; M5Unified/M5GFX/esp_dmx sources, headers and compiled font/codec inputs. |
| Required supporting build material | Unchanged PIO 6.7.0 recipes/board; S3 SDK headers, common archives/linker files, qio_qspi configuration; four externally acquired tools locked by version/hash; preferred font-source supplement. |
| Optional/unused SDK material | Omitted examples, tests, other-board/IDE/debug files, desktop preview images and inactive memory modes. Runtime-unused SDK archives named by `-l` remain supporting material. |
| Documentation/license material | Original license/copyright files, source-file notices, upstream reference notices, catalog and build/relink guide. |

The retained SDK is deliberately conservative: no vendor build recipe is rewritten to remove `-l` entries. Missing even an unretained archive would break the unchanged linker invocation. Further source minimization through patched recipes is outside this frozen-firmware delivery.

## License findings

- **Asio:** installed `ASIO_VERSION=101401` identifies 1.14.1. Its actual headers declare Boost Software License 1.0, with Christopher M. Kohlhoff and other original copyright statements. Full official Boost text and the pinned Asio copy are both included; it is not labeled MIT. [Official Boost text](https://www.boost.org/LICENSE_1_0.txt).
- **Arduino:** full used LGPL-2.1-or-later core/source/build interfaces plus original Apache/public-domain exceptions are retained. Source/relink delivery follows the [LGPL delivery guide](V0_3_0_LGPL_DELIVERY.md), using source rather than final ELF alone.
- **SDK:** Apache, BSD, MIT, ISC and specific vendor/public-domain notices are preserved. Explicit BSD-2-Clause-FreeBSD/Apache endian and ISC SDMMC declarations are accounted for. The OpenSSL-named SDK component is Espressif compatibility code; an upstream OpenSSL implementation/version is not invented.
- **Additional SDK notices:** TinyCBOR, Unity/CMock, libcoap, cJSON, protobuf-c, libsodium, SPIFFS, LittleFS wrapper/core, camera/DL, console libraries, NimBLE NOTICE and its referenced BSD/TinyCrypt ports are covered. Newlib/GCC runtime, FreeRTOS, lwIP, WPA, Mbed TLS and Xtensa terms remain in the audited base collection.
- **Fonts:** M5GFX is not MIT-only. Adafruit/classic font, TomThumb, efont BSD, IPA 1.0 and GNU FreeFont data have separate entries. The official Adafruit conversion script identifies GNU FreeFont as the outline source family. Full GPLv3/font exception, authors, actual bitmap source and a complete official FreeFont 20120503 source supplement are provided. That supplement is not asserted to identify the unrecorded revision of M5GFX’s vendor bitmaps. These fonts are not retained in the accepted firmware according to the audited link maps. No font exception is used to excuse missing source delivery.
- **Codecs:** miniz 1.15’s actual public-domain/Unlicense footer, PNGLE’s kikuchan MIT text, ChaN TJpgDec R0.01c’s original permissive terms and Richard Moore’s QR-code MIT notice are retained. The M5 QOI implementation has no separate per-file declaration; its package MIT terms are recorded without inventing a third-party author/version.

Standalone inherited-license checks caught IPA, efont, libb64 and the exact PlatformIO license in the source payload but absent from the separate notice set. They are now explicit notice entries. Missing/modified licenses, removed inventory identities, false Asio MIT labeling and inherited notices omitted from delivery are regression-tested.

## Exact exclusions

The complete 1,316-path list is `excluded_files` in the manifest. Aggregate reasons:

| Reason | Files | Original bytes |
|---|---:|---:|
| Arduino IDE/other-target metadata; not an input to the frozen PIO build | 3 | 1,345,295 |
| Dependency example/media/documentation outside required source/license material | 69 | 2,937,503 |
| Inactive memory mode; both frozen profiles use qio_qspi | 35 | 34,314,873 |
| M5GFX desktop-only preview images; included only in the non-ESP_PLATFORM branch | 9 | 27,067,587 |
| Other-board PlatformIO definition | 230 | 205,283 |
| Other-board variant; frozen board uses esp32s3 | 327 | 5,668,202 |
| Other-target/IDE/upload/debug tool; unchanged PIO build uses the retained scripts | 38 | 12,992,590 |
| Platform examples/CI/monitor support; not used by compilation/relink | 1 | 5,725 |
| SDK test fixtures; not part of either build's includes | 27 | 176,356 |
| Unused Arduino library/example; frozen LDF uses only Preferences/SPI/Wire | 577 | 2,528,149 |

Total excluded: **1,316 files / 87,241,563 bytes**. Firmware and dependency source text is never edited to make exclusions work.

## Historical local validation archives

| Archive | Bytes | Members | SHA-256 |
|---|---:|---:|---|
| `LOCAL-VALIDATION-v0.3.0-SOURCE-REVIEWED.zip` | 108,769,054 | 2,868 | `89a8736e103eee33b838a5ef7dec1ceb4a3f531acaa7071449c5050e97a2a252` |
| `LOCAL-VALIDATION-v0.3.0-NOTICES-REVIEWED-TEXT.zip` | 491,518 | 40 | `27321232d1ca82efee5e639e837fc84f13658c76973b5d1f2d2e38d0514d2535` |

The source archive expands to 315,947,958 bytes and includes the minimal frozen Git pack, core/SDK/dependency files, manifests, reconstruction tools, notices and build/relink guide. The source supplement occurs once, inside the source ZIP. The notice ZIP has every license text/catalog but references that paired source supplement instead of duplicating 6.5 MB. The pair verifier checks the supplement’s exact bytes/hash in the already verified source tree; missing source or notice text is rejected. All older RC/source/notice archives remain unchanged.

Archives contain no publisher history/other branches/stash, device flash/NVS backups, private logs or host compiler executables. The frozen source pack remains one shallow commit, one original annotated tag and required trees/build blobs. No padded FULL image is produced.

## Rebuild and relink evidence

The reviewed ZIP was verified, extracted and reconstructed into fresh private project/core directories. Tools were copied only after exact lockfile hash checks. Both production profiles passed clean non-verbose builds and the unchanged post-build layout/image/asset/provenance verifier. Arduino Print::printf was then modified only in that private core, both profiles rebuilt/relinked, and the retained marker verified in object, archive, symbol table and final ELF. Baseline ELFs contain no marker.

```text
pio run -d <reconstructed-project> -e cores3se_unit -e cores3se_base
```

| Profile | Baseline ELF SHA-256 | Modified ELF SHA-256 |
|---|---|---|
| cores3se_base | `d14024c29310aa439873507d3bd95c75b48e4aea0df7dd127899c9245638b20c` | `931bbbc43377bf68bf976507bb6f06a1b72494b943a107e35433b00c29e8f13e` |
| cores3se_unit | `cc46efc055e6b7bf8354f0543dfa9e0ae3cafd743419e39466f2c8fb6822e199` | `17f87ef0744d2c0a1260f8a11f0dd06b88bcf61343c28e3a95f2e82ecde11461` |

Exact commands, baseline/modified build logs, symbol evidence and `relink-evidence.json` are under the ignored local `dist/publication-inventory-20261101/relink-proof-reviewed/`. They are not replacement production images. The SDK’s actual prebuilt bytes are delivered; this is not a claim to reconstruct every opaque SDK library from a clean IDF source tag or to reproduce timestamped accepted images bit-for-bit.

## Validation and remaining publication steps

Final targeted Python suites: 18 publication, 9 material inventory, 11 notice, 11 release, 6 infrastructure and 15 composer/asset cases. Frontend, network/SoftAP/WebSocket, startup, canvas, telemetry, Dispatcher integration, preset archive, Wi-Fi-power and navigation regressions also pass. Real ESP-IDF NVS and reproducible driver patch-state regressions pass. Host-only tests never access device data. Final report/checksums preserve exact accepted image hashes, original tag and stash; Git whitespace checks pass.

No known missing notice text remains in the reviewed scope. This conclusion is based on the source/declaration review and captured upstream material, not solely test success. Unrecorded independent component revisions remain a documented provenance limit; exact package/file identities are provided.

Publication still requires an authorized, reviewed, clean follow-up publication commit and equivalent downloadable source/build/notices beside the firmware. The frozen firmware tag/commit stays unchanged. Dual provenance records separate firmware source/fingerprint and publication commit/input digest plus all binary/source/notice checksums. Current uncommitted publication commit is null; the stable-publication commit gate rejects it. Unit/Base sparse offsets and NVS exclusions remain unchanged; Base is experimental and separate Base migration/interrupted-upgrade recovery remain unverified.

A regression command initially reused the prior temporary check-log directory. Subsequent logs are isolated in the current audit directory. Hardware investigation logs and release/RC archives were not touched; no claim is made that overwritten temporary regression logs were preserved.

## Complete retained component inventory

The following component-level expressions summarize coverage; the manifest and original individual file notices supply the exact scope, provenance and full texts.

| Component | Classification | Version/provenance label | License coverage | Files |
|---|---|---|---|---:|
| adafruit-classic-font | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | LicenseRef-Adafruit-BSD | 2 |
| arduino-esp32 | required-for-rebuilding-relinking | 2.0.16 / PlatformIO 3.20016.0 | LGPL-2.1-or-later AND Apache-2.0 | 115 |
| arduino-preferences | required-for-rebuilding-relinking | 2.0.0 in Arduino package 3.20016.0 | Apache-2.0 | 4 |
| efont | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | BSD-3-Clause | 9 |
| esp-dmx | required-for-rebuilding-relinking | 2.0.2 / upstream v2.02 + repository patches | MIT | 18 |
| gnu-freefont-bitmaps | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | GPL-3.0-or-later WITH Font-exception-2.0 | 48 |
| ipa-font | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | IPA | 5 |
| libb64 | required-for-rebuilding-relinking | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | LicenseRef-libb64-public-domain | 6 |
| m5gfx | required-for-rebuilding-relinking | 0.2.28 | MIT AND BSD-2-Clause | 171 |
| m5unified | required-for-rebuilding-relinking | 0.2.21 | MIT | 89 |
| miniz | required-for-rebuilding-relinking | 1.15 (source header) | Unlicense | 2 |
| platformio-espressif32 | required-supporting-build-material | 6.7.0 | Apache-2.0 | 14 |
| pngle | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | MIT | 2 |
| qoi-m5gfx | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | MIT | 2 |
| qrcode-richard-moore | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | MIT | 2 |
| result | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | MIT | 1 |
| sdk-app_trace | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-app_update | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-arduino_tinyusb | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT AND BSD-3-Clause | 90 |
| sdk-asio | required-supporting-build-material | 1.14.1 (ASIO_VERSION 101401) | BSL-1.0 AND Apache-2.0 | 420 |
| sdk-bootloader_support | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 15 |
| sdk-bt | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 AND BSD-3-Clause | 138 |
| sdk-build-config | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 16 |
| sdk-cbor | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 2 |
| sdk-cmock | required-supporting-build-material | 2.5.2 (installed cmock.h) | MIT | 3 |
| sdk-coap | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-2-Clause AND MIT AND Apache-2.0 | 45 |
| sdk-console | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-3-Clause AND BSD-2-Clause AND Apache-2.0 | 5 |
| sdk-cxx | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 1 |
| sdk-driver | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 AND ISC | 43 |
| sdk-efuse | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-esp-dl | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 69 |
| sdk-esp-tls | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 7 |
| sdk-esp32-camera | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 AND MIT AND LicenseRef-ChaN-TJpgDec | 5 |
| sdk-esp_adc_cal | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_common | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-esp_diagnostics | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 6 |
| sdk-esp_eth | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 7 |
| sdk-esp_event | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-esp_gdbstub | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-esp_hid | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-esp_http_client | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_http_server | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-esp_https_ota | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_https_server | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_hw_support | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 78 |
| sdk-esp_insights | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_ipc | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-esp_lcd | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-esp_littlefs | required-supporting-build-material | 1.14.1 (ESP_LITTLEFS_VERSION_NUMBER) | MIT AND BSD-3-Clause | 2 |
| sdk-esp_local_ctrl | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_netif | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-esp_phy | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-esp_pm | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-esp_rainmaker | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 12 |
| sdk-esp_ringbuf | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_rom | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 AND LicenseRef-Individual-ROM-Header-Notices | 140 |
| sdk-esp_schedule | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_serial_slave_link | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-esp_system | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 25 |
| sdk-esp_timer | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-esp_websocket_client | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-esp_wifi | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 27 |
| sdk-espcoredump | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-espressif__esp-dsp | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 71 |
| sdk-espressif__esp_secure_cert_mgr | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-expat | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 16 |
| sdk-fatfs | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | LicenseRef-FatFs AND Apache-2.0 | 10 |
| sdk-fb_gfx | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-freemodbus | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 AND BSD-3-Clause | 5 |
| sdk-freertos | required-supporting-build-material | 10.4.3 (installed FreeRTOS.h) | MIT AND Apache-2.0 | 32 |
| sdk-gpio_button | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-hal | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 130 |
| sdk-heap | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-3-Clause AND Apache-2.0 | 8 |
| sdk-ieee802154 | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-jsmn | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 2 |
| sdk-json | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 3 |
| sdk-json_generator | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-json_parser | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT AND Apache-2.0 | 5 |
| sdk-libsodium | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | ISC | 72 |
| sdk-log | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-lwip | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-3-Clause AND MIT AND Apache-2.0 | 182 |
| sdk-mbedtls | required-supporting-build-material | 2.28.7 (installed version.h) | Apache-2.0 | 126 |
| sdk-mdns | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-mqtt | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 3 |
| sdk-newlib | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | LicenseRef-newlib-multilicense AND BSD-2-Clause-FreeBSD AND Apache-2.0 | 21 |
| sdk-nghttp | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 4 |
| sdk-nvs_flash | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-openssl | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 19 |
| sdk-perfmon | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-protobuf-c | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-2-Clause AND Apache-2.0 | 16 |
| sdk-protocomm | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 8 |
| sdk-pthread | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-qrcode | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-rmaker_common | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 8 |
| sdk-rtc_store | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-sdmmc | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-soc | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 162 |
| sdk-spi_flash | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 19 |
| sdk-spiffs | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT AND Apache-2.0 | 3 |
| sdk-tcp_transport | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 5 |
| sdk-tcpip_adapter | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 4 |
| sdk-ulp | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 8 |
| sdk-unity | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT | 7 |
| sdk-usb | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 6 |
| sdk-vfs | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 9 |
| sdk-wear_levelling | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 2 |
| sdk-wifi_provisioning | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | Apache-2.0 | 7 |
| sdk-wpa_supplicant | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | BSD-3-Clause AND Apache-2.0 | 14 |
| sdk-ws2812_led | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | LicenseRef-public-domain AND Apache-2.0 | 3 |
| sdk-xtensa | required-supporting-build-material | As vendored in framework-arduinoespressif32 3.20016.0; independent version not recorded | MIT AND Apache-2.0 | 38 |
| tjpgdec | required-for-rebuilding-relinking | R0.01c (source header) | LicenseRef-ChaN-TJpgDec | 2 |
| tomthumb | required-for-rebuilding-relinking | As vendored in M5GFX 0.2.28; independent version not recorded | BSD-3-Clause | 1 |
