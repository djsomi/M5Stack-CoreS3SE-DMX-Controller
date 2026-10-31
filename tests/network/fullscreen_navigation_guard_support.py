"""Remove exactly the two reviewed navigation guards for historical comparisons."""
EARLY_RETURN = '''        // A fullscreen entry above owns all further UI work this iteration.
        if (active_fullscreen_page != fullscreen_page_none) {
            return true;
        }

'''
STOP_MOMENTUM = '''        // Retain Main's position, but never carry its animation into another page.
        if (page != fullscreen_page_none) {
            scroll_y_add = 0;
        }
'''


def prior(name, text):
    if name != 'src/view_sender.h' or EARLY_RETURN not in text:
        return text
    assert text.count(EARLY_RETURN) == text.count(STOP_MOMENTUM) == 1
    return text.replace(EARLY_RETURN, '', 1).replace(STOP_MOMENTUM, '', 1)
