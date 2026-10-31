"""PlatformIO pre hook: verified assets, flash-only object, post-build layout receipt."""
from pathlib import Path
import sys
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
sys.dont_write_bytecode = True
root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(root / "scripts"))
from web_asset_pipeline import verify, emit_cpp, build_source_digest
from firmware_layout import partition_sdk, validate_table, inspect_build, write_json
from build_provenance import identity
from touch_acquisition_trace import install as install_touch_trace
install_touch_trace(env, root)

profile = env.subst("$PIOENV")
if profile not in ("cores3se_unit", "cores3se_base", "cores3se_unit_diagnostics", "cores3se_base_diagnostics"):
    raise RuntimeError("Stage 4B asset integration requires an explicitly reviewed hardware environment")
framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
esptool = Path(env.PioPlatform().get_package_dir("tool-esptoolpy"))
table = partition_sdk(framework).PartitionTable.from_csv((root / "partitions/cores3se_16mb_ota.csv").read_text())
validate_table(table)
if env.BoardConfig().get("upload.flash_size") != "16MB":
    raise RuntimeError("Stage 4B requires board_upload.flash_size = 16MB")
manifest = verify(root)
source_digest = build_source_digest(root)
build = Path(env.subst("$BUILD_DIR"))
generated = build / "generated/web_assets.cpp"
provenance = identity(root, profile, source_digest)
emit_cpp(root, generated, profile, source_digest, provenance)
env.Append(CPPPATH=[str(root / "include")], LINKFLAGS=["-Wl,--undefined=web_asset_bundle"])
env.BuildSources(str(build / "embedded"), str(generated.parent), "+<web_assets.cpp>")
inputs = [str(root / "web/generated/manifest.json"), str(root / "include/web_assets.h")]
inputs += [str(p) for p in (root / "web/src").iterdir() if p.is_file()]
inputs += [str(p) for p in (root / "web/generated").glob("*.gz")]
env.Depends(str(build / "embedded/web_assets.o"), inputs)


def validate_artifacts(source, target, env):
    if build_source_digest(root) != source_digest:
        raise RuntimeError("build inputs changed during compilation; rebuild before packaging")
    images = [(str(offset), env.subst(str(path))) for offset, path in env["FLASH_EXTRA_IMAGES"]]
    images.append((str(env["ESP32_APP_OFFSET"]), str(build / "firmware.bin")))
    receipt = inspect_build(root, build, framework, esptool, images, profile, source_digest, manifest)
    if identity(root, profile, source_digest) != provenance:
        raise RuntimeError("Git/version identity changed during build; rebuild")
    application = (build / "firmware.bin").read_bytes()
    for key in ("version", "source_commit", "build_id"):
        if provenance[key].encode() not in application:
            raise RuntimeError("missing linked firmware provenance: " + key)
    receipt["provenance"] = provenance
    (build / "boot_app0.bin").write_bytes((framework / "tools/partitions/boot_app0.bin").read_bytes())
    write_json(build / "infrastructure.json", receipt)
    print("Stage 4B verified: 16MiB, two 6MiB slots, all gzip assets retained; separate-image receipt written")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", validate_artifacts)
