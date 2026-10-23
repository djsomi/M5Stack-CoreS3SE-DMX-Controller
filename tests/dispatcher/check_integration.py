"""Source placement guards, not a simulation of hardware rendering or timing."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
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
for start, end in [
    ("    void drawPrecisionEditor(void)", "    void openPrecisionEditor(void)"),
    ("    void drawPrecisionKeypad(", "    void drawPrecisionEditor(void)"),
    ("    void handlePrecisionRepeat(", "    void handlePrecisionEditorTouch("),
    ("    void handlePrecisionEditorTouch(", "    void hideUIValueSet("),
    ("    void updateBatteryMonitor(", "    void formatBatteryRuntime("),
    ("    void sendDmxZeroBeforePowerAction(", "    void updateSettingsPowerArmState("),
    ("    void drawSettingsScreen(", "    void openSettingsScreen("),
    ("    void drawUIValueSet(", "    uint32_t getBarColor("),
    ("    M5Canvas* getFullscreenCanvas(", "    void pushFullscreenCanvas("),
]:
    assert start in view and end in view, f"Missing guard boundary: {start}"
    assert section(view, start, end) == section(old_view, start, end), f"Unexpected UI/power change: {start}"

for start, end in [
    ("static void serviceDmxDuringWakeTouch(", "static void serviceStartupSound("),
]:
    assert section(app, start, end) == section(baseline("src/src.cpp"), start, end)

for path in ["platformio.ini", "include/dmx_hardware.h", "src/preset_store.h",
             "include/hardware_diagnostics.h", "src/hardware_diagnostics.cpp",
             "scripts/patch_esp_dmx_v202.py"]:
    assert (root / path).read_text(encoding="utf-8") == baseline(path), f"Unexpected protected-file change: {path}"

send_start = "#if ESP_DMX_VERSION == 1"
send_end = "// Keep battery ETA/warnings active"
assert section(view.split("    bool loop(void) override", 1)[1], send_start, send_end) == section(
    old_view.split("    bool loop(void) override", 1)[1], send_start, send_end)
print("PASS application-loop service before page/wake returns; mode gates; no mutation bypass")
print("PASS unchanged transport service, shutdown sequence, critical UI geometry, holds, battery and canvas")
