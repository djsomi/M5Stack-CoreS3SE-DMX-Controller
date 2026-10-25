#include "wifi_startup_trace.h"
#if DMX_STAGE5A_TRACE
#include "wifi_startup_trace_core.h"
#include <esp_heap_caps.h>
#include <soc/soc_memory_types.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <hal/usb_serial_jtag_ll.h>
#include <stdio.h>

#if ARDUINO_USB_MODE != 1 || ARDUINO_USB_CDC_ON_BOOT != 1 || DMX_HW_DIAGNOSTICS
#error "Stage 5A trace uses the normal Unit USB Serial/JTAG console only"
#endif
namespace wifi_startup_trace {
namespace {
History<64> history;
portMUX_TYPE trace_mux=portMUX_INITIALIZER_UNLOCKED;
bool released=false, summary_sent=false;
size_t cursor=0, length=0, sent=0;
char output[448];
int64_t last_service=0;
Snapshot snapshot() {
    return {static_cast<uint32_t>(esp_timer_get_time()/1000),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)),
        static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)),
        static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))};
}
}
void capture(const char* step,int result,bool failed,const void* pointer,uint32_t detail,bool before) {
    const Entry entry{step,snapshot(),pointer,detail,result,failed,before};
    portENTER_CRITICAL(&trace_mux);history.append(entry);portEXIT_CRITICAL(&trace_mux);
}
void release() { portENTER_CRITICAL(&trace_mux);released=true;portEXIT_CRITICAL(&trace_mux); }
void service() {
    // Only application owner calls this. No console locks, waits, RX or allocation.
    const auto now=esp_timer_get_time();
    if(now-last_service<4000) return;
    last_service=now;
    if(sent==length) {
        Entry entry{};bool available=false, summary=false;
        size_t count=0,dropped=0;
        portENTER_CRITICAL(&trace_mux);
        if(released) {
            if(cursor<history.count) {entry=history.entries[cursor++];available=true;}
            else if(history.first_present && !summary_sent) {
                entry=history.first;summary_sent=true;available=summary=true;
                count=history.count;dropped=history.dropped;
            }
        }
        portEXIT_CRITICAL(&trace_mux);
        if(!available) return;
        const auto& m=entry.memory;
        const unsigned caps=entry.pointer ? ((esp_ptr_internal(entry.pointer)?1U:0U) |
            (esp_ptr_dma_capable(entry.pointer)?2U:0U) | (esp_ptr_external_ram(entry.pointer)?4U:0U)) : 0U;
        int n=snprintf(output,sizeof(output),
            "[S5A] n=%u ms=%lu step=%s edge=%s rc=%d(0x%x) fail=%u detail=%lu(0x%08lx) ptr=%p caps=%u total8=%lu int8=%lu largest8=%lu dma_free=%lu dma_largest=%lu min8=%lu psram=%lu%s\r\n",
            static_cast<unsigned>(cursor),(unsigned long)m.ms,entry.step,
            summary?"FIRST_FAILURE":(entry.before?"before":"after"),entry.result,unsigned(entry.result),unsigned(entry.failed),
            (unsigned long)entry.detail,(unsigned long)entry.detail,entry.pointer,caps,(unsigned long)m.total,(unsigned long)m.internal,
            (unsigned long)m.largest,(unsigned long)m.dma_free,(unsigned long)m.dma_largest,
            (unsigned long)m.minimum,(unsigned long)m.psram,summary?" retained=1":"");
        if(summary) n=snprintf(output,sizeof(output),
            "[S5A] FIRST_FAILURE step=%s rc=%d(0x%x) ms=%lu detail=%lu total8=%lu int8=%lu largest8=%lu dma_free=%lu dma_largest=%lu min8=%lu psram=%lu entries=%u dropped=%u trace_static=%u\r\n",
            entry.step,entry.result,unsigned(entry.result),(unsigned long)m.ms,(unsigned long)entry.detail,
            (unsigned long)m.total,(unsigned long)m.internal,(unsigned long)m.largest,(unsigned long)m.dma_free,
            (unsigned long)m.dma_largest,(unsigned long)m.minimum,(unsigned long)m.psram,
            unsigned(count),unsigned(dropped),unsigned(sizeof(history)+sizeof(output)));
        length=n>0?static_cast<size_t>(n):0;
        if(length>=sizeof(output)) length=sizeof(output)-1;
        sent=0;
    }
    // Exact existing IDF secondary USB-console FIFO/WR_DONE backend, also used
    // by the hardware-proven Stage 3D trace. Never install/reconfigure USB/IRQs.
    // At most one 64-byte packet; full endpoint retains the remainder for later.
    const auto amount=length-sent<64?length-sent:64;
    const auto written=usb_serial_jtag_ll_write_txfifo(
        reinterpret_cast<const uint8_t*>(output+sent),amount);
    if(written) {sent+=written;usb_serial_jtag_ll_txfifo_flush();}
}
}
#endif
