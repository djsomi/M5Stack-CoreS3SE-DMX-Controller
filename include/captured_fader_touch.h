#pragma once
#include <stddef.h>
#include <stdint.h>

namespace captured_fader_touch {
template<class Touch>
bool anyPressed(Touch& touch) {
    const size_t count=touch.getCount();
    for(size_t i=0;i<count && i<Touch::TOUCH_MAX_POINTS;++i) {
        const auto& point=touch.getTouchPointRaw(i);
        const auto& detail=touch.getDetail(i);
        if(point.id==detail.id && detail.isPressed()) return true;
    }
    return false;
}
// Only an already captured Main fader may call this. Match the original point,
// require its current pressed state, and convert a COPY of the cached raw point.
// No sensor read, shared touch-state mutation, threshold change or allocation.
template<class Touch,class Display,class Detail>
bool read(Touch& touch,Display& display,uint8_t id,Detail& owner,int& y) {
    if(id>=Touch::TOUCH_MAX_POINTS) return false;
    const size_t count=touch.getCount();
    for(size_t i=0;i<count && i<Touch::TOUCH_MAX_POINTS;++i) {
        auto point=touch.getTouchPointRaw(i);
        if(point.id!=id) continue;
        const auto& detail=touch.getDetail(i);
        if(detail.id!=id || !detail.isPressed()) return false;
        display.convertRawXY(&point,1);
        owner=detail;
        y=point.y;
        return true;
    }
    return false;
}
}
