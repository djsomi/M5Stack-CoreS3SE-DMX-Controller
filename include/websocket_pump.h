#pragma once
#include <stdint.h>
namespace websocket_bridge {
// Four bounded rounds, one frame/client/round. Time checked between frames:
// an in-progress frame retains its existing absolute 100 ms I/O deadline.
// Rotate the first socket to avoid starving clients when the budget expires.
constexpr unsigned pump_rounds=4;
constexpr int64_t pump_budget_us=20000;
template<class SendOne,class Clock>
void drain(unsigned slots,unsigned& first,SendOne sendOne,Clock clock) {
    if(!slots)return;
    const auto start=clock();
    for(unsigned round=0;round<pump_rounds;++round) {
        bool progress=false;
        for(unsigned i=0;i<slots;++i) {
            if(clock()-start>=pump_budget_us){first=(first+1)%slots;return;}
            if(sendOne((first+i)%slots))progress=true;
        }
        if(!progress)break;
    }
    first=(first+1)%slots;
}
}
