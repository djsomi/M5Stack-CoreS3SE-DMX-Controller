# v0.3.0 stabilization — Phase 1 diagnostic isolation

The user closed the physical slow-drag investigation for this release cycle.
The captured raw-coordinate correction, touch ownership/identity/release checks,
boot-version presentation and accepted application behavior are retained.
Version remains `0.3.0-rc.1`. No new release bundle is prepared in this phase.

## Deferred limitation and hardware evidence

Slow continuous touchscreen fader movement may exhibit intermittent coordinate stalls originating at or before the touch-controller acquisition layer. Historical upstream code contains the same relevant acquisition mechanisms. No application-level cause was demonstrated. The behavior is not proven to have occurred on the original firmware.

The specific upstream DMX512Tools example state is commit
`8a3c0bcc7d83eceebe7acff932871444313207b1` (2024-06-18, CoreS3SE handling).
Preserved original sender/main/common files match it after newline normalization.
Official M5 touch processing and FT acquisition source predating that commit match
the installed relevant function implementations; the historical unpinned library
versions and physical upstream behavior are not independently established.

The user measured THDIFF at 0xA0 on boot and repeated no-touch STATUS reads,
then 0xC8 after touchscreen activity in the same boot. Exact cause remains unknown.
The installed driver performs a touch-associated 0xA4=0x00 write; its relationship
to the changed THDIFF or stutter is unproven. Startup A0 does not establish the
value throughout later gestures. The earlier A–B–A procedure cannot establish
stable comparison conditions from single readbacks and remains **suspended**.
Do not force 0xA0, adopt 0xC8 as a replacement baseline, or write any register as
release cleanup. Old investigation reports/procedures remain historical evidence.

## Explicit opt-in rules

All four established environments keep these flags at zero by default:

| Feature | Explicit requirements |
|---|---|
| Physical fader recorder | `DMX_HW_DIAGNOSTICS=1`, `DMX_FADER_DIAGNOSTICS=1` |
| Acquisition compile copies/recorder | Above plus `DMX_TOUCH_ACQUISITION_DIAGNOSTICS=1` |
| Read-only register snapshot | `DMX_HW_DIAGNOSTICS=1`, `DMX_TOUCH_CONFIG_DIAGNOSTICS=1` |
| THDIFF write experiment | Suspended; no flashable authorized environment is provided |

`DMX_HW_DIAGNOSTICS=1` alone retains the existing Stage 0 functionality and
useful network/transport diagnostics. It does not enable the above tools.
Fader and snapshot opt-ins do not enable acquisition copies or THDIFF.
Only explicit acquisition enablement installs the locked three-source middleware;
original M5Unified/M5GFX dependency files are never edited by it.

The header defaults `DMX_THDIFF_EXPERIMENT=0` and independently requires
`DMX_THDIFF_AUTHORIZED_INVESTIGATION=1` plus hardware/configuration diagnostics if
someone explicitly enables it. The PlatformIO hook additionally rejects THDIFF
enablement in **all four established environments**, even with authorization set.
A dedicated future configuration requires separate review/authorization; current
asset integration does not admit an unreviewed fifth environment. Experimental
source and simulated host tests are retained, not an operational write workflow.

Disabled hooks are no-ops, including initialization, serial RX, error/timeout and
maintenance/Power Off cleanup. No automatic THDIFF cleanup write executes in any
of the four established configurations. No sensor polling, smoothing, interpolation,
delay, bus initialization or production touch threshold is added or changed.

## Migration and release state

Simplified Unit v0.2.0 to RC migration has user-reported functional acceptance:
seeded presets/settings survived the selective upgrade and reboot, with network,
physical UI and maintenance functionality tested. Separate Base migration,
interrupted-upgrade recovery and additional forensic/preboot NVS measurements
remain unverified. Stage 4B live-table verification is separate historical evidence.
Base remains experimental; Unit acceptance does not certify it.

Original RC ZIPs, components and checksums are preserved. They predate the retained
raw-coordinate correction and are not replaced or silently promoted. No new RC
bundle, version change, upload, NVS access, register operation, staging, commit or
publication is part of this phase. Existing stash and unrelated history/logs remain.

## Validation

All thirteen automated groups passed: isolation, simulated THDIFF, read-only
snapshot, captured-fader/recorder, acquisition differential, integration/regression,
23 core host programs, frontend/live/navigation, ten browser suites, Settings,
Maintenance, Wi-Fi and release/archive validation. Test executables use mocks;
no actual USB device, controller register or NVS was accessed.

The new isolation suite links all four real disabled investigation modules without
any Arduino/M5/I2C backend in all four default configurations. It also verifies
that fader/snapshot enablement cannot install acquisition middleware, explicit
acquisition can install it, invalid dependencies are rejected, and THDIFF cannot
be enabled in any standard environment even with the separate authorization flag.
The C++ authorization guard is checked by an expected compile failure. Existing
host experiment tests still run only through an explicitly enabled simulated
configuration, preserving fault coverage without providing a flashable environment.

One new host fixture initially failed with an undefined simulated readRegister8:
the disabled-experiment case unnecessarily linked an independently enabled
snapshot implementation. The fixture now proves that disabled THDIFF links without
an I2C backend; the separate snapshot suite retains its actual-module/I2C-helper
coverage. No firmware defect, test-baseline relaxation or domain change was made.

All four PlatformIO builds passed on the same source fingerprint:

`ac043ae7a177547fef5727c29c0380f055e3bf6b3595f0718227742e322c6c0a`

Build ID: `0.3.0-rc.1+9fdf07ee0234.modified.ac043ae7a177`.

| Environment | Static RAM | Flash | RAM delta | Flash delta |
|---|---:|---:|---:|---:|
| cores3se_unit | 95,908 | 1,100,321 | 0 | 0 |
| cores3se_base | 95,908 | 1,100,273 | 0 | 0 |
| cores3se_unit_diagnostics | 98,628 | 1,104,909 | -25,024 | -47,408 |
| cores3se_base_diagnostics | 98,628 | 1,104,881 | -25,024 | -47,408 |

Deltas compare the incoming THDIFF-investigation build, not committed Stage 6B.5C.
ELF inspection finds no thdiff, physical_fader, touch_acquisition or
touch_configuration implementation in any of the four builds. APPLY/RESTORE,
experiment events and touch trace markers are absent from every firmware image.
The captured raw-coordinate helper is present in all four; Stage 0 begin is
present only in the two diagnostics ELFs. Diagnostics recompiled the original
M5GFX/M5Unified touch sources, with no acquisition middleware post-action. Old
compile-copy evidence was preserved; its mere presence under a prior build path
does not mean it is linked into these builds.

Two fresh asset generations match each other and existing generated bytes.
HTML/CSS/Core/UI gzip sizes remain 4,950 / 5,199 / 12,467 / 12,624 bytes, all below
16 KiB. Source/manifest verification and whitespace checks pass.

## Exact Phase 1 file inventory

This inventory is relative to the incoming uncommitted tree, not to Git HEAD.

| File | Reason |
|---|---|
| include/physical_fader_diagnostics.h | Recorder defaults off |
| include/touch_acquisition_diagnostics.h | Acquisition and fallback recorder flags default off |
| include/touch_configuration_snapshot.h | Snapshot defaults off |
| include/thdiff_experiment.h | Experiment defaults off; independent authorization guard |
| scripts/touch_acquisition_trace.py | Explicit flag admission and middleware isolation |
| tests/fader/run_tests.py | Explicit recorder cases plus standard-diagnostics exclusion |
| tests/fader/run_acquisition.py | Explicit trace case; isolated output location |
| tests/touch_config/run_tests.py | Explicit snapshot case plus standard-diagnostics exclusion |
| tests/thdiff/run_tests.py | Explicit simulated authorization; ordinary diagnostics exclusion; isolated output |
| tests/fader/test_isolation.py | New default-off/link/build-policy regressions |
| README.md | Exact deferred limitation and stabilization link |
| docs/PHYSICAL_FADER_INVESTIGATION.md | Historical report closure/suspension notice |
| docs/PHYSICAL_FADER_DEADZONE_FIX.md | Retained correction and closure notice |
| docs/PHYSICAL_FADER_SLOW_DRAG_DIAGNOSTICS.md | Historical instructions superseded |
| docs/PHYSICAL_FADER_ACQUISITION_TRACE.md | Historical instructions superseded |
| docs/FT6336U_READ_ONLY_CONFIGURATION.md | Snapshot opt-in/closure notice |
| docs/FT6336U_THDIFF_ABA_TEST.md | Write experiment suspended; old workflow is historical |
| docs/V0_3_0_RELEASE_NOTES_DRAFT.md | Deferred limitation, unknown THDIFF change and suspension |
| docs/V0_3_0_MIGRATION_ACCEPTANCE.md | Accepted Unit functionality vs outstanding validation |
| docs/V0_3_0_STABILIZATION_PHASE1.md | This implementation/validation report |

The application src.cpp/view_sender.h, captured helper, version header, production
network code, four environment definitions, driver patches and existing generated
web assets are byte-identical to their incoming contents. Prior build binaries,
ELFs and acquisition receipts/copies were preserved before rebuilding. Original
RC archive/component hashes and stash remain unchanged; the staging area is empty.
Detailed local validation evidence is in ignored
`dist/v0.3.0-stabilization-phase1-20261031/`.

## Remaining gates

No further touch tuning is a v0.3.0 blocker. New candidate preparation, final
source/version/commit provenance and artifact-specific smoke acceptance require
separate authorization. Existing limitations include experimental Base output,
synchronous WS send stalls, no OTA updater/validated rollback or accessory interlock,
and unperformed Base migration/interrupted-upgrade, upper-flash and external DMX
measurements. No new electrical or physical acceptance is claimed in this phase.

## v0.3.0 local closure

This accepted production prerequisite is included in the local v0.3.0 closure.
Historical version/build measurements above remain unchanged as evidence.
Current version is 0.3.0; see [closure record](V0_3_0_LOCAL_CLOSURE.md).
The captured raw-coordinate correction and diagnostic isolation remain intact.
No new touch configuration, NVS operation, upload or electrical validation is
introduced by closure. Suspended THDIFF write experiments stay disabled.
