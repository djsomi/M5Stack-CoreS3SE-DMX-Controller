"""Explicit investigation opt-in only; standard builds never install compile copies."""
from pathlib import Path
import hashlib,json,re
LOCKS={
 'M5GFX.cpp':('M5GFX','0.2.28','df1b2e3882c45d1aa7743f6f30081fc753c609cdf38f352143f7f3bc3738514e'),
 'Touch_FT5x06.cpp':('M5GFX','0.2.28','4710a440999da7b2a65e7d2af15578314181e8c1a748ddacfbefa9ed9113ffe9'),
 'Touch_Class.cpp':('M5Unified','0.2.21','47fea9d45d0bd4835a266879a13265ea2e6060e31b46f364dedeeb3304423081')}
def sha(data):return hashlib.sha256(data).hexdigest()
def once(text,old,new):
    if text.count(old)!=1:raise ValueError('unexpected acquisition seam: '+old)
    return text.replace(old,new,1)
def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);depth=0
    for i in range(brace,len(text)):
        depth+=(text[i]=='{')-(text[i]=='}')
        if not depth:return text[start:i+1]
    raise ValueError(signature)
def instrument(name,text):
    text=text.replace('\r\n','\n')
    if name not in LOCKS or sha(text.encode())!=LOCKS[name][2]:raise ValueError('unexpected installed touch source/version/text: '+name)
    if name=='M5GFX.cpp':
        start=text.index('  struct Touch_M5StackCoreS3 :')
        body=function(text[start:],'    uint_fast8_t getTouchRaw(')
        new=once(body,'      uint_fast8_t res = 0;','      ACQ_CORE();\n      uint_fast8_t res = 0;')
        new=once(new,'!gpio_in(_cfg.pin_int)','!ACQ_PIN(gpio_in(_cfg.pin_int),true)')
        new=once(new,'{ /// clear INT.','{ /// clear INT.\n          ACQ_CLEAR();')
        new=once(new,'      return res;','      ACQ_POINTS(tp,res,false);\n      return res;')
        text=once(text,body,new)
    elif name=='Touch_Class.cpp':
        text=once(text,'  void Touch_Class::update(std::uint32_t msec)\n  {','  void Touch_Class::update(std::uint32_t msec)\n  {\n    ACQ_TOUCH(msec);')
        text=once(text,'      if (_detail_count == 0) { return; }','      ACQ_GATE();\n      if (_detail_count == 0) { return; }')
        s='    std::size_t count = _gfx->getTouchRaw(_touch_raw, TOUCH_MAX_POINTS);'
        text=once(text,s,s+'\n    ACQ_POINTS(_touch_raw,count,true);')
    else:
        text=once(text,'    size_t res = 0;','    size_t res = 0;\n    ACQ_READ(readdata,res);')
        for op,call in [('begin','lgfx::i2c::beginTransaction(_cfg.i2c_port, _cfg.i2c_addr, _cfg.freq, false)'),('write','lgfx::i2c::writeBytes(_cfg.i2c_port, readdata, 1)'),('restart','lgfx::i2c::restart(_cfg.i2c_port, _cfg.i2c_addr, _cfg.freq, true)'),('count','lgfx::i2c::readBytes(_cfg.i2c_port, readdata, 1)'),('body','lgfx::i2c::readBytes(_cfg.i2c_port, &readdata[1], points * 6 - 2)')]:text=once(text,call,'ACQ_IO('+op+','+call+')')
        text=once(text,'lgfx::i2c::endTransaction(_cfg.i2c_port).has_value();','(void)ACQ_IO(end,lgfx::i2c::endTransaction(_cfg.i2c_port));')
        text=once(text,'    if (!_check_init() || count == 0) return 0;','    ACQ_FT(count);\n    if (!ACQ_IO(init,_check_init()) || count == 0) return 0;')
        text=once(text,'(bool)gpio_in(_cfg.pin_int)','(bool)ACQ_PIN(gpio_in(_cfg.pin_int),false)')
        text=once(text,'      if (_flg_released)','      ACQ_RELEASE(_flg_released);\n      if (_flg_released)')
        text=once(text,'while ((0 == comparelen || memcmp(readdata[0], readdata[1], readlen[0])) && --retry)','while (ACQ_REPEAT(0 == comparelen || memcmp(readdata[0], readdata[1], readlen[0])) && --retry)')
        text=once(text,'    if (count > readdata[0][0])','    ACQ_SELECTED(readdata[0],readlen[0],retry);\n    if (count > readdata[0][0])')
    return '#include "touch_acquisition_diagnostics.h"\n'+text
def compile_copy(source):
    source=Path(source);text=instrument(source.name,source.read_text(encoding='utf-8'))
    def absolute(match):
        if match[1]=='touch_acquisition_diagnostics.h':return match[0]
        path=(source.parent/match[1]).resolve()
        if not path.is_file():raise ValueError('unresolved original include: '+str(path))
        return '#include '+json.dumps(path.as_posix())
    return re.sub(r'^#include "([^"]+)"',absolute,text,flags=re.M)
def investigation_flags(env):
    names=('DMX_HW_DIAGNOSTICS','DMX_FADER_DIAGNOSTICS',
           'DMX_TOUCH_ACQUISITION_DIAGNOSTICS','DMX_TOUCH_CONFIG_DIAGNOSTICS',
           'DMX_THDIFF_EXPERIMENT','DMX_THDIFF_AUTHORIZED_INVESTIGATION')
    flags=dict.fromkeys(names,0)
    seen=set()
    for item in env.ParseFlags(env.get('BUILD_FLAGS',[])).get('CPPDEFINES',[]):
        name,value=item if isinstance(item,(tuple,list)) else (item,1)
        if name not in flags:continue
        if name in seen or str(value) not in ('0','1'):
            raise ValueError('investigation flags require one explicit 0/1 definition: '+name)
        seen.add(name);flags[name]=int(value)
    if any(flags[n] for n in names[1:]) and not flags['DMX_HW_DIAGNOSTICS']:
        raise ValueError('touch investigation requires DMX_HW_DIAGNOSTICS=1')
    if flags['DMX_TOUCH_ACQUISITION_DIAGNOSTICS'] and not flags['DMX_FADER_DIAGNOSTICS']:
        raise ValueError('acquisition investigation requires DMX_FADER_DIAGNOSTICS=1')
    if flags['DMX_THDIFF_EXPERIMENT']:
        standard=('cores3se_unit','cores3se_base','cores3se_unit_diagnostics','cores3se_base_diagnostics')
        if env.subst('$PIOENV') in standard or not flags['DMX_THDIFF_AUTHORIZED_INVESTIGATION'] or not flags['DMX_TOUCH_CONFIG_DIAGNOSTICS']:
            raise ValueError('THDIFF writes suspended: dedicated separately authorized configuration required')
    return flags

def install(env,root):
    flags=investigation_flags(env)
    if not flags['DMX_TOUCH_ACQUISITION_DIAGNOSTICS']:return
    build=Path(env.subst('$BUILD_DIR'));destination=build/'touch-acquisition';selected={}
    def rewrite(source_env,node):
        source=Path(node.srcnode().get_abspath())
        if source.name not in LOCKS:return node
        package=next((p for p in source.parents if (p/'library.json').is_file()),None)
        if package is None:raise ValueError('unknown touch package: '+str(source))
        meta=json.loads((package/'library.json').read_text(encoding='utf-8'));lib,version,_=LOCKS[source.name]
        if meta.get('name')!=lib or meta.get('version')!=version:raise ValueError('unexpected touch package/version: '+str(package))
        data=compile_copy(source).encode()
        if source.name in selected and selected[source.name]['source']!=str(source):raise ValueError('duplicate touch compilation: '+source.name)
        destination.mkdir(parents=True,exist_ok=True);copy=destination/source.name
        if not copy.exists() or copy.read_bytes()!=data:copy.write_bytes(data)
        selected[source.name]=dict(source=str(source),source_sha256=LOCKS[source.name][2],generated_sha256=sha(data))
        source_env.AppendUnique(CPPPATH=[str(root/'include')]);return source_env.File(str(copy))
    env.AddBuildMiddleware(rewrite)
    def verify(source,target,env):
        if set(selected)!=set(LOCKS):raise ValueError('touch acquisition middleware must select all three sources: '+repr(selected))
        for name,m in selected.items():
            if sha(Path(m['source']).read_bytes().replace(b'\r\n',b'\n'))!=m['source_sha256']:raise ValueError('vendor source changed during build')
            if sha((destination/name).read_bytes())!=m['generated_sha256']:raise ValueError('trace copy changed during build')
        (build/'touch-acquisition.json').write_text(json.dumps(dict(mode='diagnostic compile copies; libdeps unchanged',sources=selected),indent=2)+'\n',encoding='utf-8')
        print('Touch acquisition: verified three diagnostic compile copies; vendor sources unchanged')
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.bin',verify)
