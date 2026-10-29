"""Source placement guards, not a simulation of hardware rendering or timing."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tests/network"))
from canvas_guard_support import stage5a_view
from softap_guard_support import stage5b_view
from device_settings_guard_support import restore
stage1_commit = "0e43640e57519e47177b08fae9c0b86ff8b0e9d9"
app = (root / "src/src.cpp").read_text(encoding="utf-8")
view = (root / "src/view_sender.h").read_text(encoding="utf-8")


def baseline(path):
    return subprocess.check_output(
        ["git", "-c", f"safe.directory={root.as_posix()}", "show", f"{stage1_commit}:{path}"],
        cwd=root, text=True, encoding="utf-8",
    )


def section(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


loop = app.split("void loop(void) {", 1)[1]
assert loop.index("command_dispatcher.setSenderActive(scene_mode == mode_sender)") < loop.index("command_dispatcher.process();")
assert loop.index("command_dispatcher.process();") < loop.index("return;")
assert loop.index("command_dispatcher.process();") < loop.index("if (display_sleeping && touch.wasPressed())")
assert loop.index("view_sender.serviceControllerChanges();") < loop.index("return;")
startup = section(app, "static void enterSenderFromStartup(bool consume_touch)", "static void drawStartup")
assert startup.index("setSenderActive(true)") < startup.index("view_sender.setup()")
assert "scene_mode = mode_select;\n    command_dispatcher.setSenderActive(false);" in app
assert "scene_mode = mode_startup;\n    command_dispatcher.setSenderActive(false);" in app
assert "scene_mode = (clicked_idx == 0) ? mode_receiver : mode_sender;\n        command_dispatcher.setSenderActive(scene_mode == mode_sender);" in app

mutation = re.compile(r"\b(?:controller|dmx_controller)\s*\.\s*(?:reset|setChannel|adjustChannel|setChannels|setBlackout|loadPreset|savePreset|copyPreset|clearPreset)\s*\(")
for path in (root / "src").iterdir():
    if path.suffix in {".h", ".cpp"} and path.name not in {"dmx_controller.cpp", "command_dispatcher.cpp"}:
        assert not mutation.search(path.read_text(encoding="utf-8")), f"Bypass: {path.name}"
assert "if (target_value != controller.channel(target_channel))" not in view

old_view = baseline("src/view_sender.h")
# Approved Settings and Stage5B allocation changes are independently guarded by network tests.
view_for_guard = re.sub(r"        // Stage 5 local network information begin.*?        // Stage 5 local network information end.\n\n", "", stage5a_view(stage5b_view(restore("src/view_sender.h",view))), flags=re.S)
for start, end in [
    ("    void drawPrecisionEditor(void)", "    void openPrecisionEditor(void)"),
    ("    void drawPrecisionKeypad(", "    void drawPrecisionEditor(void)"),
    ("    void handlePrecisionRepeat(", "    void handlePrecisionEditorTouch("),
    ("    void handlePrecisionEditorTouch(", "    void hideUIValueSet("),
    ("    void updateBatteryMonitor(", "    void formatBatteryRuntime("),
    ("    void drawSettingsScreen(", "    void openSettingsScreen("),
    ("    void drawUIValueSet(", "    uint32_t getBarColor("),
    ("    M5Canvas* getFullscreenCanvas(", "    void pushFullscreenCanvas("),
]:
    assert start in view and end in view, f"Missing guard boundary: {start}"
    assert section(view_for_guard, start, end) == section(old_view, start, end), f"Unexpected UI/power change: {start}"

assert "view_sender.serviceDmxOutput();" in section(app, "static void serviceDmxDuringWakeTouch(", "static void serviceDmxTransportDiagnostics(")
assert loop.index("dmx_transport.poll(") < loop.index("command_dispatcher.process();")
assert view.index("serviceDmxOutput();", view.index("bool loop(void) override")) < view.index("if (settings_screen)", view.index("bool loop(void) override"))
shutdown = section(view, "    void sendDmxZeroBeforePowerAction(", "    void updateSettingsPowerArmState(")
assert "for (int i = 0; i < 3; ++i)" in shutdown
assert "transport.stage(zero_packet, DMX_MAX_PACKET_SIZE);" in shutdown
assert "dmx_wait_send_done(" in shutdown and "50" in shutdown and "delay(10);" in shutdown
assert "dmx_send_packet" not in shutdown and "serviceTx" not in shutdown

for path in ["include/dmx_hardware.h", "src/preset_store.h",
             "include/hardware_diagnostics.h", "src/hardware_diagnostics.cpp",
             "scripts/patch_esp_dmx_v202.py"]:
    assert (root / path).read_text(encoding="utf-8") == baseline(path), f"Unexpected protected-file change: {path}"

config = (root / "platformio.ini").read_text(encoding="utf-8")
for addition in ("    pre:scripts/patch_esp_dmx_tx_ownership.py\n",
                 "board_upload.flash_size = 16MB\n",
                 "board_build.partitions = partitions/cores3se_16mb_ota.csv\n",
                 "    pre:scripts/embed_web_assets.py\n"):
    assert config.count(addition) == 1
    config = config.replace(addition, "")
assert config == baseline("platformio.ini")
assert "dmx_write_packet(" not in app + view
assert "dmx_send_packet(" not in app + view
print("PASS application-loop service before page/wake returns; mode gates; no mutation bypass")
print("PASS scoped transport integration, legacy shutdown limitation, unchanged critical UI, holds, battery, canvas, USB and S3 patch")
