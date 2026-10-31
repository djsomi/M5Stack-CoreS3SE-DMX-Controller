#pragma once
#include "touch_configuration_snapshot.h"
#ifndef DMX_THDIFF_EXPERIMENT
#define DMX_THDIFF_EXPERIMENT 0
#endif
#ifndef DMX_THDIFF_AUTHORIZED_INVESTIGATION
#define DMX_THDIFF_AUTHORIZED_INVESTIGATION 0
#endif
#if DMX_THDIFF_EXPERIMENT && (!DMX_THDIFF_AUTHORIZED_INVESTIGATION || !DMX_HW_DIAGNOSTICS || !DMX_TOUCH_CONFIG_DIAGNOSTICS)
#error "Suspended THDIFF experiment requires a separately authorized investigation configuration"
#endif

namespace thdiff {
#if DMX_THDIFF_EXPERIMENT
using Result = touch_configuration::ReadResult;
enum class Mode : uint8_t { blocked, baseline, recovery, arming, trial, restored, fault };
struct Backend {
    virtual ~Backend() {}
    virtual bool ready() const = 0;
    virtual Result read(uint8_t reg) = 0;
    virtual Result write(uint8_t value) = 0; // Implementation fixes address to 0x85.
};
struct Event {
    const char* name = "none";
    uint32_t seq=0, ms=0, remaining=0;
    Mode mode=Mode::blocked;
    Result actual;
    int write_rc=0;
    int8_t write_ok=-1;
    uint8_t attempts=0;
    bool restore=false, original_valid=false, status_ready=false;
};
class Experiment {
 public:
    static constexpr uint32_t duration_ms=120000, arm_timeout_ms=5000;
    static constexpr size_t capacity=8;
    explicit Experiment(Backend& backend):backend_(backend) {}
    void begin(const touch_configuration::Snapshot&, uint32_t now);
    void command(const char*, uint32_t now, bool touch);
    void poll(uint32_t now, bool touch, bool connected);
    void error(const char* reason, uint32_t now);
    void exit(uint32_t now);
    const Event* front() const {return count_?&events_[head_]:nullptr;}
    void sent(); // Only after the complete front line was accepted by USB TX.
    Mode mode() const {return mode_;}
    bool restoreRequired() const {return restore_;}
    unsigned attempts() const {return attempts_;}
    unsigned dropped() const {return dropped_;}
 private:
    bool check();
    void restore(const char*, uint32_t);
    bool emit(const char*, uint32_t, bool status_ready=false);
    Backend& backend_;
    Event events_[capacity];
    Result actual_;
    Mode mode_=Mode::blocked;
    size_t head_=0,count_=0;
    uint32_t sequence_=0, armed_at_=0, trial_at_=0;
    unsigned dropped_=0;
    uint8_t attempts_=0;
    bool begun_=false, original_valid_=false, restore_=false, cleanup_=false;
    bool handshake_=false, barrier_=false, used_=false;
    int8_t write_ok_=-1;
    int write_rc_=0;
};
void begin();
void poll();
bool pending();
size_t formatNext(char*,size_t);
void lineSent();
void beforeExit();
void outputError();
#else
inline void begin() {} inline void poll() {} inline bool pending() {return false;}
inline size_t formatNext(char*,size_t) {return 0;} inline void lineSent() {}
inline void beforeExit() {}
inline void outputError() {}
#endif
}
