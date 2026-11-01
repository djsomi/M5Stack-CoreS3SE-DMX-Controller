# Third-party notices — v0.3.0

This project extends M5Stack's DMX512Tools example. The original **Copyright
(c) 2022 M5Stack** and complete MIT notice are retained in [LICENSE](LICENSE),
alongside the existing additional-project notice. MIT does not replace the
licenses of linked dependencies.

The complete, unedited notice/license texts are in
[licenses/THIRD_PARTY_LICENSES.txt](licenses/THIRD_PARTY_LICENSES.txt).
[licenses/manifest.json](licenses/manifest.json) records each text's source,
Git blob identity where available, byte range and SHA-256. Only UTF-8/LF line
ending normalization is applied. No upstream wording is rewritten.

## Audited distribution baseline

- Firmware source: `59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`, annotated `v0.3.0`.
- PlatformIO: `espressif32@6.7.0`; Arduino package `3.20016.0` (Arduino 2.0.16),
  installed ESP32-S3 SDK reporting ESP-IDF 4.4.7.
- M5Unified **0.2.21**; M5GFX **0.2.28**; esp_dmx metadata **2.0.2**, upstream
  tag **v2.02**, with the repository's reproducible compatibility/transport patches.
- Xtensa ESP32-S3 toolchain package **8.4.0+2021r2-patch5**.

Unit and Base production maps were examined after garbage collection. Archive
extraction alone is not proof of inclusion: an extracted object may subsequently
be discarded. `retained_objects` records nonzero addressed sections in each
final link map; firmware hashes tie that evidence to the audited binaries.
This is a component-level inventory, not a claim that every file, algorithm or
hardware feature in an SDK archive is active. A dependency/configuration change
requires a new inventory review.

## Included components

| Component | Exact version/source | Notice and inclusion evidence |
|---|---|---|
| Original DMX512Tools | M5Stack upstream LICENSE, blob `8048a38de03e5d6be2f60882a253199acde3ac00` | MIT; derived project source. No claim that this identifies the original application's historical fork commit. |
| M5Unified | 0.2.21 | MIT, Copyright (c) 2021 M5Stack; retained initialization, touch, power, speaker and supporting objects. |
| M5GFX | 0.2.28 | Top-level MIT, Copyright (c) 2021 M5Stack; graphics/sprite/touch objects retained. This is **not** an MIT-only dependency. |
| LovyanGFX-derived code | Vendored in M5GFX 0.2.28; independent vendored revision not asserted | Referenced upstream composite notice includes LovyanGFX/TFT_eSPI FreeBSD/BSD terms and Adafruit notices. Text retrieved from official LovyanGFX tag 1.2.7, blob `5d70e99cfe1762f911db00f73d195a2cc6141fee`; this is the notice source, not a claimed M5GFX dependency version. |
| Adafruit classic font | M5GFX `glcdfont.h`; identical notice in `Fonts/GFXFF/license.txt` | BSD notice, Copyright (c) 2012 Adafruit Industries; Font0 retained. |
| Other selected fonts | M5GFX AsciiFont8x16, Font2, Font4, Font7 | Selected font objects retained; package/LovyanGFX notices preserved. Ascii8x16 has no separate per-file author/license notice; no additional glyph attribution is invented. |
| `result.hpp` | Vendored M5GFX header | MIT, Copyright (c) 2017-2021 Matthew Rodusek; included by graphics/I2C implementation. |
| esp_dmx | 2.0.2 / tag v2.02 | MIT, Copyright (c) 2020 Mitch Weisbrod; patched driver retained. Patch sources remain in the project. |
| Arduino-ESP32 | 2.0.16 / package 3.20016.0 | **LGPL-2.1-or-later** framework; source-file notices for retained core, Preferences, SPI and Wire units and base headers are also preserved. Individual files may carry other stated terms. |
| ESP-IDF / Wi-Fi / PHY | Installed 4.4.7 SDK | Apache-2.0 for Espressif code; upstream v4.4.7 LICENSE preserved. Official v4.4.7 Wi-Fi/PHY submodule LICENSE texts match that Apache text. Installed archive versions are not inferred from a current upstream branch. |
| FreeRTOS kernel | Installed headers: V10.4.3 | MIT, Copyright (C) 2020 Amazon.com, Inc. or its affiliates; complete installed header notice preserved. |
| lwIP | Espressif SDK fork; v4.4.7 submodule `a45be9e438f6cf9c54ec150581819c3b95d5af6b` used for COPYING | BSD-style notice; both COPYING and installed init-header notice preserved; TCP/IP objects retained. |
| wpa_supplicant/hostapd | ESP-IDF v4.4.7 component | BSD notice, Jouni Malinen and contributors; retained WPA/AP/crypto support. No separate upstream release number inferred. |
| fast-pbkdf2 | ESP-IDF v4.4.7 component | Joseph Birr-Pixton's dedication and full CC0-1.0 text; retained `fastpbkdf2.c.obj`. |
| Mbed TLS | Installed headers: 2.28.7 | Apache-2.0 OR GPL-2.0-or-later; full upstream dual-license text retained. Apache-2.0 is the selected alternative for this distribution. Crypto objects are retained for WPA; this does not mean HTTPS is enabled. |
| HTTP parser | ESP-IDF v4.4.7 `nghttp/port/http_parser.c` | Complete MIT-style Igor Sysoev/Joyent/Node notice preserved; parser object retained, not a claim that the nghttp2 HTTP/2 library is included. |
| Xtensa HAL | Installed ESP32-S3 SDK header | Cadence 1999-2015 MIT-style notice preserved; HAL/core support retained. |
| GCC runtime / libstdc++ | Bundled toolchain 8.4.0+2021r2-patch5 | GPLv3 plus **GCC Runtime Library Exception 3.1**, full texts preserved. Do not infer that the whole application is GPL solely from runtime linking. |
| newlib / libgloss | As bundled in that toolchain; independent version not asserted | Complete installed multi-source COPYING.NEWLIB/COPYING.LIBGLOSS notices preserved. They include notices for more source variants than this target uses. |

## Present in dependencies, not retained as firmware payload

The audited Unit/Base images do **not** retain the M5GFX PNG/miniz, TJpgDec JPEG,
QOI or QR-code implementation objects, nor IPA, efont CN/JA/KR/TW or GFX
FreeFonts data. Their presence in an archive or source directory is not treated
as redistribution in these images. IPA has its own IPA Font License 1.0 and
efont its own BSD-style notice; neither is covered merely by M5GFX's MIT file.
Review and add their full applicable notices before enabling them. Do not infer
individual FreeFonts licensing from the directory's Adafruit code notice.

ESP32-S3 ROM miniz absolute symbols are ROM references, not retained M5GFX miniz
payload. Bluetooth memory-release glue is retained; no Bluetooth control feature
is enabled. Filesystem partitions do not imply a linked filesystem, and TinyUSB,
desktop compiler/build executables and browser-test tools are not firmware ZIP
payload. Browser assets use project code/system fonts, not downloaded npm fonts
or a bundled third-party JavaScript framework.

## Binary distribution and corresponding source

The four addressed firmware images are statically linked. **Notices alone do
not satisfy every distribution term**, notably Arduino's LGPL. Before hosted
binary publication, provide the applicable corresponding framework source and
the application source/object materials and build instructions needed to modify
the LGPL library and relink, or another distribution method permitted by the
actual license. Preserve the exact source revision, dependency versions and
patches; do not describe an upstream link alone as a completed source offer.
LGPL section 6 and the GCC exception are included verbatim for the distributor
to apply to the chosen delivery method. No hosted source offer or relink-source
bundle is created by this notice-packaging step.

Future Unit/Base ZIPs include LICENSE, this inventory, the text collection and
its machine-readable manifest. All are covered by internal SHA-256 checksums.
Notice packaging does not authorize relabelling an old RC or changing firmware
identity. See `docs/V0_3_0_PUBLICATION_READINESS.md` in the source repository.
