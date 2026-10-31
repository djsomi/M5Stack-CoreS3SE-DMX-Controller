"""Actual Sender loop/entry/ownership methods with a pixel/display fake; no hardware."""
from pathlib import Path
import argparse, os, subprocess, sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from touch_acquisition_trace import function
sys.path.insert(0, str(ROOT / 'tests/network'))
from wifi_power_guard_support import prior as before_wifi_power
from fullscreen_navigation_guard_support import EARLY_RETURN, STOP_MOMENTUM, prior

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--before', type=Path, required=True)
args = p.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
current = (ROOT / 'src/view_sender.h').read_text(encoding='utf-8')
before = args.before.read_text(encoding='utf-8')
assert prior('src/view_sender.h', before_wifi_power('src/view_sender.h', current)) == before, 'Only the two approved source guards may change'
assert EARLY_RETURN not in before and STOP_MOMENTUM not in before
app = (ROOT / 'src/src.cpp').read_text(encoding='utf-8')
app_loop = function(app, 'void loop(')
for common in ['dmx_transport.poll(', 'network_service::ownerTick(', 'websocket_service::ownerPoll(',
               'command_dispatcher.process();', 'serviceSpeakerVolumeOverride();']:
    assert app_loop.index(common) < app_loop.index('view_sender.loop()'), common
view_loop = function(current, '    bool loop(')
for common in ['serviceDmxOutput();', 'updateBatteryMonitor();']:
    assert view_loop.index(common) < view_loop.index(EARLY_RETURN), common
assert view_loop.index(EARLY_RETURN) < view_loop.index('        switch (ui_mode)')
assert view_loop.index('        switch (ui_mode)') < view_loop.index('        if (scroll_y_add)')

signatures = ['    bool loop(', '    void setActiveFullscreenPage(',
              '    void openSettingsScreen(', '    void closeSettingsScreen(',
              '    void openPresetScreen(', '    void closePresetScreen(',
              '    void openPrecisionEditor(', '    void closePrecisionEditor(',
              '    void resetPrecisionState(', '    void pushFullscreenCanvas(']
os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT / 'dist/host-toolchain/cache'))
os.environ.setdefault('ZIG_LOCAL_CACHE_DIR', str(ROOT / 'dist/host-toolchain/local-cache'))
cc = [str(ROOT / 'dist/host-toolchain/ziglang/zig.exe'), 'c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-Iinclude', '-Isrc']
variants = {'before': before, 'missing-stop': current.replace(STOP_MOMENTUM, '', 1),
            'missing-return': current.replace(EARLY_RETURN, '', 1), 'unit': current,
            'base': current, 'unit-diagnostics': current, 'base-diagnostics': current}
for name, source in variants.items():
    out = args.output / name
    out.mkdir(exist_ok=True)
    geometry = source[source.index('    static constexpr uint16_t scroll_width'):source.index('    static constexpr int precision_back_x')]
    geometry += source[source.index('    static constexpr int side_button_x'):source.index('    // Full-screen P1-P8 grid.')]
    methods = []
    for signature in signatures:
        method = function(source, signature)
        if signature == '    void pushFullscreenCanvas(':
            # Existing diagnostic build omits the Serial log using label.
            # Annotate only that parameter in the host copy; retain -Werror
            # and every executable statement of the actual method unchanged.
            assert method.count('const char* label') == 1
            method = method.replace('const char* label', 'const char* label __attribute__((unused))', 1)
        methods.append(method)
    (out / 'navigation_under_test.h').write_text(geometry + '\n'.join(methods), encoding='utf-8')
    exe = out / 'navigation.exe'
    flags = ['-DDMX_HW_DIAGNOSTICS=1'] if name.endswith('diagnostics') else []
    if name.startswith('base'): flags.append('-DDMX_HARDWARE_BASE')
    else: flags.append('-DDMX_HARDWARE_UNIT')
    subprocess.run(cc + flags + ['-I'+str(out), 'tests/navigation/test_navigation.cpp',
        'src/dmx_controller.cpp', 'src/command_dispatcher.cpp', '-o', str(exe)], cwd=ROOT, check=True)
    result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
    if name in ('before', 'missing-stop', 'missing-return'):
        marker = 'v.scroll_y_add == 0' if name == 'missing-stop' else 'v.main_draws == draws'
        assert result.returncode == 1 and marker in result.stderr, (name, result)
        print('PASS expected pre-fix/partial-fix failure:', name, result.stderr.strip(), flush=True)
    else:
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        print('PASS', name, result.stdout.strip(), flush=True)
print('PASS actual complete loop and entry/exit methods; both guards necessary; common owner/DMX work precedes return; only exact two source additions')
