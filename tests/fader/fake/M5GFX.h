#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
namespace m5gfx {
struct touch_point_t {int16_t x=0,y=0;uint8_t size=0,id=0;};
class LGFX_Device {
 public:
    touch_point_t point,second_point;bool down=false,second=false,reversed=false;
    bool swap_axes=false;unsigned reads=0,conversions=0;
    size_t (*acquire)(touch_point_t*,size_t)=nullptr;
    size_t getTouchRaw(touch_point_t* out,size_t count) {
        ++reads;if(acquire)return acquire(out,count);if(!down||!count)return 0;
        out[0]=second&&reversed?second_point:point;
        if(second && count>1){out[1]=reversed?point:second_point;return 2;}
        return 1;
    }
    void convertRawXY(touch_point_t* points,size_t count) {
        ++conversions;
        if(swap_axes)for(size_t i=0;i<count;++i){auto x=points[i].x;points[i].x=points[i].y;points[i].y=x;}
    }
};
}
