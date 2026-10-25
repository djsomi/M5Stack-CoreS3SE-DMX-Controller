"""Reverse only marked Stage 5C Settings additions for earlier whole-view guards."""
import re
def stage5b_view(view):
    for name,indent in [('refresh',12),('fields',4),('methods',4),('touch',8)]:
        pattern=' '*indent+r'// Stage 5C Settings network '+name+r' begin\.\n.*?'+' '*indent+r'// Stage 5C Settings network '+name+r' end\.\n'
        if name in ('methods','touch'): pattern+=r'\n'
        view,count=re.subn(pattern,'',view,count=1,flags=re.S)
        assert count==1,'Missing Stage5C Settings boundary: '+name
    assert view.count('static constexpr int settings_content_h = 540;')==1
    return view.replace('static constexpr int settings_content_h = 540;','static constexpr int settings_content_h = 480;',1)
