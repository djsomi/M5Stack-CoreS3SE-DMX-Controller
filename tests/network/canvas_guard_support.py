"""Remove only the reviewed Stage 5B memory change for existing UI comparisons."""
NEW_ALLOCATION='''            fullscreen_canvas_buffer = fullscreen_canvas_memory::allocateOnce(
                fullscreen_canvas,
                fullscreen_canvas_allocation_attempted,
                M5.Display.width(),
                M5.Display.height(),
                esp_ptr_external_ram
            );'''
OLD_ALLOCATION='''            fullscreen_canvas.setColorDepth(16);
            fullscreen_canvas_buffer = fullscreen_canvas.createSprite(
                M5.Display.width(),
                M5.Display.height()
            );'''

def stage5a_view(view):
    pairs=[
        ('#include "fullscreen_canvas_memory.h"\n',''),
        ('#include <soc/soc_memory_types.h>\n',''),
        ('    bool fullscreen_canvas_allocation_attempted = false;\n',''),
        ('        if (!fullscreen_canvas_ready && !fullscreen_canvas_allocation_attempted) {',
         '        if (!fullscreen_canvas_ready) {'),
        (NEW_ALLOCATION,OLD_ALLOCATION),
        ('            fullscreen_canvas_ready = false;\n            fullscreen_canvas_buffer = nullptr;\n',''),
    ]
    for current,previous in pairs:
        assert view.count(current)==1,'Missing/duplicated reviewed Stage5B boundary: '+current
        view=view.replace(current,previous,1)
    return view
