from pathlib import Path

from SCons.Script import DefaultEnvironment


env = DefaultEnvironment()

PATCHES = (
    (
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->uart_idle_conf_reg_t.tx_idle_num;",
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->idle_conf.tx_idle_num;",
    ),
    (
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->uart_txbrk_conf_reg_t.tx_brk_num;",
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->txbrk_conf.tx_brk_num;",
    ),
    (
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->uart_status_reg_t.rxd;",
        "#elif defined(CONFIG_IDF_TARGET_ESP32S3)\n"
        "  return hw->status.rxd;",
    ),
)

DESCRIPTIONS = (
    (
        "hw->uart_idle_conf_reg_t.tx_idle_num",
        "hw->idle_conf.tx_idle_num",
    ),
    (
        "hw->uart_txbrk_conf_reg_t.tx_brk_num",
        "hw->txbrk_conf.tx_brk_num",
    ),
    (
        "hw->uart_status_reg_t.rxd",
        "hw->status.rxd",
    ),
)


def fail(message):
    raise RuntimeError("esp_dmx v2.02 ESP32-S3 patch failed: " + message)


project_libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
pioenv = env.subst("$PIOENV")
dmx_ll = project_libdeps_dir / pioenv / "esp_dmx" / "src" / "impl" / "dmx_ll.h"

if not dmx_ll.exists():
    fail(
        "expected source file is missing: "
        + str(dmx_ll)
        + ". PlatformIO must install upstream esp_dmx v2.02 before this script runs."
    )

text = dmx_ll.read_text(encoding="utf-8")

original_present = [original in text for original, _ in PATCHES]
patched_present = [patched in text for _, patched in PATCHES]

if all(patched_present) and not any(original_present):
    print("esp_dmx v2.02 ESP32-S3 patch: already applied")
elif all(original_present) and not any(patched_present):
    for original, patched in PATCHES:
        text = text.replace(original, patched, 1)

    dmx_ll.write_text(text, encoding="utf-8")

    print("esp_dmx v2.02 ESP32-S3 patch: applied")
    for original, patched in DESCRIPTIONS:
        print("  " + original + " -> " + patched)
else:
    details = []
    for index, (original, patched) in enumerate(PATCHES):
        if original in text and patched in text:
            state = "mixed: original and patched expressions both present"
        elif original in text:
            state = "unpatched"
        elif patched in text:
            state = "patched"
        else:
            state = "missing both expected expressions"

        description = DESCRIPTIONS[index]
        details.append("  " + description[0] + " -> " + description[1] + " : " + state)

    fail(
        "dependency file is partially patched or does not match expected upstream "
        "esp_dmx v2.02 text:\n"
        + "\n".join(details)
    )
