# v0.3.0 publication-readiness supplement

This documentation/attribution/packaging round does not change application,
Controller, Dispatcher, DMX, network, NVS, UI or diagnostic behavior. No firmware
was built in the original notice-only round. The later source/relink extension
below builds isolated copies only. Accepted binaries are never replaced or
flashed, and no final release bundles are generated.

## License evidence

See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for the component inventory
and [the manifest](../licenses/manifest.json) for exact sources and hashes. The
complete 2022 M5Stack upstream MIT notice and the existing additional-project
notice both appear in LICENSE. Twenty-four complete texts/comment collections
preserve the actual wording, including M5GFX's LovyanGFX/Adafruit BSD notices,
Matthew Rodusek's MIT header, Arduino LGPL, ESP-IDF Apache, FreeRTOS, lwIP, WPA,
CC0 fast-pbkdf2, Mbed TLS dual license, HTTP parser, Xtensa HAL and runtime terms.

The inventory uses installed M5Unified 0.2.21, M5GFX 0.2.28, esp_dmx 2.0.2,
Arduino package 3.20016.0 and Xtensa toolchain 8.4.0+2021r2-patch5. Installed SDK
headers identify FreeRTOS 10.4.3 and Mbed TLS 2.28.7. No independent version is
invented for vendored fonts, LovyanGFX, newlib or the WPA component. Final link
maps distinguish retained code from discarded codec/font objects and ROM symbols.
Library presence is not equated with linked payload or an enabled feature.

The upstream sources for fetched texts are recorded by URL and Git blob SHA.
Espressif v4.4.7 submodule references were inspected rather than treating the
current Wi-Fi/PHY/lwIP/Mbed TLS default branches as the validated versions.
LGPL corresponding-source/relink delivery is a separate binary-publication gate;
including license texts alone does not claim to fulfill it.

## Notice packaging and validation

The shared Unit/Base ZIP path includes exactly these additional archive entries:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `licenses/manifest.json`
- `licenses/THIRD_PARTY_LICENSES.txt`

`scripts/distribution_notices.py` checks all 24 required notice identities,
complete text/hash/range integrity, original attribution, pinned dependency
configuration and (when supplied) installed versions and 42 source/notice hashes.
It copies canonical UTF-8/LF bytes, independent of Git checkout line endings.
An exact-path whitespace-check exception preserves inherited legal-text spacing;
it changes no global Git or line-ending settings. Hash checks still reject edits.
The archive manifest lists every notice file's path, length and SHA-256, and
SHA256SUMS.txt covers them. ZIP verification rejects missing, modified, duplicate,
unexpected or incorrectly checksummed entries and false notice metadata.

Existing deterministic ZIP ordering, timestamps and permissions are retained.
The four image offsets remain **0x0000, 0x8000, 0xe000 and 0x10000**. Image,
sector-rounding, layout, checksum and provenance validation remain intact; NVS
`[0x9000,0xe000)` is not a flash-image payload. No padded FULL image is introduced.

Tests create only synthetic archives with fake image bytes in disposable test
directories. They do not produce installable firmware or a stable release bundle.
The existing candidate generator still labels candidates as candidates; this task
does not silently turn it into a stable-release publisher.

Offline verification:

```text
python -B scripts/distribution_notices.py
python -B tests/release/test_candidate.py
python -B tests/release/test_notices.py
python -B scripts/web_asset_pipeline.py
git diff --check
```

Optional installed verification (replace the three paths with the actual local
PlatformIO locations; repeat for Base or diagnostics libdeps as appropriate):

```text
python -B scripts/distribution_notices.py --libdeps .pio/libdeps/cores3se_unit --framework <packages>/framework-arduinoespressif32 --toolchain <packages>/toolchain-xtensa-esp32s3
```

## Acceptance and unchanged limitations

- Unit functional acceptance, preset Backup/Save/Delete and simplified Unit
  v0.2.0 migration were reported passed by the user. Base remains experimental;
  separate Base migration and interrupted-upgrade recovery are not established.
- Predictable MAC-derived factory credentials must be changed before use in an
  untrusted environment. Associated clients share control; physical approval of
  Wi-Fi changes does not authenticate DMX/preset/maintenance operations.
- HTTP/WebSocket has no application-layer encryption. ESP-IDF 4.4 is EOL; no
  security upgrade or new authorization system is implemented in this round.
- Accepted fixed-scale iOS behavior restricts browser magnification. It is a
  documented accessibility tradeoff, not a newly claimed fix.
- Synchronous TCP sends can temporarily block the shared HTTP task. NVS work
  can delay the next application-serviced DMX start. No external wire-timing or
  coherence guarantee is inferred from software tests or fixture acceptance.
- Existing Power Off behavior remains accepted; complete shutdown-zero frames,
  upper-flash alias/write checks and application-health OTA rollback remain
  unverified. Preset generations are not backwards-readable by v0.2.0; export
  before downgrading. Framework recovery of broken NVS can format that partition.
- Import/Export intentionally cancels on browser backgrounding; keep the tab open
  and active. This behavior and all existing UI design remain unchanged.

Slow continuous touchscreen fader movement may exhibit intermittent coordinate
stalls originating at or before the touch-controller acquisition layer. Historical
upstream code contains the same relevant acquisition mechanisms. No application-level
cause was demonstrated. The behavior is not proven to have occurred on the original
firmware. Captured raw coordinates are retained and THDIFF experiments suspended.

## Immutable tag and binary provenance

The annotated `v0.3.0` tag remains on
`59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`. Its object is
`1c9c388388a31be701c4021bdacb3d70e89906b9`. It must not be moved or recreated.

The existing four binaries remain valid artifacts of that tagged source. Their
embedded version, source commit, source fingerprint and SHA-256 do not change
when documentation is edited. They remain rebuildable from the tag with the
pinned environment; byte-identical rebuilds are not promised because compilation
timestamps and toolchain details can affect the output.

These publication files require a **new follow-up commit** if they are to become
part of published repository history. The existing tag/source ZIP cannot contain
that later commit. No new release number or tag is selected here.

Packaging scripts are included in the existing build-input fingerprint. Editing
them changes the current checkout fingerprint even though runtime code is frozen.
Consequently, existing receipts are intentionally stale for packaging from this
modified checkout. The package gate is not weakened to make them pass.

Before distribution, explicitly choose one provenance strategy:

1. Retain the tested v0.3.0 firmware/tag and publish a separately identified
   documentation/notice/packaging supplement. Record **firmware source commit**
   and **packaging commit** separately and verify the original binary hashes.
   This requires an explicitly reviewed packaging step using the frozen firmware
   source/receipts; the current single-checkout candidate gate does not perform
   this separation automatically.
2. Authorize a new release identity for a follow-up source commit, rebuild and
   verify its provenance, then create a new tag. Do not present those new builds
   as artifacts of the unchanged old tag.

The first option preserves the physically tested firmware exactly. The extension
below implements its offline preflight; it does not publish or commit it.
Old RC archives/checksums remain historical and must not be
relabeled. Also complete the LGPL source/relink distribution method, final sparse
bundle/checksum verification and the documented acceptance/limitation review before
authorizing hosted publication.

## Validation record and diagnostic dependency discrepancy

The 11 existing release tests, 11 notice/archive tests and 15 existing asset
pipeline tests passed. Synthetic Unit and Base archives contain the four notice
files, unchanged image offsets and NVS exclusion; repeated archive bytes match.
Missing/modified notices, removed inventory entries, false archive metadata,
changed dependency configuration and modified installed evidence are rejected.
LF and CRLF checkouts produce identical notice payload bytes. Production Unit
and Base both pass installed version and all 42 source/notice checks.

An additional check of the current diagnostic libdeps directories was correctly
rejected: **both report M5GFX 0.2.34**, whereas production Unit/Base report 0.2.28.
The preserved `cores3se_*_diagnostics-committed.log` build dependency graphs report
0.2.28. Those logs and today's metadata therefore disagree; metadata alone does
not establish when it changed or what source was compiled into an older ELF.
Do not rewrite the historical logs or claim that the diagnostic metadata check
passed. No dependency installation, correction or diagnostic rebuild is performed
here. Diagnostics cannot be release-candidate profiles. Resolve/reverify their
dependency state in a separately authorized build task if diagnostics are needed.
The production notice inventory is not widened to silently accept 0.2.34.

The existing firmware files/receipts and all runtime source are compared with
pre-task hashes. Asset verification is read-only; no web assets are regenerated.
No four-environment rebuild is claimed in this publication-only round.

Final frontend Node suites and network/Dispatcher integration guards passed.
Existing Unit/Base bootloader/table/OTA-init/application images were revalidated
read-only, including checksums, headers, layout and sector ranges. All 24 existing
build artifacts/receipts match pre-task hashes. Git whitespace checks pass;
upstream legal whitespace is intentionally preserved by the exact-path exception.
Assets remain HTML 5,360 B, CSS 5,411 B, Core 13,325 B and UI 15,486 B gzip.

Original notice-only checkpoint build-input fingerprint (superseded below):
`3c6a88cd7e103d7ad585a9360fa49c94fab9fb703114852ccf944b0718965b2f`.
This is not retroactively assigned to the frozen firmware, whose fingerprint
remains `cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232`.

## Exact publication change inventory

Modified:

- `.gitattributes` — legal-text whitespace exception only; existing EOL rules unchanged.
- `LICENSE` — complete original and existing project MIT notices.
- `README.md`, `CHANGELOG.md` — completed feature, attribution and limitations.
- `docs/ESP_DMX_UPGRADE_EVALUATION.md`
- `docs/STAGE4_WEB_INFRASTRUCTURE_DESIGN.md`
- `docs/STAGE5A_SOFTAP_MEMORY_DIAGNOSIS.md`
- `docs/STAGE5_SOFTAP_HTTP_IMPLEMENTATION.md`
- `docs/STAGE6B1_WEB_UI_FOUNDATION.md`
  — those five technical reports retain their contents except personal path generalization.
- `docs/V0_3_0_INSTALLATION.md`, `docs/V0_3_0_RELEASE_NOTES_DRAFT.md` — distribution caveats.
- `scripts/package_release_candidate.py` — shared notice inclusion/integrity/preflight.
- `tests/release/test_candidate.py` — existing archive fixtures include real notices.
- `tests/web/check_scope.py` — explicit allowance for the new offline notice tool only.

Added:

- `THIRD_PARTY_NOTICES.md`
- `licenses/manifest.json`
- `licenses/THIRD_PARTY_LICENSES.txt`
- `scripts/distribution_notices.py`
- `tests/release/test_notices.py`
- `docs/V0_3_0_PUBLICATION_READINESS.md` (this report)

All remain unstaged/uncommitted. Existing historical/untracked files, stash and
release tag are preserved. No runtime fix, production dependency installation,
stable bundle generation or hosted publication is performed.

## 2026-11-01 source/relink and dual-provenance extension

The leading-source-notice collector now handles both `/* ... */` and `//`
headers. Seventeen distinct leading comment groups from the audited retained
Arduino source/header list are captured verbatim, with UTF-8/LF normalization.
In particular the previous omission of Preferences and HAL miscellaneous line
comments is corrected. Installed verification now checks that each captured
source header is actually present, as well as all 42 installed file hashes.
This is not an inventory of every optional module in the whole SDK.
Negative personal-path tests use a generic Windows user-directory pattern,
without embedding the developer's account name or weakening path detection.

### Delivered local materials and relink evidence

See [the build/relink guide](V0_3_0_LGPL_DELIVERY.md). The chosen LGPL method is
Section 6(a)'s application-source route with Section 6(d)'s equivalent access
alongside the firmware download. The actual source/build archive must accompany
binary hosting; a license notice or link to upstream alone is not sufficient.
The project does not make a Section 6(c) three-year written offer.

The local material tree has **4,145 files, 392,736,646 bytes** at the initial
snapshot, including complete installed Arduino source/build files, actual S3 SDK
headers/static archives/linker scripts, the PlatformIO platform and exact M5/
esp_dmx dependencies. External tool metadata records exact versions, acquisition
commands and per-file hashes; compiler executables are not redistributed.
Later instruction-only changes can change the material-tree byte total.

Frozen source reconstruction contains **149 Git objects**: one shallow original
commit, its original annotated tag, required trees and only build-input blobs.
No ancestor commits, unrelated refs, stash, origin URL, private document blobs,
logs or publisher Git configuration are copied. An intentionally URL-less
`omitted` promisor marker allows the sparse Git tree to retain its real identity.
Both reconstructed HEAD and source fingerprint exactly match the frozen build.
Tests reject unlisted objects, private paths, extra parent history, changed
identity, changed tree entries and damaged pack/source data.

Isolated `pio run -e cores3se_unit -e cores3se_base` passed, including unchanged
post-build image/provenance verification. Both profiles also passed with a
modified Arduino **Print::printf** implementation. An empty volatile assembly
reference retains `LGPL_RELINK_TEST_20261101` without adding serial output.

| Profile | Baseline ELF SHA-256 | Modified ELF SHA-256 |
|---|---|---|
| Unit | `f355fa118ca2870a0c9d77efa6d563307c10d7bc7b91cc28fc137a4fc45039b6` | `73ec06487e639a212dc09ed7a34cb0e50e652991cd01f400a14173dbb23cee8a` |
| Base | `5254f121abc1659d2d3fd82d6905bdb35a79ab5e6a92f6bd706461883e4c466e` | `f60dd3e9fed10d14cf67d5a88f5a0f477ec8b271a4786043456011f3ad412c85` |

The marker is absent from baseline ELF and present in modified Print.cpp.o,
libFrameworkArduino.a and final ELF, with a retained Print::printf probe symbol
and archive member in the final map. Original Print.cpp SHA-256 is
`118332b311f749a7bf5806c9eac7127dcb788d5843aaf432656d134325ea2f49`;
modified is `0584afd8186d734ab6dffeb1ed29757e9584e7fde8860de600a4d560338adef3`.
The main application source is unchanged. Rebuilt bytes are not represented as
byte-identical to accepted firmware, and none replace accepted images.

Environment/probe failures were preserved and corrected during validation:

- A 32,996-character compiler command exceeded Windows subprocess limits;
  short project/core aliases resolved it. No source workaround was applied.
- SCons 4.11.1 verbose post-action printing failed with `_Null + str` after
  linking. Ordinary non-verbose builds run the frozen verifier successfully.
- The first probe in default buffer Print::write was discarded by linker GC.
  Its object/archive marker was **not** accepted as proof of final linking.
  The retained printf probe supplies the successful final evidence above.

### Dual provenance and dry-run structure

`scripts/publication_release.py` verifies actual accepted production images
against a separate clean frozen-source reconstruction and the original receipts.
It reuses source/asset/layout/sector/image checks, checks exact stable identity,
and rejects RC/modified/diagnostic profiles or changed accepted application
hashes. The original single-checkout candidate/receipt gates are not relaxed.

The report records firmware commit, tag object, source fingerprint/build ID,
every binary checksum, corresponding-source ZIP checksum, notice ZIP checksum,
relink evidence hash and separate publication commit/input manifest. While this
tree is uncommitted, **publication commit is null**, with dirty/base commit and
an input digest; no nonexistent packaging commit is invented. A distinct clean
follow-up commit is required by the publication-commit gate. No tag is moved.

Local `dist/publication-relink-20261101/` holds source/build trees, baseline and
modified logs/ELFs, relink evidence, host-test logs and `local-publication/`.
The latter has only **LOCAL-VALIDATION** source/notice archives and JSON dry-run
reports, not final Unit/Base firmware ZIPs. Dry-run checks use the original Unit
and Base production images without copying over them. NVS `[0x9000,0xe000)`
remains excluded; offsets are 0, 0x8000, 0xe000 and 0x10000. Unit acceptance and
experimental Base/unverified Base migration and interrupted-upgrade recovery
remain distinct.

### Previous checkpoint blockers (resolved by the inventory extension below)

The technical relink gate passes. **The expanded source/build archive is not
yet cleared for redistribution.** It contains optional SDK archives/headers
and unused dependency/font sources beyond the retained-firmware notice scope.
Existing in-place notices are preserved, but this does not prove the complete
new archive's notice obligations. A concrete example is the delivered SDK Asio
header, which references Boost LICENSE_1_0.txt; that full text is absent from the
installed package and the 24-notice firmware collection. Do not ship this local
validation archive as a compliance-complete final source package.

Complete a separate inventory/notice closure for these additional distributed
build materials, without inventing their versions or treating the entire SDK as
LGPL. Then repeat archive integrity/preflight (and relink if build inputs change).
The preflight explicitly records this gate and refuses publication-commit mode
while the expanded material notice review is incomplete. This is a material
distribution issue, not a firmware defect or a request to change runtime code.

Also outstanding: review and authorize a **new publication-only commit**, record
its actual SHA, and arrange equivalent downloadable source/build/notices beside
the firmware. No final stable ZIPs, source offer, hosting, commit or external
publication is authorized by the local validation results.

### Extension tests and exact new file inventory

Passed: 17 publication/source safety tests; 11 notice tests; 11 release tests;
6 infrastructure tests; 15 asset/composer tests; Node frontend suites; scope,
network, SoftAP, WebSocket, startup-trace, canvas, status and Dispatcher integration
guards; preset-archive host cases; real ESP-IDF NVS exhaustion/cleanup/power-cut
cases; Wi-Fi-power and before/after navigation host regressions; Controller,
Dispatcher and transport fault-injection suites; driver patch-state tests; and
WebSocket bridge/adapter and HTTP boundary host cases. No device/NVS is accessed.
The navigation runner was rerun successfully with its required preserved
pre-fix fixture after an initial invocation omitted `--before`.

Both production profiles pass baseline and modified-library isolated builds.
No standard diagnostics build or global dependency is rebuilt/reinstalled.
The previously documented diagnostics M5GFX 0.2.34 metadata discrepancy stays
isolated: source delivery uses production 0.2.28 only and diagnostics cannot be
release profiles. Historical diagnostic logs/binaries are preserved.

Additional publication files (beyond the 20-file notice/documentation inventory
above):

- `scripts/frozen_source.py` — minimal verifiable source reconstruction.
- `scripts/publication_materials.py` — deterministic local source/build archive.
- `scripts/publication_release.py` — dual-provenance dry-run preflight/gates.
- `scripts/validate_relink.py` — isolated modified-library relink validation.
- `tests/release/test_publication.py` — source/privacy/provenance/archive tests.
- `docs/V0_3_0_LGPL_DELIVERY.md` — source delivery and build/relink instructions.

The source-file notice aggregation/manifest, notice verifier/tests, scope
allowlist and publication documentation are also updated. All publication work
remains unstaged and uncommitted; runtime inputs outside offline packaging
scripts and the accepted production artifacts are verified unchanged.

Previous publication-tool checkpoint input fingerprint:
`60a36a19494ce5555a29085d8487a8fb8764420035261dcc68a9d39e022cc577`.
This identifies today's modified build-tool inputs, not the frozen firmware.
Firmware commit/tag and fingerprint `cbffd12e6339693294b46b2fe1ee1ba698c5d887bd223a2486f9c20aa0faa232`
remain unchanged. The final dry-run report separately hashes the publication
documents/notices/tools, since these are not all part of the firmware input hash.

## Completed source/build distribution inventory

The [complete inventory and validation report](V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md)
and [machine-readable catalog](../licenses/source_materials/manifest.json)
supersede the earlier expanded-material notice blocker above. They cover 112
component groups, 2,818 exact retained dependency files and 37 notice/source items.
The catalog lists 1,316 omissions with original hashes and explicit reasons.
No original dependency or historical file is deleted.

Full official Boost 1.0 and Asio's actual 1.14.1 BSL declarations are reconciled.
SDK library/port notices and separate font/codec terms are included. All retained
standalone license/copyright files must occur in the notice set; this specifically
covers IPA, efont, libb64 and PlatformIO in addition to the original 24-text
firmware inventory. GPL font source/exception/author material accompanies the
actual bitmap sources. Independent upstream versions not recorded by the vendor
are not fabricated: exact package versions and file hashes identify the artifacts.

The reviewed source ZIP was extracted and reconstructed in a fresh isolated
environment. Both Unit and Base passed clean builds and retained-marker modified
Print::printf relinks. The firmware's frozen fingerprint and the original images
are unchanged. Standard diagnostics are not release profiles and their previously
noted cache discrepancy is not imported into this distribution.

The source archive is 108,769,054 bytes (2,868 entries). Its paired complete
notice ZIP is 491,518 bytes (40 entries). The 6.5 MB preferred font-source
supplement occurs only in the source ZIP; pair verification checks it there by
hash rather than duplicating it in the notice ZIP. All license text/catalog
members remain mandatory. Both archive checksums are in the report.

The preflight now validates this reviewed catalog and exact payload rather than
unconditionally reporting a pending inventory. It still requires a clean,
distinct publication commit, equivalent source/notices access and the original
accepted-image receipts/hashes before publication mode can pass. Today's
publication commit is null because changes remain uncommitted. Existing candidate
and stale-receipt gates are not bypassed. No stable firmware ZIP, tag, commit,
hardware operation or hosted release is produced.

No known missing notice text remains in the reviewed scope. This is a source and
license-declaration review backed by captured upstream texts; automated tests
establish integrity/relinking, not a general legal guarantee. See the inventory
report for the complete component table, source-revision limits, tests and the
remaining publication steps.

## Final documentation inventory reconciliation — 2026-11-01

This focused follow-up supersedes the documentation-inventory blocker recorded
in the earlier coverage audit. It changes offline packaging/tests/documentation
only. Firmware, dependencies, configuration, Web UI, NVS and accepted binary bytes
remain frozen. Existing archives, receipts, the annotated tag and stash are retained.

### Exact public document inventory

The only three additions to the explicit publication input allowlist are:

1. `KNOWN_ISSUES.md`
2. `docs/V0_3_0_DOCUMENTATION_COVERAGE.md`
3. `docs/PRESET_EXPORT_IMPORT.md`

There is no `docs/` wildcard. `package_release_candidate.RELEASE_DOCUMENTS`
contains this same exact tuple. Both profile bundles copy these files at those
unchanged relative paths, using canonical UTF-8/LF bytes. Each receives a
`path`/`bytes`/SHA-256 entry under `distribution_documentation`, and each is covered
by `SHA256SUMS.txt` and the exact ZIP-member validator. The immutable manifest
comparison rejects changed contents/declared hashes even if the bundle's own
checksums are recomputed. Duplicate/unexpected members and missing notices remain
errors. Required publication inputs can no longer disappear silently from the
fingerprint; missing files, empty required directories and symlinks are rejected.

The publication fingerprint includes the documents together with the other
explicitly reviewed tools/notices/docs. The dual-provenance dry-run includes the
same document manifest and rechecks the complete publication identity after
verification, rejecting an input change during verification. No existing firmware
fingerprint, receipt or archive checksum is updated to match today's tools.

Evidence links are checked in repository context, not used as a recursive copy
list. Linked private build/history paths and untracked non-publication reports
are rejected. Seven untracked investigation entries in the coverage catalog are
now plain historical references rather than broken public links. Their files are
unchanged and excluded. The generated candidate INSTALL page links the three
bundled documents and explains that other evidence links require the matching
repository/publication checkout. The intentionally small ZIP is not an offline
mirror of the repository's entire evidence corpus.

The original project LICENSE, THIRD_PARTY_NOTICES and both required license
manifest/text files remain mandatory in both profiles. The separately paired
complete notice/source archives retain their existing stricter inventories and
checksums; no new documentation is inserted into or overwrites those archives.

### Stable versus candidate classification

The candidate creator still emits `release_candidate: true`, even if its input
firmware happens to have clean stable build provenance. Its verifier now explicitly
rejects an archive claiming otherwise. A stable firmware build is not equivalent
to a finalized stable distribution. Historical RC names/checksums are unchanged.
The publication preflight remains `UNPUBLISHED-DRY-RUN`; while this work is dirty,
`publication.commit` is null and `publication_ready` is false. Its publication
mode still requires a distinct reviewed clean commit and exact frozen receipts.
The preflight is a verifier/report writer, not a final stable-ZIP creator. Do not
substitute candidate creation/renaming for final dual-provenance assembly.

### Validation in this follow-up

| Check | Result |
|---|---|
| Candidate packaging | PASS, 16 cases: exact public inventory, missing/modified docs, forged hash, duplicate/private members, canonical line endings, invalid links and false stable classification |
| Publication/provenance | PASS, 21 cases: minimum source history, fixed source/tag/fingerprint, document fingerprints/missing inputs, unrelated tracked changes, untracked report links, dirty commit/stale receipt/RC rejection, notice pair and relink proof |
| License/notice suite | PASS, 11 cases, including deterministic synthetic Unit/Base bundles with all notices and three documents |
| Expanded source/build license inventory | PASS, nine cases |
| Composer/assets | PASS, 15 cases; deterministic temporary outputs and unchanged source assets |
| Infrastructure | PASS, six cases; sparse image/range/table and stale-input checks |
| Actual frozen-input dry-run | Verifies the existing Unit/Base accepted receipts, source/notice ZIPs and retained relink evidence; no build or final ZIP generation |
| Preservation/whitespace | Original source/binaries/archive/ref/stash hashes and Git whitespace checked separately in the local verification record |

The first new missing-input test initially encountered a missing-link error before
the required-input diagnostic. Input validation was ordered before link validation;
the same negative test now passes without broadening acceptance or weakening its
assertion. Tests use synthetic firmware for ZIP creation. The actual-image check
only reads the previously accepted production artifacts. Logs/reports are in
ignored `dist/documentation-packaging-20261101/`, not in distribution members.

### Exact provenance and recommended future publication sequence

Let **F** be `59d73dde675b3e873c6d7bbdf0e7b0c9fc5c568b`, the immutable firmware
commit, and **P** be the not-yet-created clean publication commit containing all
reviewed notice/source-delivery/tooling/documentation changes. P must descend from
F. No numerical P SHA can be reported until that commit is separately authorized.

The original `v0.3.0` tag points to F and **cannot contain later documentation**.
Its GitHub automatic Source code ZIP will continue to describe F, not P. This
is compatible with keeping v0.3.0 as the firmware release identity if release notes
and manifests explicitly name P as the publication supplement. No replacement
tag, new firmware version or new firmware build is necessary for this scheme.
If a single tag containing both is required instead, a separately authorized tag
strategy is needed; silently moving v0.3.0 is never acceptable.

After separate authorization, the smallest safe sequence is:

1. Review the exact publication-only file list and diff, explicitly stage those
   files, run `git diff --cached --check`, and create one publication commit on
   the current branch. A suitable message is
   `docs(release): finalize v0.3.0 publication materials`. Preserve historical
   untracked files/stash; do not blanket-stage the workspace or amend F.
2. Record P with `git rev-parse HEAD`; confirm
   `git merge-base --is-ancestor v0.3.0 P` and `git rev-parse v0.3.0^{commit}`
   still yields F. Confirm tracked publication inputs are clean. No new tag is
   required for the dual-provenance supplement; do not create one implicitly.
3. Use a checkout of exact **P** for tools/notices/release documents, and the
   separately reconstructed exact **F** for firmware-source validation. Reuse
   the original accepted Unit/Base binaries and their original stable receipts.
   Do not invoke the single-checkout candidate creator against P and try to
   silence its correctly stale F-versus-P receipt failure.
4. Run `scripts/publication_release.py` with `--require-publication-commit`,
   `--root <clean-P>`, `--source <reconstructed-F>`, `--builds <accepted-builds>`
   and the existing framework/esptool/source-archive/notice-archive/materials/
   relink-evidence arguments. Write a new report path. It must record P and its
   publication-input digest, F/tag object/firmware fingerprint, all binary hashes,
   document hashes and paired source/notice archive hashes. No old report is reused.
5. Under separate final-assembly authorization, create fresh Unit/Base stable
   sparse distributions using those verified inputs and document/notice/member
   contracts. Verify every entry/checksum and final stable classification; do not
   rename/relabel any RC archive. Final stable assembly remains distinct from the
   current candidate writer and report-only preflight.
6. After separate repository/publication approval, publish the development/
   publication history without rewriting public main, then publish the release
   against the unchanged v0.3.0 firmware tag. Resolve release-page documentation
   links to exact P. Provide equally accessible corresponding source/build/relink
   and complete paired notices, plus final bundle checksums. Explain that automatic
   tag-source downloads do not contain the later publication supplement.

A single GitHub release can therefore identify firmware F and publication P
without changing the tag. Reproducing distribution bytes requires exact P plus
recorded external input hashes; reconstructing/relinking firmware requires F and
the reviewed source/build/tool locks. Timestamped firmware rebuilds are not
promised byte-identical. The recorded accepted binaries remain authoritative.

Remaining release gates are the separately authorized P commit, committed-mode
preflight, final stable archive assembly/content verification and publication.
Hardware limitations remain as disclosed: Unit rc.1 migration was accepted;
separate Base/final-stable migration, interrupted-upgrade recovery and external
measurements are not newly certified by packaging tests.
