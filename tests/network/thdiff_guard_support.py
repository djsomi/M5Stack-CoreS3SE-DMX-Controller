"""Remove only the exact diagnostic experiment integration, preserving domain guards."""
def prior(name,text):
    text=text.replace('#include "thdiff_experiment.h"\n','',1)
    if name=='src/src.cpp':
        text=text.replace('        thdiff::beforeExit();\n','')
        text=text.replace('    thdiff::beforeExit();\n','')
        text=text.replace('''#if DMX_THDIFF_EXPERIMENT
bool thdiffTouchCaptured() { return view_sender.diagnosticFaderCaptured(); }
#endif

''','',1)
    if name=='src/view_sender.h':
        text=text.replace('''#if DMX_THDIFF_EXPERIMENT
    bool diagnosticFaderCaptured() const {return main_value_slider_dragging || precision_slider_dragging;}
#endif
''','',1)
    if name=='src/hardware_diagnostics.cpp':
        text=text.replace('bool thdiff_line = false;\n','',1)
        text=text.replace('''    } else if (thdiff::pending()) {
        length = static_cast<int>(thdiff::formatNext(line, sizeof(line)));
        thdiff_line = true;
''','',1)
        text=text.replace('    thdiff::begin();\n','',1)
        text=text.replace('    thdiff::poll(); // RX/deadline/rollback run even while diagnostic TX is paused.\n','',1)
        text=text.replace(' && !thdiff::pending()','')
        text=text.replace('''    const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(line + line_sent), count);
    if (!written || written > count) thdiff::outputError();
    if (written <= count) line_sent += written;
    if (line_sent == line_size && thdiff_line) {
        thdiff_line = false;
        thdiff::lineSent();
    }
''','''    line_sent += Serial.write(reinterpret_cast<const uint8_t*>(line + line_sent), count);
''',1)
    return text
