"""Production lifecycle, persistence and touchscreen methods against host fakes."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from touch_acquisition_trace import function
sys.path.insert(0, str(ROOT / 'tests/network'))
from wifi_power_guard_support import prior

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
out = p.parse_args().output.resolve()
assert (ROOT / 'dist').resolve() in out.parents
out.mkdir(parents=True, exist_ok=True)
net = (ROOT / 'src/network_service.cpp').read_text(encoding='utf-8')
view = (ROOT / 'src/view_sender.h').read_text(encoding='utf-8')
header = (ROOT / 'include/network_service.h').read_text(encoding='utf-8')
for name, text in [('src/network_service.cpp', net), ('src/view_sender.h', view), ('include/network_service.h', header)]:
    prior(name, text)  # Exact scope, including all pre-existing uncommitted fixes.
methods = ''.join(function(net, s) for s in ['bool runNetwork(', 'void worker(', 'bool requestEnabled('])
(out / 'lifecycle_under_test.h').write_text(methods, encoding='utf-8')
start = net.index('    bool enabled=true; // Existing installations')
end = net.index('    storage.end();', start)
(out / 'preference_under_test.h').write_text(net[start:end], encoding='utf-8')
assert 'WIFI_MODE_STA' not in net and 'WIFI_MODE_APSTA' not in net
assert 'shared.enabled' in function(net, '    bool begin(')
assert 'shared.enabled' in function(net, '    bool available(')
assert 'configuration_authority.active()' in function(net, 'bool requestEnabled(')
request = function(net, 'bool requestEnabled(')
assert not any(s in request for s in ['esp_wifi_', 'httpd_', 'delay(', 'vTaskDelay(', 'nvs_flash_erase'])
assert '"ap1"' not in request and '"factory"' not in request
assert 'worker_task=nullptr;' not in net and 'vTaskDelete' not in net
assert 'launched=true;' in function(net, 'void launch(')
draw = function(view, '    void drawSettingsScreen(')
assert draw.index('// Stage 5 local network information end.') < draw.index('// BUILD DATE FOOTER')
assert 'settings_wifi_power_y = 288;' in view and 'build_timestamp_y = 540;' in view
assert 'settings_content_h = 562;' in view
assert 234 + 34/2 < 288 and 288 + 50 < 356 and 410 < 434 and 434 + 50 < 502 < 540 < 562
assert 'settings_controls_left = 10;' in view and 'settings_controls_right = 240;' in view
assert 'settings_wifi_button_x = settings_controls_left;' in view
assert 'settings_wifi_button_w = settings_controls_right - settings_controls_left + 1;' in view
assert 'settings_wifi_button_h = settings_back_h;' in view
assert 'side_button_h = 50;' in view and 'settings_slider_radius = 8;' in view
assert 'RESET TAKES EFFECT AFTER REBOOT' in view and 'WIFI RESET APPLY NEXT BOOT' not in view
assert 'settings_wifi_notice_y + 10 - settings_scroll_y >= 0' in view
for sig in ['    bool handleWifiPowerTouch(', '    bool handleWifiResetTouch(']:
    body=function(view,sig)
    assert 'tp.x >= settings_wifi_button_x' in body and 'tp.x < settings_wifi_button_x + settings_wifi_button_w' in body
    assert '+ settings_wifi_button_h' in body
before_view=prior('src/view_sender.h',view)
# Only button-label centering changes; network details retain their original
# center and the exact same clipping/fitting/credential visibility behavior.
text_helper=function(view,'        auto networkText =').replace(', int center_x = settings_content_center_x','').replace('c.drawString(fitted, center_x, screen_y);','c.drawString(fitted, settings_content_center_x, screen_y);')
assert text_helper==function(before_view,'        auto networkText =')
power_touch=function(view,'    bool handleWifiPowerTouch(')
assert 'tp.isPressed()' in power_touch and '>= 1000' in power_touch
assert '>= 2000' in function(view,'    bool handleWifiResetTouch(')
assert '- HOLD 2s' not in draw and '"Wi-Fi: %s"' in draw
assert '"RESET WIFI"' in draw and '"HOLD RESET WIFI 2s"' not in draw
# Same actual font and scale for Wi-Fi, RESET, RESTART, POWER OFF and BACK.
network_part=draw[draw.index('// Stage 5 local network information begin'):draw.index('// Stage 5 local network information end.')]
restart_part=draw[draw.index('// RESTART'):draw.index('// POWER OFF')]
power_part=draw[draw.index('// POWER OFF'):draw.index('// FIXED BACK BUTTON')]
back_part=draw[draw.index('// FIXED BACK BUTTON'):]
assert all('c.setFont(&fonts::Font0);' in part for part in [network_part,restart_part,back_part])
assert 'setFont(' not in power_part  # Inherits RESTART's Font0.
assert 'setTextSize(' not in draw
assert 'setTextSize(' not in function(view,'    void drawBatteryStatusTo(')
assert 'setTextSize(' not in function(view,'    void drawBuildTimestamp(')
assert 'wasClicked()' not in power_touch and 'requestEnabled(wifi_power_target)' in power_touch
assert 'delay(' not in power_touch and 'while(' not in power_touch
assert 'TFT_YELLOW' in draw and 'network.radio_on ? TFT_GREEN : TFT_WHITE' in draw
# Keep the original handle radius, now included in the shared visible bounds.
volume=function(view,'    void drawVolumeSlider(')
assert 'knob_x,\n            y,\n            settings_slider_radius,' in volume
for label in ['brightness_text','timeout_text','volume_text']:
    assert label+',\n                settings_slider_center_x,' in draw
assert 540 - (562 - 240) == 218  # Footer visible at final scroll position.
assert 'const int status_y = settings_wifi_power_y - settings_scroll_y;' in view
assert 'handleWifiPowerTouch(tp, content_y)' in function(view, '    void handleSettingsTouch(')
assert function(view, '    void drawBuildTimestamp(') == function(prior('src/view_sender.h', view), '    void drawBuildTimestamp(')
# Same native close path retires Bridge generations; no reinitialization/replay.
assert 'websocket_service::close(s->token)' in function(net, 'void closed(')
assert 'Bridge::close' in (ROOT/'src/websocket_bridge.cpp').read_text()
assert 'websocket_service::initialize' not in net

rows = view[view.index('        // Stage 5 local network information begin'):view.index('        // Stage 5 local network information end.')]
(out/'wifi_settings_rows.h').write_text(rows, encoding='utf-8')
footer=draw[draw.index('        // BUILD DATE FOOTER'):draw.index('        // Battery is a fixed status element')]
(out/'wifi_settings_footer.h').write_text(footer, encoding='utf-8')
for file, sig in [('wifi_settings_reset.h','    bool handleWifiResetTouch('),('wifi_settings_power.h','    bool handleWifiPowerTouch('),('wifi_settings_scroll.h','    void serviceSettingsKineticScroll(')]:
    (out/file).write_text(function(view, sig), encoding='utf-8')
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT/'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR', str(ROOT/'dist/host-toolchain/local-cache'))
cc = [str(ROOT/'dist/host-toolchain/ziglang/zig.exe'),'c++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude','-Isrc','-I'+str(out)]
import re
names=['settings_controls_left','settings_controls_right','settings_slider_radius','settings_slider_x','settings_slider_w','settings_slider_center_x','settings_slider_touch_x0','settings_slider_touch_x1','settings_wifi_button_x','settings_wifi_button_w']
geometry='\n'.join(re.search(r'    static constexpr int '+name+r'\s*=.*?;',view,re.S).group() for name in names)
methods='\n'.join(function(view,sig) for sig in ['    int sliderValueFromX(','    int volumeSliderValueFromX(','    void drawVolumeSlider(','    void drawSettingsSlider('])
(out/'slider_alignment.h').write_text(geometry+'\n'+methods,encoding='utf-8')
for name, source in [('slider-alignment','tests/wifi_power/test_slider_alignment.cpp'),('lifecycle','tests/wifi_power/test_lifecycle.cpp'),('settings-render','tests/wifi/test_settings_render.cpp')]:
    exe=out/(name+'.exe')
    subprocess.run(cc+[source,'-o',str(exe)], cwd=ROOT, check=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)
exe = out/'offline-controller.exe'
subprocess.run(cc+['src/dmx_controller.cpp','src/command_dispatcher.cpp','src/websocket_bridge.cpp',
    'src/websocket_protocol.cpp','tests/wifi_power/test_offline_controller.cpp','-o',str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=ROOT, check=True)
print('PASS lifecycle/SDK fault injection, persistence, Settings toggle and footer geometry, legacy scope and unchanged timestamp/provenance')
