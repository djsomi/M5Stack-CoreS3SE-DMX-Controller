#pragma once

namespace fullscreen_canvas_memory {
// Caller retains the successful pointer and sprite for all fullscreen pages.
// One attempt per lifetime: allocation failure must not become a redraw loop.
template<class Canvas, class IsExternal>
void* allocateOnce(Canvas& canvas, bool& attempted, int width, int height,
                   IsExternal is_external) {
    if (attempted) return nullptr;
    attempted = true;
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    void* buffer = canvas.createSprite(width, height);
    // M5GFX 0.2.28 can silently fall back to DMA/internal RAM on PSRAM failure.
    // Do not retain that allocation and then consume the networking reserve.
    if (buffer && !is_external(buffer)) {
        canvas.deleteSprite();
        buffer = nullptr;
    }
    return buffer;
}
}
