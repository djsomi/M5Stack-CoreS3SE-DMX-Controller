#include "physical_fader_diagnostics.h"
#include <assert.h>
int main() {
    int calls=0;FADER_SCOPE(scope,loop);FADER_START(start);
    FADER_STOP(start,owner);FADER_TOUCH();FADER_VALUE(before,++calls);
    FADER_MAP(main,++calls,++calls,++calls);FADER_ACCEPT(++calls,++calls,++calls);
    FADER_VISIBLE(main,++calls);assert(calls==0);
}
