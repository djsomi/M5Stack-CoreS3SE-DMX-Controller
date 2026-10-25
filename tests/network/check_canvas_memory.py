"""Display/memory integration checks and actual-guard host test preparation."""
from pathlib import Path
import argparse
import re
from canvas_guard_support import stage5a_view

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--guard-header',type=Path,help='write extracted production guard under ignored dist/')
a=p.parse_args()
view=(root/'src/view_sender.h').read_text(encoding='utf-8')
original=stage5a_view(view) # Exact, scoped reversal; existing whole-view guards run separately.
assert view.count('M5Canvas fullscreen_canvas;')==1
assert 'bool fullscreen_canvas_allocation_attempted = false;' in view
assert 'if (!fullscreen_canvas_ready && !fullscreen_canvas_allocation_attempted)' in view
assert view.count('fullscreen_canvas_memory::allocateOnce(')==1
assert 'fullscreen_canvas_ready = fullscreen_canvas_buffer != nullptr;' in view
assert view.count('fullscreen_canvas.pushSprite(&M5.Display, 0, 0);')==1
assert 'if (active_fullscreen_page != owner)' in view
close=view.split('    void close(void) override {',1)[1].split('    void serviceDmxOutput()',1)[0]
assert 'if (!network_canvas_reserved)' in close
assert 'fullscreen_canvas_ready = false;' in close and 'fullscreen_canvas_buffer = nullptr;' in close
helper=(root/'include/fullscreen_canvas_memory.h').read_text()
assert helper.index('if (attempted)')<helper.index('canvas.createSprite(')
assert helper.index('canvas.setPsram(true)')<helper.index('canvas.createSprite(')
assert 'buffer && !is_external(buffer)' in helper and 'canvas.deleteSprite();' in helper
# Inspect the exact installed, tested M5GFX source instead of assuming newer behavior.
vendor=root/'.pio/libdeps/cores3se_unit/M5GFX/src/lgfx/v1'
sprite=(vendor/'LGFX_Sprite.hpp').read_text(encoding='utf-8')
buffer=(vendor/'misc/SpriteBuffer.cpp').read_text(encoding='utf-8')
panel=(vendor/'panel/Panel_LCD.cpp').read_text(encoding='utf-8')
bus=(vendor/'platforms/esp32/Bus_SPI.cpp').read_text(encoding='utf-8')
panel_base=(vendor/'Panel.hpp').read_text(encoding='utf-8')
pixelcopy=(vendor/'misc/pixelcopy.cpp').read_text(encoding='utf-8')
assert 'getSpriteBuffer()->use_dma()' in sprite
assert 'AllocationSource::Psram:' in buffer and 'buffer = heap_alloc_psram(length);' in buffer
assert '_source = AllocationSource::Dma;' in buffer # Confirms why fallback validation is necessary.
assert '_source == AllocationSource::Dma || heap_capable_dma(_buffer)' in buffer
assert 'write_bytes(src, wb * h, use_dma);' in panel
assert 'false == use_dma && length < 1024' in bus
assert 'uint32_t regbuf[16];' in bus # Non-DMA SPI source uses bounded register staging.
assert '_write_depth = color_depth_t::rgb565_2Byte' in panel_base
assert 'no_convert( src_depth_ == dst_depth_ )' in pixelcopy
assert 'M5.Display.setColorDepth(' not in view
net=(root/'src/network_service.cpp').read_text()
constants=re.search(r'constexpr size_t minimum_internal = \d+, minimum_dma = \d+;',net).group()
predicate=re.search(r'bool safe\(const Memory& m\) \{[^\n]+\}',net).group()
assert constants=='constexpr size_t minimum_internal = 32768, minimum_dma = 16384;'
assert 'MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT' in net and 'MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA' in net
assert 'm.internal<81920 || m.largest<49152 || m.dma<32768' in net
if a.guard_header:
    target=a.guard_header.resolve()
    assert (root/'dist').resolve() in target.parents,'Host output must stay under ignored dist/'
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('#pragma once\n#include <stddef.h>\n#include "network_service.h"\nnamespace guard_under_test {\nusing Memory=network_service::Memory;\n'+constants+'\n'+predicate+'\n}\n',encoding='utf-8')
print('PASS single persistent fullscreen sprite, native PSRAM/non-DMA transfer integration, one-attempt failure path, unchanged capability guards; hardware rendering/timing remains unmeasured')
