"""Deterministic v0.3.0 stable assembler; never builds firmware or uses RC ZIPs.

Production requires committed tooling T descending from P and a separate exact-P
publication checkout. Example (not executed during development):
  python -B scripts/assemble_stable_release.py --tooling-commit <T> --publication-root <clean-P>
    --source <reconstructed-F> --builds <accepted-builds> --framework <framework>
    --esptool <esptool> --source-archive <reviewed-source.zip>
    --notice-archive <reviewed-notices.zip> --materials-tree <verified-source-tree>
    --notices-tree <verified-notice-tree> --relink-evidence <proof.json>
    --output <NEW-directory> [--check-only]

--check-only executes every production input/provenance gate without writing.
There is no dirty/synthetic/candidate override. Tests inject synthetic fixtures
only into private construction helpers, never a production bypass flag.
"""
import argparse
import copy
import hashlib
import io
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
import zlib

import distribution_notices as notices
import frozen_source as frozen
import package_release_candidate as candidate
import publication_materials as materials
import publication_release as publication

PUBLICATION = "19a4862b74e310a763f9231d76e7baaeed3ec16f"
PUBLICATION_SHA256 = "960d83ae031d4a125ac87dec4aa249fef36ea8af56ca72c8b780213a73c88f21"
SOURCE_SHA256 = "89a8736e103eee33b838a5ef7dec1ceb4a3f531acaa7071449c5050e97a2a252"
NOTICE_SHA256 = "27321232d1ca82efee5e639e837fc84f13658c76973b5d1f2d2e38d0514d2535"
TOOL_CHANGES = ("scripts/assemble_stable_release.py", "tests/release/test_stable.py",
                "tests/web/check_scope.py")
DOCUMENTS = (*candidate.RELEASE_DOCUMENTS, *candidate.DOCS, "CHANGELOG.md",
             "docs/V0_3_0_RELEASE_NOTES_DRAFT.md", "docs/V0_3_0_LGPL_DELIVERY.md",
             "docs/V0_3_0_SOURCE_DISTRIBUTION_INVENTORY.md")
SOURCE_ASSET = "CoreS3SE-DMX-v0.3.0-SOURCE.zip"
NOTICE_ASSET = "CoreS3SE-DMX-v0.3.0-NOTICES.zip"
ZIP_TIME = (1980, 1, 1, 0, 0, 0)
MIGRATION = dict(nvs_offset=0x9000, nvs_size=0x5000, sparse_images_only=True,
    unit="simplified v0.2.0 to rc.1 functional migration accepted; final stable bundle not hardware-tested",
    base="experimental; separate migration unverified",
    interrupted_upgrade_recovery="unverified", electrical_measurements="unverified")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def dump(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def file_sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1048576), b""): h.update(chunk)
    return h.hexdigest()


def git(root, *args):
    return frozen.git(root, *args).decode().strip()


def committed_files(root, commit, names):
    """Compare canonical filesystem inputs to actual immutable Git blobs."""
    entries = frozen.git(root, "ls-tree", "-rz", commit, "--", *names)
    rows = []
    for entry in entries.split(b"\0"):
        if not entry: continue
        meta, name = entry.split(b"\t", 1); mode, kind, oid = meta.decode().split()
        name = name.decode("utf-8")
        if mode not in ("100644", "100755") or kind != "blob":
            raise ValueError("non-regular committed input: " + name)
        rows.append(dict(path=name, git_blob=oid))
    if {r["path"] for r in rows} != set(names):
        raise ValueError("missing/uncommitted provenance input")
    blobs = frozen.batch(root, sorted({r["git_blob"] for r in rows}))
    for row in rows:
        path = Path(root) / row["path"]
        if path.is_symlink() or not path.is_file(): raise ValueError("unsafe provenance input")
        data = path.read_bytes() if path.name.endswith(".tar.gz") else notices.canonical_file(path)
        if data != blobs[row["git_blob"]][1]: raise ValueError("modified committed input: " + row["path"])
        row.update(bytes=len(data), sha256=sha(data))
    return sorted(rows, key=lambda r: r["path"])


def loaded_modules():
    names=("distribution_notices","frozen_source","package_release_candidate","publication_materials",
           "publication_release","build_provenance","firmware_layout","web_asset_pipeline",
           "web_ui_bundle","package_firmware","source_material_inventory")
    return [Path(sys.modules[n].__file__).resolve() for n in names]


def tooling_identity(root, expected_commit):
    root = Path(root).resolve()
    commit = git(root, "rev-parse", "HEAD")
    if (not re.fullmatch(r"[0-9a-f]{40}", commit) or commit != expected_commit or
            commit in (PUBLICATION, frozen.COMMIT)):
        raise ValueError("assembler requires its own committed tooling T")
    git(root, "merge-base", "--is-ancestor", PUBLICATION, commit)
    if git(root, "rev-parse", "v0.3.0") != frozen.TAG_OBJECT or git(root, "rev-parse", "v0.3.0^{commit}") != frozen.COMMIT:
        raise ValueError("frozen firmware tag changed")
    changes = set(git(root, "diff", "--name-only", PUBLICATION, commit).splitlines())
    if not changes <= set(TOOL_CHANGES) or not set(TOOL_CHANGES[:2]) <= changes:
        raise ValueError("unreviewed tooling changes relative to P")
    if git(root, "diff", "HEAD", "--name-only"):
        raise ValueError("dirty production tooling checkout")
    extras = frozen.git(root, "ls-files", "--others", "--exclude-standard", "-z").decode().split("\0")
    if any(n.startswith("scripts/") or n in TOOL_CHANGES for n in extras if n):
        raise ValueError("uncommitted executable/tooling input")
    names = git(root, "ls-tree", "-r", "--name-only", commit, "--", "scripts", *TOOL_CHANGES[1:]).splitlines()
    rows = committed_files(root, commit, names)
    allowed = {(root / r["path"]).resolve() for r in rows}
    if not set(loaded_modules()) <= allowed or Path(__file__).resolve() not in allowed:
        raise ValueError("loaded assembler/helper is not from verified T")
    return dict(commit=commit, publication_base=PUBLICATION, source_sha256=sha(dump(rows)), files=rows,
        repository="https://github.com/djsomi/M5Stack-CoreS3SE-DMX-Controller",
        runtime=dict(python=platform.python_version(), zlib_build=zlib.ZLIB_VERSION,
                     zlib_runtime=zlib.ZLIB_RUNTIME_VERSION),
        zip=dict(compression="DEFLATE", level=9, filename_encoding="ASCII (UTF-8 compatible)",
                 timestamp=list(ZIP_TIME), create_system=3, mode="100644"))


def publication_identity(root):
    if git(root, "rev-parse", "HEAD") != PUBLICATION or git(root, "rev-parse", "HEAD^") != frozen.COMMIT:
        raise ValueError("publication checkout must be exact P with parent F")
    result = publication.publication_identity(root, True)
    if result["commit"] != PUBLICATION or result["input_sha256"] != PUBLICATION_SHA256:
        raise ValueError("publication identity/fingerprint differs from accepted P")
    committed_files(root, PUBLICATION, [r["path"] for r in result["files"]])
    committed_files(root, PUBLICATION, DOCUMENTS)
    return result


def external_inputs(args):
    """The partition/image interpreters must match delivered build materials."""
    m = materials.verify_tree(args.materials_tree)
    prefix = "core/packages/framework-arduinoespressif32/"
    for row in m["files"]:
        if row["path"].startswith(prefix):
            path = Path(args.framework) / row["path"][len(prefix):]
            if path.is_symlink() or file_sha(path) != row["sha256"]:
                raise ValueError("framework differs from verified source material")
    lock = json.loads((Path(args.materials_tree) / "BUILD-TOOLS.json").read_text())
    esptool = next(t for t in lock["tools"] if t["name"] == "tool-esptoolpy")
    if materials.file_rows(args.esptool) != esptool["files"]:
        raise ValueError("esptool differs from verified build-tool lock")


def notice_manifest(root):
    reviewed=publication.inventory.verify_repository(root)
    names=set(notices.FILES)|{publication.inventory.MANIFEST}
    names.update(r["path"] for r in reviewed["notice_files"] if r["id"]!="freefont-preferred-source")
    return [dict(path=n,bytes=len(notices.canonical_file(Path(root)/n)),
                 sha256=sha(notices.canonical_file(Path(root)/n))) for n in sorted(names)]


def named_payload(data):
    return [dict(path=n,bytes=len(d),sha256=sha(d)) for n,d in sorted(data.items())]


def verify_inputs(args):
    tooling = tooling_identity(args.tooling_root, args.tooling_commit)
    p = publication_identity(args.publication_root)
    if file_sha(args.source_archive) != SOURCE_SHA256 or file_sha(args.notice_archive) != NOTICE_SHA256:
        raise ValueError("not the accepted source/notice archive pair")
    external_inputs(args)
    with tempfile.TemporaryDirectory(prefix="stable-preflight-") as temp:
        report = publication.dry_run(args.publication_root, args.source, args.builds,
            args.framework, args.esptool, args.source_archive, args.notice_archive,
            args.materials_tree, args.notices_tree, args.relink_evidence, Path(temp) / "preflight.json", True)
    if report["publication"] != p or report["firmware"]["source_commit"] != frozen.COMMIT or not report["publication_ready"]:
        raise ValueError("stable input preflight identity mismatch")
    documents = {n: notices.canonical_file(Path(args.publication_root) / n) for n in DOCUMENTS}
    legal_rows=notice_manifest(args.publication_root)
    legal={r["path"]:(Path(args.notices_tree)/r["path"]).read_bytes() for r in legal_rows}
    if named_payload(legal)!=legal_rows: raise ValueError("notice changed during capture")
    # Both maps are explicit: no recursive workspace copying or evidence links.
    if set(documents) & set(legal): raise ValueError("duplicate document/notice member")
    guards = {Path(args.builds) / r["profile"] / "infrastructure.json":
              file_sha(Path(args.builds) / r["profile"] / "infrastructure.json") for r in report["profiles"]}
    for r in report["profiles"]:
        for c in r["components"]: guards[Path(args.builds) / r["profile"] / c["file"]] = c["sha256"]
    guards.update({Path(args.source_archive): SOURCE_SHA256, Path(args.notice_archive): NOTICE_SHA256,
                   Path(args.relink_evidence): file_sha(args.relink_evidence)})
    guards.update({Path(args.notices_tree)/row["path"]:row["sha256"] for row in legal_rows})
    return dict(args=args, tooling=tooling, preflight=report, documents=documents, notices=legal, guards=guards,
                document_manifest=named_payload(documents),notice_manifest=legal_rows)


def image_payload(ctx, receipt):
    p = receipt["provenance"]
    if (p.get("source_commit") != frozen.COMMIT or p.get("source_sha256") != frozen.SOURCE_SHA256 or
            p.get("status") != "stable" or p.get("source_modified") is not False or
            p.get("version") != "0.3.0" or any(n in receipt for n in
                ("release_candidate","classification","physical_migration_validation"))):
        raise ValueError("candidate/incorrect firmware receipt")
    if receipt["profile"] not in candidate.PROFILES or p["environment"] != receipt["profile"]:
        raise ValueError("incorrect hardware profile")
    build = Path(ctx["args"].builds) / receipt["profile"]
    candidate.validate_components(build, receipt, Path(ctx["args"].framework), Path(ctx["args"].esptool))
    data = {c["file"]: (build / c["file"]).read_bytes() for c in receipt["components"]}
    for c in receipt["components"]:
        if sha(data[c["file"]]) != c["sha256"]: raise ValueError("binary changed during capture")
    return data


def asset_reference(ctx, kind):
    path = Path(getattr(ctx["args"], kind + "_archive"))
    return dict(file=SOURCE_ASSET if kind == "source" else NOTICE_ASSET,
                bytes=path.stat().st_size, sha256=file_sha(path))


def payload(ctx, profile):
    report = ctx["preflight"]
    if report["firmware"] != dict(tag=frozen.TAG, tag_object=frozen.TAG_OBJECT,
        source_commit=frozen.COMMIT, source_sha256=frozen.SOURCE_SHA256):
        raise ValueError("wrong frozen firmware provenance")
    if report["publication"]["commit"] != PUBLICATION or report["publication"]["input_sha256"] != PUBLICATION_SHA256:
        raise ValueError("wrong publication provenance")
    t = ctx["tooling"]
    if (not re.fullmatch(r"[0-9a-f]{40}", t.get("commit", "")) or t["commit"] != ctx["args"].tooling_commit or
            t["commit"] in (PUBLICATION, frozen.COMMIT) or
            t.get("publication_base") != PUBLICATION or
            t.get("source_sha256") != sha(dump(t.get("files")))): raise ValueError("wrong assembler provenance")
    receipts = [r for r in report["profiles"] if r["profile"] == profile]
    if len(receipts) != 1: raise ValueError("missing/duplicate profile receipt")
    if set(ctx["documents"])!=set(DOCUMENTS) or named_payload(ctx["documents"])!=ctx["document_manifest"]:
        raise ValueError("modified/missing/unexpected document payload")
    if named_payload(ctx["notices"])!=ctx["notice_manifest"]:
        raise ValueError("modified/missing/unexpected notice payload")
    required=[r for r in ctx["document_manifest"] if r["path"] in candidate.RELEASE_DOCUMENTS]
    if sorted(report["distribution_documentation"],key=lambda r:r["path"])!=required:
        raise ValueError("documentation differs from P preflight")
    receipt = receipts[0]
    files = image_payload(ctx, receipt)
    receipt_path=Path(ctx["args"].builds)/profile/"infrastructure.json"
    receipt_bytes=receipt_path.read_bytes()
    if sha(receipt_bytes)!=ctx["guards"][receipt_path]: raise ValueError("accepted receipt changed")
    files["BUILD-RECEIPT.json"]=receipt_bytes
    files.update(ctx["documents"]); files.update(ctx["notices"])
    pair = dict(corresponding_source=asset_reference(ctx, "source"), notices=asset_reference(ctx, "notice"))
    if pair["corresponding_source"]["sha256"] != SOURCE_SHA256 or pair["notices"]["sha256"] != NOTICE_SHA256:
        raise ValueError("source/notice archive changed")
    refs = ("Publication documents retain repository-context evidence links. View them at exact P: "
            "https://github.com/djsomi/M5Stack-CoreS3SE-DMX-Controller/tree/" + PUBLICATION +
            ". GitHub automatic v0.3.0 source downloads identify F, not P. "
            "Distribute the paired complete source/build/relink and notice archives alongside these bundles.\n")
    files["PUBLICATION.md"] = ("# Stable v0.3.0 publication provenance\n\nF: `" + frozen.COMMIT +
        "`\n\nP: `" + PUBLICATION + "`\n\nT: `" + t["commit"] + "`\n\nAssembler source: "
        "https://github.com/djsomi/M5Stack-CoreS3SE-DMX-Controller/tree/"+t["commit"]+"\n\n" + refs).encode()
    pairs = " ".join(f"0x{c['offset']:04x} {c['file']}" for c in receipt["components"])
    files["INSTALL.md"] = ("# v0.3.0 " + candidate.PROFILES[profile] + " — stable sparse distribution\n\n" +
        "Verify SHA256SUMS.txt and manifest.json. No erase-all or padded FULL image. "
        "This does not claim new migration or electrical acceptance.\n\n" + refs + "\n" +
        ctx["documents"]["docs/V0_3_0_INSTALLATION.md"].decode("utf-8") +
        "\n## Exact image command (not executed)\n\n```text\npython -m esptool --chip esp32s3 --port PORT "
        "--baud 1500000 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB " + pairs + "\n```\n").encode()
    manifest = dict(schema=1, classification="stable-release", release_candidate=False, version="v0.3.0",
        firmware=copy.deepcopy(report["firmware"]), publication=dict(commit=PUBLICATION, input_sha256=PUBLICATION_SHA256),
        assembler=copy.deepcopy(t), firmware_build=copy.deepcopy(receipt["provenance"]),
        profile=profile, hardware_status="recommended" if profile == "cores3se_unit" else "experimental",
        layout=receipt["layout"], flash_size=receipt["flash_size"], components=copy.deepcopy(receipt["components"]),
        migration=copy.deepcopy(MIGRATION), **pair,
        receipt_sha256=ctx["guards"][Path(ctx["args"].builds) / profile / "infrastructure.json"],
        distribution_documentation=copy.deepcopy(report["distribution_documentation"]),
        public_documents=copy.deepcopy(ctx["document_manifest"]),distribution_notices=copy.deepcopy(ctx["notice_manifest"]),
        contents=[dict(path=n, bytes=len(d), sha256=sha(d)) for n, d in sorted(files.items())])
    files["manifest.json"] = dump(manifest)
    files["SHA256SUMS.txt"] = ''.join(sha(d)+"  "+n+"\n" for n,d in sorted(files.items())).encode("ascii")
    for n in files:
        materials.safe_path(n)
        if not n.isascii(): raise ValueError("non-ASCII archive member name")
    return files


def write_zip(path, files):
    with Path(path).open("xb") as stream: stream.write(zip_bytes(files))


def zip_bytes(files):
    stream=io.BytesIO()
    with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, ZIP_TIME); info.create_system=3
            info.external_attr=0o100644 << 16; info.compress_type=zipfile.ZIP_DEFLATED
            archive.writestr(info, data, compresslevel=9)
    return stream.getvalue()


def verify_bundle(path, expected):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if names != sorted(expected) or len(names) != len(set(names)) or archive.testzip():
            raise ValueError("unexpected/duplicate/missing/corrupt stable archive member")
        for info in archive.infolist():
            materials.safe_path(info.filename)
            if (info.date_time != ZIP_TIME or info.create_system != 3 or info.external_attr != 0o100644 << 16 or
                    info.compress_type != zipfile.ZIP_DEFLATED or info.flag_bits != 0 or info.extra or info.comment):
                raise ValueError("noncanonical ZIP metadata")
            if archive.read(info.filename) != expected[info.filename]:
                raise ValueError("modified stable payload/manifest/checksum")
        if archive.comment: raise ValueError("unexpected ZIP comment")
    if Path(path).read_bytes()!=zip_bytes(expected): raise ValueError("noncanonical ZIP encoding/compression")


def unchanged(ctx):
    if tooling_identity(ctx["args"].tooling_root, ctx["args"].tooling_commit) != ctx["tooling"]:
        raise ValueError("tooling changed during assembly")
    if publication_identity(ctx["args"].publication_root) != ctx["preflight"]["publication"]:
        raise ValueError("publication changed during assembly")
    for path, digest in ctx["guards"].items():
        if file_sha(path) != digest: raise ValueError("assembly input changed: " + str(path))


def release_record(ctx, rows):
    return dict(schema=1,classification="stable-release",firmware=ctx["preflight"]["firmware"],
        publication=dict(commit=PUBLICATION,input_sha256=PUBLICATION_SHA256),assembler=ctx["tooling"],
        assets=sorted(rows,key=lambda r:r["file"]))


def verify_release(folder, ctx):
    folder=Path(folder)
    expected_names={"CoreS3SE-DMX-v0.3.0-"+n+".zip" for n in candidate.PROFILES.values()}
    expected_names|={SOURCE_ASSET,NOTICE_ASSET,"RELEASE.json","SHA256SUMS.txt"}
    if {p.name for p in folder.iterdir()}!=expected_names or any(p.is_symlink() or not p.is_file() for p in folder.iterdir()):
        raise ValueError("unexpected/missing/unsafe release directory member")
    rows=[]
    for profile,label in candidate.PROFILES.items():
        name="CoreS3SE-DMX-v0.3.0-"+label+".zip"
        verify_bundle(folder/name,payload(ctx,profile))
        rows.append(dict(file=name,bytes=(folder/name).stat().st_size,sha256=file_sha(folder/name)))
    for kind,name in (("source",SOURCE_ASSET),("notice",NOTICE_ASSET)):
        ref=asset_reference(ctx,kind)
        if (folder/name).stat().st_size!=ref["bytes"] or file_sha(folder/name)!=ref["sha256"]:
            raise ValueError("modified release source/notice asset")
        rows.append(ref)
    expected=dump(release_record(ctx,rows))
    if (folder/"RELEASE.json").read_bytes()!=expected: raise ValueError("modified release provenance")
    checks={r["file"]:r["sha256"] for r in rows}; checks["RELEASE.json"]=sha(expected)
    sums=''.join(h+"  "+n+"\n" for n,h in sorted(checks.items())).encode("ascii")
    if (folder/"SHA256SUMS.txt").read_bytes()!=sums: raise ValueError("incorrect release checksums")


def assemble(args):
    ctx = verify_inputs(args)  # All production gates precede output creation.
    expected = {p: payload(ctx, p) for p in candidate.PROFILES}
    unchanged(ctx)
    if args.check_only: return ctx["preflight"]
    output = Path(args.output).resolve()
    for name in ("source","builds","framework","esptool","materials_tree","notices_tree","publication_root"):
        if output.is_relative_to(Path(getattr(args,name)).resolve()):
            raise ValueError("output must be separate from verified input trees")
    if ".git" in output.parts: raise ValueError("unsafe output location")
    # Exclusive new directory and exclusive writes; failures leave an incomplete
    # directory. It must never be distributed or reused without full validation.
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for profile, files in expected.items():
        name = "CoreS3SE-DMX-v0.3.0-" + candidate.PROFILES[profile] + ".zip"
        write_zip(output / name, files); verify_bundle(output / name, files)
        rows.append(dict(file=name, bytes=(output/name).stat().st_size, sha256=file_sha(output/name)))
    for kind, name in (("source",SOURCE_ASSET), ("notice",NOTICE_ASSET)):
        with Path(getattr(args,kind+"_archive")).open("rb") as source, (output/name).open("xb") as target:
            shutil.copyfileobj(source,target,1048576)
        ref=asset_reference(ctx,kind)
        if file_sha(output/name)!=ref["sha256"]: raise ValueError("source/notice copy checksum")
        rows.append(ref)
    unchanged(ctx)
    required={r["file"]for r in rows}
    if {p.name for p in output.iterdir()}!=required: raise ValueError("unexpected release directory member")
    release=release_record(ctx,rows); body=dump(release)
    with (output/"RELEASE.json").open("xb") as f: f.write(body)
    checks={r["file"]:r["sha256"]for r in rows};checks["RELEASE.json"]=sha(body)
    sums=''.join(h+"  "+n+"\n" for n,h in sorted(checks.items())).encode("ascii")
    with (output/"SHA256SUMS.txt").open("xb") as f: f.write(sums)
    verify_release(output,ctx);unchanged(ctx)
    return release


def parser():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--tooling-root",type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument("--tooling-commit",required=True,help="exact reviewed committed assembler T")
    for name in ("publication-root","source","builds","framework","esptool","source-archive",
                 "notice-archive","materials-tree","notices-tree","relink-evidence","output"):
        p.add_argument("--"+name,type=Path,required=True)
    p.add_argument("--check-only",action="store_true")
    return p


if __name__=="__main__":
    p=parser();a=p.parse_args()
    try:
        result=assemble(a)
        print("Validated inputs only; no stable artifacts generated." if a.check_only else "Verified stable release directory: "+str(a.output))
    except (ValueError,OSError,KeyError,zipfile.BadZipFile,subprocess.CalledProcessError) as error:
        p.exit(1,"Stable assembly rejected: "+str(error)+"\n")
