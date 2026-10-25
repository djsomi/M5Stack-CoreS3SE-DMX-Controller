#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <deque>
#include <vector>
#include <algorithm>
#if !defined(_WIN32)
#include <strings.h>
#endif
using esp_err_t = int;
using dmx_port_t = int;
using TaskHandle_t = int;
using TickType_t = uint32_t;
using portBASE_TYPE = int;
using QueueHandle_t = void*;
enum dmx_mode_t { DMX_MODE_READ, DMX_MODE_WRITE, DMX_MODE_MAX };
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_STATE = 1,
       ESP_ERR_INVALID_ARG = 2, ESP_ERR_TIMEOUT = 3, DMX_NUM_MAX = 2,
       DMX_MAX_PACKET_SIZE = 513, DMX_TX_MAX_BRK_TO_BRK_US = 1000000,
       DMX_RX_MAX_BRK_TO_BRK_US = 1250000, DMX_OK = 0,
       DMX_ERR_DATA_OVERFLOW, DMX_ERR_IMPROPER_SLOT, DMX_ERR_PACKET_SIZE,
       DMX_ERR_BUFFER_SIZE, UART_SIGNAL_TXD_INV = 1, pdFALSE = 0, pdTRUE = 1 };
struct dmx_event_t { unsigned size; int status, start_code, duration; struct { int brk, mab; } timing; };
#define IRAM_ATTR
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
#define portYIELD_FROM_ISR() ((void)0)
#define DMX_ALL_INTR_MASK UINT32_MAX
struct FakeUart {
    struct { uint32_t val = 0; } int_raw;
    uint32_t enabled = 0;
    bool idle = true, inject_done_in_write = false, inverted = false;
    bool fail_fifo_write = false, inject_clash_in_write = false;
    bool inject_done_on_enable = false;
    unsigned snapshot_reads = 0;
    int64_t now = 0;
    std::deque<uint8_t> fifo;
    std::vector<uint8_t> wire;
};
extern FakeUart hw;
extern bool fake_isr_context;
extern int fake_task;
static inline int64_t esp_timer_get_time() { return hw.now; }
static inline TaskHandle_t xTaskGetCurrentTaskHandle() { return fake_task; }
static inline bool xPortInIsrContext() { return fake_isr_context; }
static inline TickType_t xTaskGetTickCount() { return static_cast<TickType_t>(hw.now / 1000); }
static inline void vTaskDelay(int ticks) { hw.now += ticks * 1000; }
static inline void ets_delay_us(int us) { hw.now += us; }
static inline void xQueueSendFromISR(void*, void*, int*) {}
static inline void dmx_sniffer_disable(int) {}
#if defined(_WIN32)
static inline void bzero(void* p, size_t n) { memset(p, 0, n); }
#endif
static inline int get_brk_us(unsigned baud, unsigned bits) { return (bits * 1000000 + baud - 1) / baud; }
static inline int get_mab_us(unsigned baud, unsigned bits) { return get_brk_us(baud, bits); }
esp_err_t dmx_set_mode(dmx_port_t, dmx_mode_t);
esp_err_t dmx_write_packet(dmx_port_t, const void*, uint16_t);
esp_err_t dmx_send_packet(dmx_port_t, uint16_t);
esp_err_t dmx_wait_send_done(dmx_port_t, TickType_t);
