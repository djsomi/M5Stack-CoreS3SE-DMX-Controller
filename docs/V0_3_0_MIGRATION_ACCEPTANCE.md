# v0.2.0 → v0.3.0-rc.1 hardware migration acceptance

## Simplified physical acceptance — user reported PASS

On 2026-10-30 the user reported that all steps of the simplified physical
v0.2.0 → v0.3.0-rc.1 migration test passed. The supplied procedure used the Unit
profile: destructive genuine v0.2.0 installation, creation/reboot verification of
P1–P8 and non-default settings, selective four-image RC upgrade excluding NVS,
retained data, application/web/Wi-Fi/maintenance checks and another reboot.
The user explicitly waived a mandatory full-flash backup for that controlled
test; loss of old configuration during its initial setup was expected.

This is user-reported functional migration acceptance for that procedure, not a
raw NVS/readback or interrupted-upgrade experiment. Separate Base migration,
preboot byte equality, power-cut recovery and upper-flash write/alias tests were
not separately reported. The original RC archives and their manifests/checksums
are preserved; their pre-test `pending` metadata records when they were prepared.

The more extensive forensic procedure below remains **not fully executed**.
Do not infer its additional measurements from the simplified acceptance or the
earlier Stage 4B live-table verification.

Use a controlled CoreS3SE. Before installing old firmware, privately back up the
current device: the genuine v0.2.0 FULL installation deliberately overwrites NVS.
This destructive baseline setup is separate from the proposed normal upgrade.

## 1. Establish genuine v0.2.0 and known data

Verify the old release's published checksums:

| Profile | Full-file SHA-256 |
|---|---|
| Unit | f89405c9875a46c4b2ce88e4e9713790788f6b07d8ce5d326dd0fbefee06bb39 |
| Base | e96f904c2b127e18aa423fa0733bc31499271b0225226cec0d67a8c6e3901aa5 |

Install that exact correct-profile v0.2.0 firmware on the test device. Confirm
its old table: NVS 0x9000/0x5000; otadata 0xe000/0x2000; app0 0x10000/0x330000;
app1 0x340000/0x330000. Record actual 16 MiB physical flash and 8 MiB old headers.

Populate **all eight presets**. A manageable deterministic pattern is zero for
all channels except probes **1, 2, 21, 64, 128, 256, 511, 512**. For preset slot
`s` (1..8), set each probe channel `c` to `(29*s+c) mod 256`. Start from a zero
universe and replace those same eight probes for each slot; save P1 through P8.
Record all eight full expected 512-channel vectors and occupied states. Do not
erase/clear a slot in this primary test. Test all-zero/cleared compatibility in
an additional run only after the all-eight-presets retention case passes.

Choose non-default brightness 4, timeout 2 minutes and UI volume 1. Reboot old
firmware and confirm every preset and setting is already persistent. A second
run should cover Always On and MUTE. Battery ETA is RAM-only, not persisted data.

## 2. Capture and verify a baseline backup

Enter ROM download mode. Privately read the full 16 MiB flash, NVS at
0x9000/0x5000 and partition sector 0x8000/0x1000. Read each twice or compare
against corresponding full-backup slices and record SHA-256 equality. Decode
the old table; record `dmxpreset/p1..p8`, `dmxmeta/cleared`, and the U8
`dmxui/bright`, `timeout`, `volume` values. Keep credentials/backups out of Git.

Record board, profile/accessory, power, OS, USB port, old tag/source commit,
candidate ZIP hash, candidate build_id/source fingerprint, and baseline values.

## 3. Selective candidate upgrade

Verify all candidate archive/internal checksums and the profile. Confirm the
generated manifest's image/sector ranges exclude `[0x9000,0xe000)`. Use the
bundle's four separate offset/image pairs with **no erase-all** and no flat merge.

For the preboot preservation test, use a validated download-mode procedure that
does not run the app between write and readback (for example the appropriate
esptool `--after no_reset` flow). Exercise those reset flags on this test board;
do not assume an automatic post-upload reboot still permits a preboot NVS hash.

Before running the app, read NVS and compare byte-for-byte with step 2. Read the
live table and verify it equals the entire generated partitions.bin. Confirm
16 MiB headers, app0 at 0x10000, app1 at 0x610000, both size 0x600000, and the
intended app0 OTA initialization. Record actual outputs, not just PASS labels.

## 4. Boot and retention

Boot the candidate and check its Unit/Base startup label. In web Status or
/status record version, exact base commit, modified flag, build_id, input hash,
environment and layout. Match the bundle manifest. The RC includes uncommitted
release-preparation changes; it must not claim a clean source tag.

Load **each P1–P8** and compare all expected channel values/occupancy. Verify
brightness 4, timeout 2 and volume 1; reboot/cold-start and repeat. After first
boot Wi-Fi adds NVS records, so compare legacy namespace contents rather than a
raw whole-partition hash. Test save/load/copy/clear only after recording retention.

Test grid, Precision/keypad, web faders/fine/numeric controls, multi-client sync,
blackout zero output/restoration, sleep/wake, tones/MUTE and battery warning
override. Confirm continuous Unit DMX function. External capture is still needed
for actual RS-485 timing, complete frames and electrical coherence.

## 5. Network, recovery and reboot

Check new default AP initialization without changing legacy keys; join offline
HTTP/WS, validate reconnect and no mutation replay. Test Wi-Fi browser confirmation,
physical OK/CANCEL/60-second expiry, changed credential persistence and recovery.
Keep actual passwords private. Check Restart/Power Off and five-second CANCEL,
including the corrected deferred-maintenance navigation cases. Observe whether
automatic upload/reset unexpectedly enters a maintenance countdown; do not assume
the cause of the earlier deferred observation.

On the controlled unit, test failed boot/interrupted serial migration and ROM
recovery. Retry the correct sparse bundle or explicitly restore the matching
old four images/private backup. Verify retained data again. Do not use a public
FULL as a preservation rollback. Record that the migration is not power-cut atomic.

Repeat on Base hardware/external PSU separately and keep its experimental label.
Do not infer Base or recovery acceptance from a Unit-only result.

## Acceptance record

For every step record PASS/FAIL, evidence/backup hash, candidate ZIP/build ID,
hardware/profile and anomalies. Sign off separately: preboot NVS equality,
all-eight preset vectors, settings/reboot retention, new table/headers,
application/network regression, and recovery. The simplified Unit functional
migration is accepted as recorded above. These
additional forensic measurements, separate Base migration and interrupted-upgrade
recovery remain unvalidated; do not infer that they passed from Unit acceptance.
