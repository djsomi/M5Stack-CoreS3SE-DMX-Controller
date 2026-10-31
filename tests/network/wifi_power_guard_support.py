"""Exact reversal of the reviewed local Wi-Fi lifecycle/footer change only.

Keep historical whole-file invariants in force; current lifecycle, persistence
and UI behavior are tested independently by tests/wifi_power.
"""
from pathlib import Path
import hashlib
import json
from preset_archive_guard_support import prior as before_archive

DELTAS = json.loads(Path(__file__).with_name('wifi_power_delta.json').read_text(encoding='utf-8'))


def prior(name, text):
    text = before_archive(name, text)
    if name not in DELTAS:
        return text
    markers = {'src/view_sender.h': ('handleWifiPowerTouch', 'build_timestamp_y = 474'),
               'src/network_service.cpp': ('bool runNetwork()', 'bool requestEnabled('),
               'include/network_service.h': ('requestEnabled(', 'radio_on;')}
    # Other existing scope adapters can call us again after their own exact
    # reversals. A source without this feature remains their responsibility.
    if not any(marker in text for marker in markers[name]):
        return text
    change = DELTAS[name]
    digest = hashlib.sha256(text.encode()).hexdigest()
    if digest == change['before']:
        return text
    assert digest == change['after'], 'Unreviewed local Wi-Fi lifecycle/footer delta: ' + name
    for after, before in change['patches']:
        assert text.count(after) == 1, name
        text = text.replace(after, before, 1)
    assert hashlib.sha256(text.encode()).hexdigest() == change['before'], name
    return text
