#include "wifi_startup_trace.h"
static_assert(DMX_STAGE5A_TRACE==0,"Standard builds must keep verbose startup tracing disabled");
int main() {
    wifi_startup_trace::capture("disabled");
    wifi_startup_trace::release();
    wifi_startup_trace::service();
}
