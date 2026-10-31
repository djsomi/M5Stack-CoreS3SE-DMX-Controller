"""Exact removal of optional measurement hooks and the approved boot caption only."""
from wifi_power_guard_support import prior as before_wifi_power
from pathlib import Path
import re
import subprocess
from captured_fader_guard_support import prior as before_capture
from thdiff_guard_support import prior as before_thdiff
from fullscreen_navigation_guard_support import prior as before_navigation
ROOT=Path(__file__).resolve().parents[2]
BASE='9fdf07ee02340003e2ac4efc3daf61ce8c787062'

def prior(name,text):
    text=before_wifi_power(name,text)
    text=before_navigation(name,text)
    text=before_thdiff(name,text)
    text=before_capture(name,text)
    marker='#include "physical_fader_diagnostics.h"\n'
    if marker not in text:return text
    assert name in ('src/src.cpp','src/view_sender.h','src/hardware_diagnostics.cpp')
    text=text.replace(marker,'',1)
    text=re.sub(r'^[^\n]*\bFADER_(?:SCOPE|START|STOP|TOUCH|MAP|ACCEPT|VISIBLE|VALUE)\([^\n]*\);\n','',text,flags=re.M)
    if name=='src/src.cpp':
        text=text.replace('#include "firmware_version.h"\n','',1)
        text=text.replace('"UNIT DMX - V" DMX_FIRMWARE_VERSION','"UNIT DMX"',1)
        text=text.replace('"BASE DMX - V" DMX_FIRMWARE_VERSION','"DMX BASE"',1)
    if name=='src/hardware_diagnostics.cpp':
        block='''#if DMX_FADER_DIAGNOSTICS
    // Pause all diagnostic output during touch. Finish any S0 line before FADER
    // owns this same non-waiting USB path; never interleave partial lines.
    if (physical_fader::touchActive()) return;
    if (line_sent == line_size && physical_fader::service()) return;
#endif
'''
        assert text.count(block)==1
        return text.replace(block,'',1) # Existing release guard verifies metadata delta.
    old=subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),'show',BASE+':'+name],cwd=ROOT).decode('utf-8')
    assert text==old,'Unreviewed fader/boot source change: '+name
    return text
