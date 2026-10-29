#pragma once
#include <stdint.h>

// Release-qualified pair: no timers, allocations or dependency on display APIs.
class PresetDoubleTap {
 public:
    void reset() { tracking_ = candidate_ = false; }
    bool sample(int slot, bool begin, bool down, bool end, int x, int y, uint32_t now) {
        if (begin) {
            tracking_ = slot >= 0 && slot < 8;
            if (!tracking_) candidate_ = false;
            slot_ = slot; x_ = x; y_ = y; began_ = now;
        }
        if (!tracking_) return false;
        if (slot != slot_ || far(x, x_) || far(y, y_) || uint32_t(now - began_) > 350) {
            reset(); return false;
        }
        if (!end) { if (!down) reset(); return false; }
        tracking_ = false;
        const bool pair = candidate_ && last_slot_ == slot && uint32_t(now - last_) <= 350 &&
            !far(x, last_x_) && !far(y, last_y_);
        candidate_ = !pair; last_slot_ = slot; last_x_ = x; last_y_ = y; last_ = now;
        return pair;
    }
 private:
    static bool far(int a, int b) { return a - b > 12 || b - a > 12; }
    bool tracking_ = false, candidate_ = false;
    int slot_ = -1, x_ = 0, y_ = 0, last_slot_ = -1, last_x_ = 0, last_y_ = 0;
    uint32_t began_ = 0, last_ = 0;
};
