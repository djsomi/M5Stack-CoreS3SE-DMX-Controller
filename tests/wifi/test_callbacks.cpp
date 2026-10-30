// Actual HTTP-task retirement/probe callbacks, including an injected send stall.
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
int mux=0;
struct {unsigned work_started=0,work_completed=0;} ws_diag;
#define portENTER_CRITICAL(p) do {(void)(p);} while(0)
#define portEXIT_CRITICAL(p) do {(void)(p);} while(0)
enum class WsReason {wifi_reconfigure};enum class WsSource {worker};
struct Job {uint32_t epoch=1;bool busy=true,retire_requested=true,retired=false,probe=false;} wifi_job;
struct Session {int fd=-1;bool closing=false;} sessions[6];
unsigned sweeps=0,closes=0,change=0;
void sweep(void*) {++sweeps;if(change==1)wifi_job.busy=false;if(change==2)++wifi_job.epoch;}
void closeSession(Session& s,WsReason reason,WsSource source){assert(reason==WsReason::wifi_reconfigure&&source==WsSource::worker);s.closing=true;++closes;}
#include "callbacks_under_test.h"
int main(){
 const auto reset=[](){wifi_job=Job{};for(auto& s:sessions)s=Session{};sessions[0].fd=10;sessions[1].fd=11;sweeps=closes=change=0;ws_diag.work_started=ws_diag.work_completed=0;};
 const auto token=reinterpret_cast<void*>(uintptr_t(1));
 reset();wifiRetire(token);assert(sweeps==1&&closes==2&&wifi_job.retired);
 reset();wifi_job.epoch=2;wifiRetire(token);wifiProbe(token);assert(!sweeps&&!closes&&!wifi_job.retired&&!wifi_job.probe);assert(ws_diag.work_started==1&&ws_diag.work_completed==1);
 reset();wifi_job.busy=false;wifiRetire(token);assert(!sweeps&&!closes);
 for(unsigned interruption=1;interruption<=2;++interruption){reset();change=interruption;wifiRetire(token);assert(sweeps==1&&closes==0&&!wifi_job.retired);}
 reset();wifiProbe(token);assert(wifi_job.probe);wifi_job.probe=false;wifi_job.epoch=2;wifiProbe(token);assert(!wifi_job.probe);
 puts("PASS actual HTTP callbacks: matching retirement/readiness, stale queued epoch, abort/new job during blocking send, no late closure of fresh sessions");
}
