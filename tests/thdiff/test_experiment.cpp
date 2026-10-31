#include "thdiff_experiment.h"
#include "Arduino.h"
#include "M5Unified.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
uint32_t fake_us=1000000;
FakeM5 M5;FakeSerial Serial;bool captured=false,physical_touch=false;
bool thdiffTouchCaptured(){return captured;}
namespace physical_fader {bool touchActive(){return physical_touch;}bool service(){return false;}}
#if DMX_THDIFF_EXPERIMENT
using namespace thdiff;
using touch_configuration::Snapshot;
using touch_configuration::Status;
struct Mock:Backend {
    uint8_t value=0xA0;bool context=true,identity=true,b_error=false,b_read_error=false,b_wrong=false;
    int fail_restores=0,fail_restore_reads=0;bool b_written=false;
    std::vector<uint8_t> writes;unsigned reads=0;
    bool ready()const override{return context;}
    Result read(uint8_t reg)override{
        ++reads;Result r;r.ok=true;
        if(reg==0xA3)r.value=identity?0x64:0;
        else if(reg==0xA6)r.value=3;
        else if(reg==0xA8)r.value=0x20;
        else {assert(reg==0x85);r.value=value;
            if(b_written && value==0x80 && b_read_error){r.ok=false;r.error=2;b_read_error=false;}
            if(value==0xA0 && fail_restore_reads>0){--fail_restore_reads;r.ok=false;r.error=2;}
        }return r;
    }
    Result write(uint8_t v)override{
        assert(v==0x80 || v==0xA0);writes.push_back(v);Result r;r.ok=true;
        if(v==0x80){value=b_wrong?0x90:v;b_written=true;r.ok=!b_error;}
        else if(fail_restores>0){--fail_restores;r.ok=false;}else value=v;
        if(!r.ok)r.error=2;return r;
    }
};
Snapshot snap(uint8_t value=0xA0){Snapshot s;s.attempted=s.identity_ok=true;s.context.admission=touch_configuration::Admission::ready;s.entries[0].address=0x85;s.entries[0].value=value;s.entries[0].status=Status::ok;return s;}
void drain(Experiment& e){while(e.front())e.sent();}
void handshake(Experiment& e,uint32_t now=1){drain(e);e.command("THDIFF STATUS",now,false);drain(e);}
void start(Experiment& e,uint32_t now=10){handshake(e,now);e.command("THDIFF APPLY 0x80",now,false);assert(e.mode()==Mode::arming && e.restoreRequired());drain(e);e.poll(now+1,false,true);assert(e.mode()==Mode::trial);drain(e);}
void core() {
    Mock m;Experiment e(m);auto s=snap();e.begin(s,0);assert(m.writes.empty());
    e.command("THDIFF APPLY 0x80",1,false);assert(m.writes.empty());drain(e);
    handshake(e);e.command("THDIFF APPLY 0x80",2,true);assert(m.writes.empty());drain(e);
    e.command("THDIFF APPLY 0x80",3,false);assert(e.restoreRequired() && m.writes.empty());
    e.poll(4,false,true);assert(m.writes.empty());drain(e);e.poll(5,false,true);assert(m.value==0x80 && e.mode()==Mode::trial);drain(e);
    e.poll(120004,true,true);assert(e.mode()==Mode::trial);e.poll(120005,true,true);
    assert(e.mode()==Mode::restored && !e.restoreRequired() && m.value==0xA0 && e.attempts()==1);
    drain(e);handshake(e);e.command("THDIFF APPLY 0x80",120010,false);assert(m.writes.size()==2);
    e.exit(120011);assert(m.writes.size()==2); // idempotent cleanup
    for(const char* why:{"MANUAL","USB","EXIT","INVALID","TX","OVERFLOW"}) {
        Mock b;Experiment t(b);t.begin(s,0);start(t);
        if(!strcmp(why,"MANUAL"))t.command("THDIFF RESTORE",100,false);
        else if(!strcmp(why,"USB"))t.poll(100,true,false);
        else if(!strcmp(why,"EXIT"))t.exit(100);
        else if(!strcmp(why,"INVALID"))t.command("nonsense",100,false);
        else if(!strcmp(why,"OVERFLOW")){for(int i=0;i<9;++i)t.command("THDIFF STATUS",100,false);}
        else t.error("USB_TX_ERROR",100);
        assert(t.mode()==Mode::restored && b.value==0xA0 && t.attempts()==1 && !t.restoreRequired());
        if(strcmp(why,"OVERFLOW"))assert(!t.dropped());
        bool verified=false;while(t.front()){verified|=!strcmp(t.front()->name,"RESTORED_VERIFIED");t.sent();}assert(verified);
    }
    for(int type=0;type<3;++type) {
        Mock b;b.b_error=type==0;b.b_read_error=type==1;b.b_wrong=type==2;Experiment t(b);t.begin(s,0);handshake(t);t.command("THDIFF APPLY 0x80",10,false);drain(t);t.poll(11,false,true);
        assert(t.mode()==Mode::restored && b.value==0xA0 && b.writes.size()==2);
    }
    for(bool recover:{true,false}) {
        Mock b;Experiment t(b);t.begin(s,0);start(t);b.fail_restores=recover?1:9;
        t.command("THDIFF RESTORE",30,false);assert(t.attempts()==2 && !t.dropped());
        assert(t.mode()==(recover?Mode::restored:Mode::fault));
        auto n=b.writes.size();drain(t);t.command("THDIFF RESTORE",31,false);drain(t);t.command("THDIFF APPLY 0x80",32,false);t.poll(1000000,false,true);t.exit(1000001);assert(b.writes.size()==n);
    }
    Mock absent;absent.value=0x80;Experiment recovery(absent);recovery.begin(snap(0x80),0);
    assert(recovery.mode()==Mode::recovery);recovery.poll(200000,false,false);recovery.exit(200001);assert(absent.writes.empty());drain(recovery);
    recovery.command("THDIFF APPLY 0x80",200002,false);assert(absent.writes.empty());drain(recovery);
    recovery.command("THDIFF RESTORE",200003,false);assert(recovery.mode()==Mode::restored && absent.value==0xA0);
    Mock reset;Experiment new_boot(reset);new_boot.begin(snap(),0);assert(new_boot.mode()==Mode::baseline && reset.writes.empty());
    Mock bad;bad.identity=false;Experiment invalid(bad);invalid.begin(s,0);handshake(invalid);invalid.command("THDIFF APPLY 0x80",2,false);assert(bad.writes.empty());
    Mock changed;Experiment lost(changed);lost.begin(s,0);start(lost);changed.identity=false;lost.command("THDIFF STATUS",100,false);assert(lost.mode()==Mode::fault && lost.attempts()==2 && changed.writes.size()==1);
    Mock hold;Experiment arm(hold);arm.begin(s,0);handshake(arm);arm.command("THDIFF APPLY 0x80",10,false);arm.poll(5010,false,true);assert(hold.value==0xA0 && hold.writes.size()==1);
    Mock flip;Experiment touched(flip);touched.begin(s,0);handshake(touched);touched.command("THDIFF APPLY 0x80",10,false);drain(touched);touched.poll(11,true,true);assert(flip.value==0xA0 && flip.writes.size()==1);
    Mock wrap;Experiment rollover(wrap);rollover.begin(s,0);start(rollover,0xFFFF0000U);rollover.poll(uint32_t(0xFFFF0001U+120000U),true,true);assert(rollover.mode()==Mode::restored);
    Mock congested;congested.fail_restores=9;Experiment queue(congested);queue.begin(s,0);handshake(queue);
    queue.command("THDIFF APPLY 0x80",10,false);
    for(int i=0;i<3;++i)queue.command("THDIFF STATUS",11,false);
    queue.sent(); // Barrier emitted, three STATUS replies remain behind it.
    congested.value=0x80; // Inject a state that unsuccessful rollback cannot confirm.
    queue.poll(12,false,true);assert(queue.mode()==Mode::fault && queue.attempts()==2 && !queue.dropped());
    bool terminal=false;while(queue.front()){terminal|=!strcmp(queue.front()->name,"RESTORE_UNVERIFIED");queue.sent();}assert(terminal);
    for(auto value:congested.writes)assert(value==0xA0); // Never start B without rollback log capacity.
    puts("PASS core: baseline/handshake/barrier, touch admission, 120s/wrap, manual/exit/disconnect/errors, bounded queue/restore attempts, fault latch, readonly reboot/recovery and no replay");
}
namespace io {
uint8_t bytes[256]={},selected=0;bool locked=false;unsigned starts=0;int fail_stop=0;
std::vector<uint8_t> writes;bool reject_restore=false;
}
namespace lgfx {namespace i2c {
cpp::Result beginTransaction(int port,int address,uint32_t hz,bool read){assert(port==0 && address==0x38 && hz==400000 && !read && !io::locked);io::locked=true;++io::starts;return {};}
cpp::Result writeBytes(int,const uint8_t* data,size_t n){assert(io::locked && (n==1 || n==2));io::selected=data[0];
    if(n==2){assert(data[0]==0x85 && (data[1]==0x80 || data[1]==0xA0));io::writes.push_back(data[1]);if(data[1]==0xA0 && io::reject_restore)return cpp::fail(connection_lost);io::bytes[0x85]=data[1];}return {};}
cpp::Result restart(int,int,uint32_t,bool){assert(io::locked);return {};}
cpp::Result readBytes(int,uint8_t* data,size_t n,bool){assert(io::locked && n==1);*data=io::bytes[io::selected];return {};}
cpp::Result endTransaction(int){assert(io::locked);io::locked=false;if(io::selected==0x85 && io::fail_stop){--io::fail_stop;return cpp::fail(connection_lost);}return {};}
#include "io_under_test.h"
}}
namespace hardware_diagnostics {
char line[768];size_t line_sent=0,line_size=0;bool banner_pending=false,thdiff_line=false;
unsigned queued=0;uint32_t last_tx_ms=0;constexpr size_t tx_chunk_bytes=40;constexpr uint32_t tx_interval_ms=4;
void prepareLine(){thdiff_line=thdiff::pending();line_size=thdiff_line?thdiff::formatNext(line,sizeof(line)):touch_configuration::formatNext(line,sizeof(line));line_sent=0;}
#include "service_under_test.h"
}
void pump(unsigned count=2000){for(unsigned i=0;i<count;++i){fake_us+=4000;hardware_diagnostics::service();}}
void command(const std::string& s){Serial.rx+=s;pump();}
#endif
int main(int argc,char** argv) {
#if DMX_THDIFF_EXPERIMENT
    core();std::string mode=argc>1?argv[1]:"ready";
    io::bytes[0xA3]=0x64;io::bytes[0xA6]=3;io::bytes[0xA8]=0x20;io::bytes[0x85]=mode=="recovery"?0x80:0xA0;
    touch_configuration::captureStartup();thdiff::begin();pump();assert(io::writes.empty());
    command("THDIFF STATUS\r\n");assert(Serial.tx.find("event=STATUS")!=std::string::npos);
    if(mode=="recovery") {
        command("THDIFF APPLY 0x80\n");assert(io::writes.empty());command("THDIFF RESTORE\n");assert(io::bytes[0x85]==0xA0);
    } else {
        if(mode=="capture"){captured=true;command("THDIFF APPLY 0x80\n");assert(io::writes.empty());captured=false;command("THDIFF STATUS\n");}
        if(mode=="touch"){M5.Touch.count=1;M5.Touch.details[0].pressed=true;command("THDIFF APPLY 0x80\n");assert(io::writes.empty());M5.Touch.count=0;command("THDIFF STATUS\n");}
        command("THDIFF APPLY 0x80\n");assert(io::bytes[0x85]==0x80 && io::writes.size()==1);
        assert(Serial.tx.find("event=RESTORE_REQUIRED_BEFORE_WRITE")<Serial.tx.find("event=B_VERIFIED"));
        if(mode=="restore_fail") {
            io::reject_restore=true;command("THDIFF RESTORE\n");assert(io::writes.size()==3 && io::bytes[0x85]==0x80);
            assert(Serial.tx.find("event=RESTORE_UNVERIFIED")!=std::string::npos);
            command("THDIFF RESTORE\n");command("THDIFF APPLY 0x80\n");thdiff::beforeExit();assert(io::writes.size()==3);return 0;
        }
        if(mode=="disconnect"){Serial.connected=false;pump(1);Serial.connected=true;pump();}
        else if(mode=="expiry"){physical_touch=true;fake_us+=120000000;pump(1);physical_touch=false;pump();}
        else if(mode=="exit"){thdiff::beforeExit();pump();}
        else if(mode=="overflow")command(std::string(100,'X')+"\n");
        else if(mode=="partial"){Serial.rx+="THDIFF AP";pump(1);fake_us+=5000000;pump();}
        else if(mode=="badbyte")command(std::string("THDIFF\0bad\n",11));
        else if(mode=="txerror"){thdiff::outputError();pump();}
        else if(mode=="txzero"){Serial.rx+="THDIFF STATUS\n";Serial.write_zero=true;pump(8);Serial.write_zero=false;pump();}
        else if(mode=="rxerror"){Serial.rx+="THDIFF STATUS\n";Serial.read_error=true;pump(3);Serial.read_error=false;pump();}
        else if(mode=="rxunavailable"){Serial.unavailable=true;pump(3);Serial.unavailable=false;pump();}
        else command("THDIFF RESTORE\n");
        assert(io::bytes[0x85]==0xA0 && io::writes.size()==2);
    }
    assert(!io::locked && Serial.max_write<=40);
    assert(Serial.tx.find("event=RESTORED_VERIFIED")!=std::string::npos);
    auto writes=io::writes.size();command("THDIFF APPLY 0x80\n");thdiff::beforeExit();assert(io::writes.size()==writes);
    printf("PASS real adapter/installed I2C helpers/actual diagnostic service %s: USB RX+partial TX, event before B, capture/any-touch, error/expiry/exit recovery, exact 0x85-only writes and no reinitialization\n",mode.c_str());
#else
    (void)argc;(void)argv;thdiff::begin();thdiff::poll();thdiff::beforeExit();char out[1];assert(!thdiff::pending() && !thdiff::formatNext(out,1));puts("PASS production/opt-out: no experiment code, USB reads or writes");
#endif
}
