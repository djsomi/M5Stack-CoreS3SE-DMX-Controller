"""Default-off headers and actual PlatformIO middleware admission; no hardware."""
from pathlib import Path
import argparse, os, subprocess, sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import touch_acquisition_trace as trace


class Environment:
    def __init__(self, profile, defines=()):
        self.profile, self.defines = profile, list(defines)
        self.middleware, self.actions = [], []

    def get(self, key, default=None):
        assert key == 'BUILD_FLAGS'
        return self.defines

    def ParseFlags(self, flags):
        assert flags == self.defines
        return {'CPPDEFINES': flags}

    def subst(self, key):
        return self.profile if key == '$PIOENV' else 'unused-investigation-output'

    def AddBuildMiddleware(self, callback):
        self.middleware.append(callback)

    def AddPostAction(self, target, callback):
        self.actions.append((target, callback))


def rejected(env, marker):
    try:
        trace.install(env, ROOT)
    except ValueError as error:
        assert marker in str(error), error
    else:
        raise AssertionError('unsafe configuration accepted: ' + env.profile)
    assert not env.middleware and not env.actions


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    out = p.parse_args().output
    out.mkdir(parents=True, exist_ok=True)
    hw = ('DMX_HW_DIAGNOSTICS', 1)
    fader = ('DMX_FADER_DIAGNOSTICS', 1)
    acquisition = ('DMX_TOUCH_ACQUISITION_DIAGNOSTICS', 1)
    snapshot = ('DMX_TOUCH_CONFIG_DIAGNOSTICS', 1)
    experiment = ('DMX_THDIFF_EXPERIMENT', 1)
    authorized = ('DMX_THDIFF_AUTHORIZED_INVESTIGATION', 1)
    profiles = ('cores3se_unit', 'cores3se_base', 'cores3se_unit_diagnostics', 'cores3se_base_diagnostics')
    for profile in profiles:
        defines = [hw] if profile.endswith('_diagnostics') else []
        env = Environment(profile, defines)
        trace.install(env, ROOT)
        assert not env.middleware and not env.actions
        assert not trace.investigation_flags(env)['DMX_THDIFF_EXPERIMENT']
        rejected(Environment(profile, [hw, snapshot, experiment, authorized]), 'suspended')
    for defines in ([hw, fader], [hw, snapshot], [hw, fader, snapshot]):
        env = Environment('cores3se_unit_diagnostics', defines)
        trace.install(env, ROOT)
        assert not env.middleware and not env.actions
    env = Environment('cores3se_unit_diagnostics', [hw, fader, acquisition])
    trace.install(env, ROOT)
    assert len(env.middleware) == 1 and len(env.actions) == 1
    rejected(Environment('investigation', [acquisition]), 'DMX_HW_DIAGNOSTICS')
    rejected(Environment('investigation', [hw, acquisition]), 'DMX_FADER_DIAGNOSTICS')
    rejected(Environment('investigation', [hw, snapshot, experiment]), 'suspended')
    rejected(Environment('investigation', [hw, (acquisition[0], 2)]), '0/1')
    rejected(Environment('investigation', [hw, hw]), 'one explicit')

    # Link all real modules without Arduino/M5 mocks: disabled code must not
    # reference USB, I2C, touch, cleanup, or any missing investigation backend.
    source = out / 'disabled.cpp'
    source.write_text('''#include "touch_acquisition_diagnostics.h"
#include "physical_fader_diagnostics.h"
#include "touch_configuration_snapshot.h"
#include "thdiff_experiment.h"
static_assert(!DMX_FADER_DIAGNOSTICS && !DMX_TOUCH_ACQUISITION_DIAGNOSTICS &&
              !DMX_TOUCH_CONFIG_DIAGNOSTICS && !DMX_THDIFF_EXPERIMENT, "default isolation");
int main() {
  touch_configuration::captureStartup(); thdiff::begin(); thdiff::poll();
  thdiff::beforeExit(); thdiff::outputError();
  return thdiff::pending() || touch_configuration::pending();
}
''', encoding='utf-8')
    os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT / 'dist/host-toolchain/cache'))
    os.environ.setdefault('ZIG_LOCAL_CACHE_DIR', str(ROOT / 'dist/host-toolchain/local-cache'))
    cc = [str(ROOT / 'dist/host-toolchain/ziglang/zig.exe'), 'c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-Iinclude']
    modules = ['src/physical_fader_diagnostics.cpp', 'src/touch_acquisition_diagnostics.cpp', 'src/touch_configuration_snapshot.cpp', 'src/thdiff_experiment.cpp']
    for profile in profiles:
        flags = ['-DDMX_HW_DIAGNOSTICS=1'] if profile.endswith('_diagnostics') else []
        exe = out / (profile + '.exe')
        subprocess.run(cc + flags + modules + [str(source), '-o', str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe)], cwd=ROOT, check=True)
    probe = out / 'authorization.cpp'
    probe.write_text('#include "thdiff_experiment.h"\n', encoding='utf-8')
    bad = subprocess.run(cc + ['-DDMX_HW_DIAGNOSTICS=1', '-DDMX_TOUCH_CONFIG_DIAGNOSTICS=1', '-DDMX_THDIFF_EXPERIMENT=1', '-c', str(probe), '-o', str(out / 'bad.o')], cwd=ROOT, capture_output=True, text=True)
    assert bad.returncode and 'separately authorized investigation' in bad.stderr
    print('PASS four default environments: no investigation I/O/cleanup references; THDIFF cannot be enabled indirectly or in standard PlatformIO environments; compile copies require explicit acquisition opt-in')


if __name__ == '__main__':
    main()
