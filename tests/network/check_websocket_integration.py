"""Stage6A source/task guards + extract actual endpoint/I/O for native API mocks."""
from pathlib import Path
from preset_loaded_guard_support import prior
import argparse,subprocess,re
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--adapter-header',type=Path)
a=p.parse_args()
def read(name):return (root/name).read_text(encoding='utf-8')
net=read('src/network_service.cpp');app=read('src/src.cpp')
bridge=read('src/websocket_bridge.cpp');service=read('src/websocket_service.cpp')
baseline='7f09d4c3314d0444a11786aea9ba86793842c9bb'
def old(name):return subprocess.check_output(['git','-c','safe.directory='+root.as_posix(),'show',baseline+':'+name],cwd=root).decode('utf-8')
for name in ['src/view_sender.h','src/view_receiver.h','src/dmx_controller.cpp','src/dmx_controller.h',
             'src/command_dispatcher.cpp','src/command_dispatcher.h','src/dmx_transport.cpp','src/dmx_transport.h',
             'src/dmx_transport_esp_dmx.h','src/preset_store.h','platformio.ini','include/dmx_hardware.h',
             'scripts/esp_dmx_tx_patch.py','patches/esp_dmx_v202/tx_service.h',
             'patches/esp_dmx_v202/tx_core.h','scripts/package_firmware.py','partitions/cores3se_16mb_ota.csv']:
    assert prior(name,read(name))==old(name),name
for folder in ['patches']:
    for path in (root/folder).rglob('*'):
        if path.is_file():assert path.read_bytes()==subprocess.check_output(['git','-c','safe.directory='+root.as_posix(),'show',baseline+':'+path.relative_to(root).as_posix()],cwd=root),str(path)
# Stage6B replaces only the approved web assets; freeze every Stage6A firmware
# input and validate actual composed/gzip outputs instead of old placeholder bytes.
import sys
sys.path.insert(0,str(root/'tests/web'))
from check_scope import check
check()
sys.path.insert(0,str(root/'scripts'))
from web_asset_pipeline import verify
verify(root)
assert 'dispatcher_.setCompletion(completed,this)' in bridge
assert 'setObserver(' not in bridge+service and 'setPublisher(' not in bridge+service and 'setAdmission(' not in bridge+service
assert not any(s in net for s in ['DmxController','CommandDispatcher','DmxTransport','M5.','Serial.','HWCDC','dmx_write','dmx_send'])
assert 'dispatcher_.submit(' not in bridge.split('void Bridge::ownerPoll',1)[0]
assert 'controller_.' not in bridge.split('Event Bridge::state',1)[0]
assert 'case CommandStatus::accepted:return "accepted"' in read('src/websocket_protocol.cpp')
assert 'status==CommandStatus::applied||status==CommandStatus::no_change?Kind::ack:Kind::error' in bridge
assert app.index('websocket_service::initialize(')<app.index('network_service::launch(')
loop=app.split('void loop(void)',1)[1]
assert loop.index('setSenderActive(')<loop.index('websocket_service::ownerPoll(')<loop.index('command_dispatcher.process();')<loop.index('return;')
owner=bridge.split('void Bridge::ownerPoll',1)[1].split('void Bridge::completed',1)[0]
assert 'dispatcher_.submit(' in owner and 'in_flight_=true' in owner
assert 'max_sessions = 6' in net and 'max_sessions+3<=CONFIG_LWIP_MAX_SOCKETS' in net
assert 'ws.is_websocket=true;ws.handle_ws_control_frames=true;' in net
assert net.index('httpd_register_uri_handler(server,&ws)')<net.index('httpd_register_uri_handler(server,&route)')
assert 'httpd_ws_send_frame_async' in net and 'httpd_ws_recv_frame' in net and 'httpd_ws_get_fd_info' in net
assert 'httpd_queue_work' not in service+bridge and 'portENTER_CRITICAL' in service
assert 'new(storage)' in service and 'malloc(' not in bridge+net
assert 'vTaskDelay(pdMS_TO_TICKS(20))' in net and 'maintenance<1000000' in net
assert 'static char json[4096]' in net and 'char ws_output[websocket_bridge::max_json]' in net
assert 'deadline-esp_timer_get_time()' in net and 'done+=size_t(n)' in net
assert 'if(left<1000)' in net  # Never turn a sub-ms remaining budget into lwIP's unlimited zero timeout.
assert 'websocket_diagnostics::rejection(ws_diag' in net
assert 's->wire.wsBytes(data,done)' in net and 's->wire.httpBytes(data,size_t(n))' in net and 's->wire.trailing()' in net
assert "connect-src 'self' ws://192.168.4.1;" in net
assert 'now-s.activity>=60000000' in net and 'now-s.ping>=20000000' in net
assert 'websocket_bridge::drain(max_sessions,first' in net
assert 'outbox_capacity=8' in read('include/websocket_bridge.h') and 'pump_rounds=4' in read('include/websocket_pump.h')
assert 'pump_budget_us=20000' in read('include/websocket_pump.h')
assert 'WS_FIRST_FAILURE' in net and '++ws_diag.wifi_disconnects' in net
assert '"/ws-diagnostics"' in net and 'native_evictions\\\":null' in read('src/websocket_diagnostics.cpp')
if a.adapter_header:
    target=a.adapter_header.resolve();assert (root/'dist').resolve() in target.parents
    def function(signature, source=net):
        start=source.index(signature);opening=source.index('{',start);depth=0
        # Functions under test contain no braces in strings/comments.
        for i in range(opening,len(source)):
            if source[i]=='{':depth+=1
            if source[i]=='}':
                depth-=1
                if not depth:return source[start:i+1]+'\n'
        raise AssertionError(signature)
    signatures=['void recordClosure(','int ioFailure(','int io(','bool wsSend(','bool header(','esp_err_t handlerFailure(','esp_err_t websocket(']
    # Exercise the real HTTP accounting alongside the extracted socket override.
    budget='namespace stage5 {\n'+function('bool Budget::allow(',read('src/http_policy.cpp'))+'}\n'
    target.write_text(budget+''.join(function(s) for s in signatures),encoding='utf-8')
print('PASS Stage6A exact Stage5 domain/UI/driver/assets/layout preservation; owner-only Dispatcher bridge; native endpoint/socket/queue/stack guards')
