"""Exact reviewed Stage6B.4 provenance/tile exceptions to earlier-stage freezes.

Functional Controller/gesture/WS tests validate semantics; these pinned hashes
keep the old complete-file reconstruction guards strict outside this change.
No driver, dispatcher, storage, timing or other physical UI exception is added.
"""
from pathlib import Path
import hashlib
import subprocess
from device_settings_guard_support import restore
ROOT = Path(__file__).resolve().parents[2]
REVIEWED = {'src/dmx_controller.cpp': 'df4adec34621c2ac0f4caca84e5136c22dd7f0b5aa37283cc3a69d70ac916bcf', 'src/dmx_controller.h': 'c3494d45f58aed417014b2ea83416da7ecd1bb97aa6b31495e088e8683ecbe34', 'src/view_sender.h': 'd6cd78fa609262f22b0238acf086f658c0432f3d98d52147b45e03500df08e29', 'include/websocket_bridge.h': 'cc48015cbdea5d8f0cf7c20854c047c7925b285f9ffde57ab8509e816bfa07cc', 'src/websocket_protocol.cpp': '7cac4f0bad0e0a76d027dc7fa63a0aa2bf8faa0eb25f431e257502be18033be7', 'include/preset_double_tap.h': '8e3b07fbeb6481629d77f2651ae1820c9cbd0499931b7f92a90b153842f12ef7'}

def prior(name, text):
    text = restore(name,text)
    if name not in REVIEWED:
        return text
    assert hashlib.sha256(text.replace("\r\n", "\n").encode("utf-8")).hexdigest() == REVIEWED[name], "Unreviewed loaded-preset input: " + name
    return subprocess.check_output(["git", "-c", "safe.directory=" + ROOT.as_posix(), "show", "5b3cea7:" + name], cwd=ROOT).decode("utf-8")

def check_new_header():
    name = "include/preset_double_tap.h"
    assert hashlib.sha256((ROOT/name).read_bytes().replace(b"\r\n", b"\n")).hexdigest() == REVIEWED[name]
