"""Undo only the reviewed captured-Main-fader raw-coordinate input substitution."""
def prior(name,text):
    marker='#include "captured_fader_touch.h"\n'
    if marker not in text:return text
    assert name=='src/view_sender.h'
    text=text.replace(marker,'',1)
    blocks=[('''        int main_fader_y=tp.y;
        bool main_fader_sample=false;
        if (main_value_slider_dragging) {
            main_fader_sample=captured_fader_touch::read(
                M5.Touch,M5.Display,main_value_slider_touch_id,tp,main_fader_y);
            if (!main_fader_sample) {
                main_value_slider_dragging=false;
                main_touch_consume_release=captured_fader_touch::anyPressed(M5.Touch);
                if (!main_touch_consume_release) tp.state=m5::touch_state_t::none;
            }
        }

''',''),('''                            if (slider_hit) main_value_slider_touch_id=tp.id;
''',''),('''    uint8_t main_value_slider_touch_id = 0;
''',''),('''                        if (!main_fader_sample && !captured_fader_touch::read(
                                M5.Touch,M5.Display,main_value_slider_touch_id,tp,main_fader_y)) {
                            main_value_slider_dragging=false;
                            main_touch_consume_release=captured_fader_touch::anyPressed(M5.Touch);
                        } else {
                            new_value = 255 - (main_fader_y - slider_y) * 2;
                        }
''','''                        new_value = 255 - ((tp.y - slider_y) << 1);
''')]
    for new,old in blocks:
        assert text.count(new)==1,'Unexpected captured-fader delta: '+new
        text=text.replace(new,old,1)
    return text # Enclosing fader guard still requires exact accepted source.
