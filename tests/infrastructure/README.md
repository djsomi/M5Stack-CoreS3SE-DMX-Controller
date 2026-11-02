# Stage 4B infrastructure verification

No device is accessed by these tests or the packaging tool.

```powershell
python scripts/web_asset_pipeline.py
python tests/infrastructure/test_infrastructure.py
pio run -e cores3se_unit -e cores3se_base -e cores3se_unit_diagnostics -e cores3se_base_diagnostics
python scripts/package_firmware.py --env cores3se_unit --output dist/NEW-UNIT-BUNDLE
```

Asset regeneration requires Python 3.11.7 and zlib 1.2.13:

```powershell
python scripts/web_asset_pipeline.py --generate
```

Ordinary builds verify the checked bundle without needing that regeneration
toolchain or Node. Tests use the installed framework partition generator under
`PLATFORMIO_CORE_DIR` (default `~/.platformio`). New tests deliberately corrupt
temporary copies only. Keep generated `.gz` files and `manifest.json` with sources
when preparing a future commit/archive; they are required build inputs.

To test the generated C++ API with any C++11 host compiler, after a Unit build:

```powershell
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude .pio/build/cores3se_unit/generated/web_assets.cpp tests/infrastructure/test_asset_api.cpp -o dist/asset-api-tests
```

Run the resulting executable. Existing Controller/Dispatcher/transport tests
remain required. The post-build hook also validates real images, their SHA256
and segment checksums, exact layout, source/asset identity and linker retention.
See `docs/TESTING.md` for fresh-build evidence and the deliberately
separate, unexecuted device migration/rollback procedure.
