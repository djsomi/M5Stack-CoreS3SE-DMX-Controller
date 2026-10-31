#include "thdiff_experiment.h"
#if DMX_THDIFF_EXPERIMENT
#include <Arduino.h>
#include <M5Unified.hpp>
#include "physical_fader_diagnostics.h"
#include <stdio.h>
#include <string.h>

// Read-only UI capture query supplied by the application task.
extern bool thdiffTouchCaptured();

namespace thdiff {
namespace {
constexpr uint8_t original=0xA0, alternative=0x80;
const char* modeName(Mode m) {
    switch(m) {
        case Mode::blocked:return "BLOCKED";
        case Mode::baseline:return "A";
        case Mode::recovery:return "RECOVERY_REQUIRED";
        case Mode::arming:return "ARMING";
        case Mode::trial:return "B";
        case Mode::restored:return "A2";
        case Mode::fault:return "RESTORE_UNVERIFIED";
    }
    return "UNKNOWN";
}
}

bool Experiment::check() {
    actual_=Result{};write_ok_=-1;write_rc_=0;
    if(!backend_.ready()){actual_.error=-1;return false;}
    const uint8_t regs[]={0xA3,0xA6,0xA8}, values[]={0x64,0x03,0x20};
    for(unsigned i=0;i<3;++i) {
        auto r=backend_.read(regs[i]);
        if(!r.ok || r.value!=values[i]){actual_.error=r.ok?-2:r.error;return false;}
    }
    actual_=backend_.read(0x85);return actual_.ok;
}
bool Experiment::emit(const char* name,uint32_t now,bool status_ready) {
    if(count_==capacity){++dropped_;return false;}
    Event e;e.name=name;e.seq=++sequence_;e.ms=now;e.mode=mode_;e.actual=actual_;
    e.write_ok=write_ok_;e.write_rc=write_rc_;e.attempts=attempts_;
    e.restore=restore_;e.original_valid=original_valid_;e.status_ready=status_ready;
    if(mode_==Mode::trial && uint32_t(now-trial_at_)<duration_ms)e.remaining=duration_ms-uint32_t(now-trial_at_);
    events_[(head_+count_)%capacity]=e;++count_;return true;
}
void Experiment::begin(const touch_configuration::Snapshot& s,uint32_t now) {
    if(begun_)return;begun_=true;
    if(s.attempted && s.identity_ok && s.context.admission==touch_configuration::Admission::ready && backend_.ready()) {
        for(const auto& e:s.entries)if(e.address==0x85 && e.status==touch_configuration::Status::ok) {
            actual_.ok=true;actual_.value=e.value;
            if(e.value==original){mode_=Mode::baseline;original_valid_=true;}
            else{mode_=Mode::recovery;restore_=true;}
        }
    }
    emit("BOOT_READ_ONLY",now);
}
void Experiment::restore(const char* reason,uint32_t now) {
    handshake_=false;barrier_=false;
    if(attempts_>=2)return;
    cleanup_=true;restore_=true;emit(reason,now);
    while(attempts_<2) {
        ++attempts_;
        // Do not blindly write to an unverified identity after a bus error.
        const bool identified=check();
        if(identified) {
            auto w=backend_.write(original);write_ok_=w.ok?1:0;write_rc_=w.error;
            actual_=backend_.read(0x85);
        }
        emit("RESTORE_ATTEMPT",now);
        // Readback is decisive, even if the write reported an ambiguous error.
        if(identified && actual_.ok && actual_.value==original) {
            restore_=false;cleanup_=false;used_=true;mode_=Mode::restored;
            emit("RESTORED_VERIFIED",now);return;
        }
    }
    mode_=Mode::fault;emit("RESTORE_UNVERIFIED",now);
}
void Experiment::error(const char* reason,uint32_t now) {
    if(cleanup_ && restore_ && mode_!=Mode::fault)restore(reason,now);
    else if(count_<capacity-4)emit(reason,now);
    else ++dropped_;
}
void Experiment::command(const char* command,uint32_t now,bool touch) {
    if(!begun_)return;
    // Reserve four slots: reason + two restore attempts + terminal result.
    if(count_>capacity-5){error("OUTPUT_BACKPRESSURE",now);return;}
    if(!strcmp(command,"THDIFF RESTORE")) {
        if(mode_==Mode::fault || attempts_>=2){emit("RESTORE_BUDGET_EXHAUSTED",now);return;}
        if(mode_==Mode::blocked){emit("NO_VERIFIED_CONTEXT",now);return;}
        if(mode_==Mode::baseline || mode_==Mode::restored) {
            if(check() && actual_.value==original){emit("ALREADY_A_VERIFIED",now);return;}
            if(mode_==Mode::baseline)mode_=Mode::recovery;
        }
        restore("MANUAL_RESTORE",now);return;
    }
    if(!strcmp(command,"THDIFF STATUS")) {
        if(touch){emit("BUSY_TOUCH",now);return;}
        const bool ok=check();
        if(mode_==Mode::trial && (!ok || actual_.value!=alternative)) {restore("STATUS_MISMATCH",now);return;}
        if(mode_==Mode::baseline && (!ok || actual_.value!=original)) {
            mode_=ok?Mode::recovery:Mode::blocked;restore_=ok;handshake_=false;
        }
        emit("STATUS",now,ok && mode_==Mode::baseline && actual_.value==original);return;
    }
    if(strcmp(command,"THDIFF APPLY 0x80")){error("INVALID_COMMAND",now);return;}
    if(mode_!=Mode::baseline || used_){error("APPLY_NOT_ALLOWED",now);return;}
    if(touch){emit("BUSY_TOUCH",now);return;}
    if(!handshake_){emit("STATUS_HANDSHAKE_REQUIRED",now);return;}
    handshake_=false;
    if(!check() || actual_.value!=original){mode_=Mode::recovery;restore_=true;emit("BASELINE_MISMATCH",now);return;}
    original_valid_=true;restore_=true;cleanup_=true;used_=true;
    mode_=Mode::arming;armed_at_=now;barrier_=false;
    emit("RESTORE_REQUIRED_BEFORE_WRITE",now);
}
void Experiment::poll(uint32_t now,bool touch,bool connected) {
    if(!connected) {
        handshake_=false;
        if(cleanup_ && restore_ && mode_!=Mode::fault)restore("USB_LOST",now);
        return;
    }
    if(mode_==Mode::trial && uint32_t(now-trial_at_)>=duration_ms) {restore("B_TIMEOUT_120S",now);return;}
    if(mode_!=Mode::arming)return;
    if(touch){restore("TOUCH_BEFORE_WRITE",now);return;}
    if(uint32_t(now-armed_at_)>=arm_timeout_ms){restore("ARM_OUTPUT_TIMEOUT",now);return;}
    if(!barrier_)return;
    // Two B-result events must still leave four slots for worst-case rollback.
    if(count_>capacity-6){restore("OUTPUT_BACKPRESSURE",now);return;}
    if(!check() || actual_.value!=original){restore("PRE_WRITE_BASELINE_FAILED",now);return;}
    // The restoration flag/event precede this first potentially effective write.
    trial_at_=now;
    auto w=backend_.write(alternative);write_ok_=w.ok?1:0;write_rc_=w.error;
    actual_=backend_.read(0x85);
    emit("B_WRITE_READBACK",now);
    if(!w.ok || !actual_.ok || actual_.value!=alternative){restore("B_VERIFY_FAILED",now);return;}
    mode_=Mode::trial;emit("B_VERIFIED",now);
}
void Experiment::exit(uint32_t now) {
    if(cleanup_ && restore_ && mode_!=Mode::fault)restore("MANAGED_EXIT",now);
}
void Experiment::sent() {
    if(!count_)return;
    const auto& e=events_[head_];
    if(!strcmp(e.name,"STATUS") && e.status_ready && mode_==Mode::baseline)handshake_=true;
    if(!strcmp(e.name,"RESTORE_REQUIRED_BEFORE_WRITE") && mode_==Mode::arming)barrier_=true;
    head_=(head_+1)%capacity;--count_;
}

namespace {
class DeviceBackend final:public Backend {
 public:
    bool ready() const override {
        const auto& c=touch_configuration::startupSnapshot().context;
        if(c.admission!=touch_configuration::Admission::ready || M5.getBoard()!=m5::board_t::board_M5StackCoreS3SE || !M5.Touch.isEnabled())return false;
        const auto* panel=M5.Display.panel();const auto* driver=panel?panel->getTouch():nullptr;
        if(!driver)return false;const auto cfg=driver->config();
        return cfg.pin_cs<0 && cfg.i2c_port==c.port && cfg.i2c_addr==0x38 && c.address==0x38
            && cfg.freq==c.frequency && c.frequency && c.frequency<=400000
            && cfg.pin_sda==c.sda && cfg.pin_scl==c.scl && c.port==M5.In_I2C.getPort()
            && c.sda==M5.In_I2C.getSDA() && c.scl==M5.In_I2C.getSCL();
    }
    Result read(uint8_t reg) override {
        Result r;if(!ready()){r.error=1;return r;}
        if(reg!=0x85 && reg!=0xA3 && reg!=0xA6 && reg!=0xA8){r.error=1;return r;}
        const auto& c=touch_configuration::startupSnapshot().context;
        const auto value=lgfx::i2c::readRegister8(c.port,0x38,reg,c.frequency);
        r.ok=value.has_value();if(r.ok)r.value=value.value();else r.error=int(value.error());return r;
    }
    Result write(uint8_t value) override {
        Result r;if(!ready() || (value!=original && value!=alternative)){r.error=1;return r;}
        const auto& c=touch_configuration::startupSnapshot().context;
        const auto result=lgfx::i2c::writeRegister8(c.port,0x38,0x85,value,0,c.frequency);
        r.ok=result.has_value();if(!r.ok)r.error=int(result.error());return r;
    }
};
DeviceBackend backend;
Experiment experiment(backend);
char rx[32];size_t rx_length=0;
bool rx_bad=false,skip_lf=false,rx_fault=false;
uint32_t rx_started=0;
bool touching() {
    if(thdiffTouchCaptured())return true;
#if DMX_FADER_DIAGNOSTICS
    if(physical_fader::touchActive())return true;
#endif
    for(size_t i=0;i<M5.Touch.getCount() && i<M5.Touch.TOUCH_MAX_POINTS;++i)
        if(M5.Touch.getDetail(i).isPressed())return true;
    return false;
}
}
void begin() {experiment.begin(touch_configuration::startupSnapshot(),millis());}
void poll() {
    const uint32_t now=millis();const bool touch=touching(),connected=bool(Serial);
    experiment.poll(now,touch,connected);
    if(!connected){rx_length=0;rx_bad=skip_lf=rx_fault=false;return;}
    if((rx_length || rx_bad) && uint32_t(now-rx_started)>=5000) {
        rx_length=0;rx_bad=false;experiment.error("RX_LINE_TIMEOUT",now);
    }
    // HWCDC uses zero-wait queue reads; never wait for a whole command/line.
    for(unsigned n=0;n<32;++n) {
        const int available=Serial.available();
        if(available<0){if(!rx_fault)experiment.error("USB_RX_UNAVAILABLE",now);rx_fault=true;return;}
        if(!available)return;
        const int c=Serial.read();if(c<0){if(!rx_fault)experiment.error("USB_RX_ERROR",now);rx_fault=true;return;}
        rx_fault=false;
        if(skip_lf && c=='\n'){skip_lf=false;continue;}skip_lf=false;
        if(c=='\r' || c=='\n') {
            skip_lf=c=='\r';
            if(rx_bad)experiment.error("INVALID_OR_LONG_LINE",now);
            else if(rx_length){rx[rx_length]=0;experiment.command(rx,now,touch);}
            rx_length=0;rx_bad=false;return; // At most one command per owner turn.
        }
        if(!rx_length && !rx_bad)rx_started=now;
        if(c<32 || c>126 || rx_length==sizeof(rx)-1)rx_bad=true;
        if(!rx_bad)rx[rx_length++]=char(c);
    }
}
bool pending() {return experiment.front()!=nullptr;}
size_t formatNext(char* out,size_t cap) {
    const auto* e=experiment.front();if(!e || !cap)return 0;
    char value[8],decimal[5];
    if(e->actual.ok){snprintf(value,sizeof(value),"0x%02x",e->actual.value);snprintf(decimal,sizeof(decimal),"%u",e->actual.value);}
    else{snprintf(value,sizeof(value),"NA");snprintf(decimal,sizeof(decimal),"NA");}
    const int n=snprintf(out,cap,"[THDIFF] seq=%lu t_ms=%lu event=%s mode=%s reg=0x85 original=0xA0 original_valid=%u trial=0x80 restore_required=%u hex=%s dec=%s read_ok=%u read_rc=%d write_ok=%d write_rc=%d restore_attempts=%u remaining_ms=%lu apply_ready_after_line=%u dropped=%u\n",
        (unsigned long)e->seq,(unsigned long)e->ms,e->name,modeName(e->mode),e->original_valid,e->restore,value,decimal,
        e->actual.ok,e->actual.error,e->write_ok,e->write_rc,e->attempts,(unsigned long)e->remaining,e->status_ready,experiment.dropped());
    return n>0?(size_t(n)<cap?size_t(n):cap-1):0;
}
void lineSent() {experiment.sent();}
void beforeExit() {experiment.exit(millis());}
void outputError() {experiment.error("USB_TX_ERROR",millis());}
}
#endif
