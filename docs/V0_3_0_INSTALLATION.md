# v0.3.0 installation

Use the image matching your hardware:

- `CoreS3SE-DMX-Unit-v0.3.0.bin` — Unit DMX U183 + Battery Bottom; recommended.
- `CoreS3SE-DMX-Base-v0.3.0.bin` — DMX Base + external PSU; experimental.

Both include Wi-Fi. The device cannot automatically identify which accessory is
attached. Check the filename and the UNIT DMX / DMX BASE boot label.

## Before starting

Install Python and the esptool version used to prepare these files:

```powershell
python -m pip install esptool==4.5.1
Get-FileHash .\CoreS3SE-DMX-Unit-v0.3.0.bin -Algorithm SHA256
```

Compare the hash with `SHA256SUMS.txt`. Substitute the Base filename when needed.
Close serial monitors. Commands below use **COM8**; change it to your actual port.
They are manual instructions, not commands executed by this project at startup.

## v0.2.0 → v0.3.0: clean installation

**This erases presets, settings and Wi-Fi credentials.** A complete flash erase
is required when coming from v0.2.0. The new 16 MiB layout and current preset
storage replace the old installation; there is no data-preserving v0.2 migration.

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 460800 erase_flash
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x0 .\CoreS3SE-DMX-Unit-v0.3.0.bin
```

For Base use `CoreS3SE-DMX-Base-v0.3.0.bin` in the second command.
The headers already encode the verified ESP32-S3 / 16 MB / DIO / 80 MHz settings;
`keep` avoids rewriting them. The PlatformIO project uses its existing qio build
configuration; do not override the generated boot image's header by guessing.

Each factory image contains these verified components byte-for-byte:

| Component | Offset |
|---|---:|
| Bootloader | 0x0000 |
| Partition table | 0x8000 |
| OTA initialization | 0xE000 |
| Application | 0x10000 |

The merged file spans the NVS range `0x9000–0xDFFF` with 0xFF fill. **Writing it
can erase NVS even without a separate erase command.** It is not an update image.
The file ends after the application; it is not padded to the whole 16 MB flash.

## Existing current v0.3.0 → updated v0.3.0: retain data

The user physically verified a normal no-erase application update preserving
current-format v0.3.0 presets/settings. Use this only with the existing 16 MiB
layout and generation-backed preset format. Historical RC/pre-cleanup builds
are not covered merely because their version label contains v0.3.0.

Extract the matching application from the factory file, then write **only** that
application at 0x10000. Extraction is byte-for-byte and tested during packaging.

```powershell
python -c "from pathlib import Path; p=Path('CoreS3SE-DMX-Unit-v0.3.0.bin'); p.with_name(p.stem+'-app.bin').write_bytes(p.read_bytes()[0x10000:])"
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 .\CoreS3SE-DMX-Unit-v0.3.0-app.bin
```

For Base replace `Unit` with `Base` in both filenames. Do not run `erase_flash`,
do not write the complete factory image, and do not replace the partition table
or OTA metadata for this update. `BUILD_INFO.json` records the extracted app hash
and size. Back up presets with JSON Export before updating; Backup excludes
settings and Wi-Fi. These commands target the established app0 layout. No OTA
updater/app-slot switching is implemented; a manually altered boot slot is outside
this tested procedure.

## First boot and recovery

After a clean install, check empty presets and default settings. Read the SSID
and password on physical Settings, join the SoftAP, and open **http://192.168.4.1/**.
Factory credentials use `CoreS3SE-DMX-XXXX` / `espdmx-xxxx` derived from the AP MAC.
**Change the predictable password before use in untrusted environments.** Physical
approval protects credential changes; associated clients otherwise share control.
The Unit needs Battery Bottom for external Unit power. Local Wi-Fi ON/OFF remains
available if the radio is disabled; RESET WIFI takes effect after reboot.

Verify the hardware profile, physical controls, web UI, saved presets, Settings,
Restart/Power Off and another reboot. Keep the Backup tab open/active for transfers.

If normal upload fails, enter the board's ROM download mode and retry the matching
image/port. A factory reinstall resets data; do not describe it as data-preserving
recovery. Interrupted-upgrade recovery and Base electrical behavior need separate
acceptance. See [known issues](../KNOWN_ISSUES.md).

## Build the factory images from source

The existing PlatformIO hooks verify the pinned driver patches, embedded assets,
partition table and component headers. From the matching source checkout:

```powershell
pio run -e cores3se_unit -e cores3se_base
python -m esptool --chip esp32s3 merge_bin --flash_mode keep --flash_freq keep --flash_size keep --output CoreS3SE-DMX-Unit-v0.3.0.bin 0x0 .pio/build/cores3se_unit/bootloader.bin 0x8000 .pio/build/cores3se_unit/partitions.bin 0xE000 .pio/build/cores3se_unit/boot_app0.bin 0x10000 .pio/build/cores3se_unit/firmware.bin
python -m esptool --chip esp32s3 merge_bin --flash_mode keep --flash_freq keep --flash_size keep --output CoreS3SE-DMX-Base-v0.3.0.bin 0x0 .pio/build/cores3se_base/bootloader.bin 0x8000 .pio/build/cores3se_base/partitions.bin 0xE000 .pio/build/cores3se_base/boot_app0.bin 0x10000 .pio/build/cores3se_base/firmware.bin
```

Do not add a whole-flash padding option or mix profile components. Release preparation uses these same settings with a separate output directory to
preserve prior builds. Build timestamps and embedded commit identity may change
binary bytes; use the checksums supplied with the actual binaries.

## Source and release identity

`BUILD_INFO.json` records the actual source commit, source fingerprint, hardware
profile and SHA-256 checksums. Both production images must come from that committed
source with no modified firmware inputs. Do not reuse old RC images/checksums or
assume the earlier local v0.3.0 tag identifies the cleaned firmware. Tag correction
and hosted publication require explicit authorization and are not flashing steps.

Binary distribution includes LICENSE and third-party notices plus matching
source/build/relink materials. See the [source delivery guide](V0_3_0_LGPL_DELIVERY.md).
