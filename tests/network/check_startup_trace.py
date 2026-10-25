"""Stage 5A source guards; no device/serial/network access."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[2]
net=(root/'src/network_service.cpp').read_text(encoding='utf-8')
trace=(root/'src/wifi_startup_trace.cpp').read_text(encoding='utf-8')
header=(root/'include/wifi_startup_trace.h').read_text(encoding='utf-8')
app=(root/'src/src.cpp').read_text(encoding='utf-8')
view=(root/'src/view_sender.h').read_text(encoding='utf-8')
assert '#ifndef DMX_STAGE5A_TRACE\n#define DMX_STAGE5A_TRACE 0\n#endif' in header
assert '#define DMX_STAGE5A_TRACE 1' not in header
assert 'History<64>' in trace and 'char output[448]' in trace
for caps in ['MALLOC_CAP_8BIT','MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT',
             'MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA','MALLOC_CAP_SPIRAM']:
    assert caps in trace
for api in ['esp_netif_init','esp_wifi_init','esp_wifi_set_mode','esp_wifi_start','httpd_start']:
    assert 'traced("'+api+'"' in net and '!strcmp(api,"'+api+'")' in net
assert 'const esp_err_t result=function();' in net and 'return result;' in net
assert 'm.internal<81920 || m.largest<49152 || m.dma<32768' in net
assert 'm.internal >= minimum_internal && m.dma >= minimum_dma' in net
assert 'minimum_internal = 32768, minimum_dma = 16384' in net
assert 's.http_stack<1024' in net and 's.worker_stack<1024' in net
for label in ['guard.preflight','guard.canvas','guard.worker.entry','guard.after_ap',
              'guard.after_http','guard.runtime.heap','guard.stack','guard.transport']:
    assert '"'+label+'"' in net
assert 'wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();cfg.nvs_enable=0;' in net
assert not re.search(r'cfg\.(static_rx_buf_num|dynamic_rx_buf_num|static_tx_buf_num|tx_buf_type|cache_tx_buf_num)\s*=',net)
assert 'fullscreen_canvas_memory::allocateOnce(' in view # Stage5B allocation is checked separately.
assert 'wifi_startup_trace::service();' in net[net.index('void ownerTick('):]
for forbidden in [r'Serial\.begin\(',r'Serial\.read\(',r'HWCDC\s*\(',r'usb_serial_jtag_driver_install\(',
                  r'esp_log_set_vprintf\(',r'\bmalloc\(',r'\bnew\s',r'vTaskDelay\(',r'while\s*\(']:
    assert not re.search(forbidden,trace),forbidden
assert trace.count('usb_serial_jtag_ll_write_txfifo(')==1
assert 'usb_serial_jtag_ll_txfifo_flush()' in trace and 'now-last_service<4000' in trace
assert 'length-sent<64?length-sent:64' in trace
assert 'credentials' not in trace and 'password' not in trace
assert app.index('"M5.begin.before"')<app.index('    M5.begin(cfg);')<app.index('"M5.begin.after"')
assert view.index('"fullscreen.before"')<view.index('getFullscreenCanvas(active_fullscreen_page')<view.index('"fullscreen.after"')
assert app.index('"dmx.startup.before"')<app.index('    dmx_driver_install(')<app.index('"dmx.startup.after"')
print('PASS Stage5A unchanged memory/config guards, exact API results, startup/canvas checkpoints, bounded task-only USB console output and no credentials/USB initialization')
