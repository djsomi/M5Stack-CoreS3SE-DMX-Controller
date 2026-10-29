#pragma once
#include <stdint.h>

// Application-task owner. Browser confirmation schedules a cancellation window,
// not authentication. No power-source checks or hardware calls in network tasks.
class DeviceMaintenance {
 public:
    static constexpr uint32_t countdown_ms=5000;
    enum class Action : uint8_t { none, restart, power_off };
    enum class Phase : uint8_t { idle, countdown, zeroing, quiescing, executing, failed };
    enum class Reason : uint8_t { none, busy, cancelled, transport, deadline, returned };
    enum class Result : uint8_t { invalid, accepted, cancelled, unauthorized, busy, unsupported };
    enum class Step : uint8_t { pending, ready, failed };
    struct Snapshot {
        uint64_t revision=1;
        uint32_t request=0,remaining_ms=0; // Rounded up to seconds; no client-side deadline authority.
        Action action=Action::none;Phase phase=Phase::idle;Reason reason=Reason::none;
    };
    struct Backend {
        virtual bool busy()=0;
        virtual void begin(bool sender)=0;
        virtual Step zeroFrame(int64_t now_us)=0; // Existing Restart preparation only.
        virtual Step quiesce(int64_t now_us)=0;
        virtual void execute(Action)=0; // Power Off invokes the exact accepted original sequence.
        virtual ~Backend()=default;
    };
    explicit DeviceMaintenance(Backend& backend):backend_(backend){}
    Snapshot snapshot() const {return state_;}
    bool active() const {return state_.phase!=Phase::idle;}
    bool committed() const {return unsigned(state_.phase)>=unsigned(Phase::zeroing);}
    uint32_t remainingSeconds(uint32_t now) const {
        if(state_.phase!=Phase::countdown)return 0;
        const auto elapsed=uint32_t(now-started_);
        return elapsed>=countdown_ms?0:(countdown_ms-elapsed+999)/1000;
    }
    Result request(Action action,uint32_t owner,uint32_t id,uint32_t now) {
        if(!owner||!id||!valid(action))return Result::invalid;
        if(active()||backend_.busy())return Result::busy;
        state_.action=action;state_.request=id;state_.phase=Phase::countdown;
        state_.reason=Reason::none;state_.remaining_ms=countdown_ms;
        owner_=owner;started_=now;++state_.revision;return Result::accepted;
    }
    Result cancel(uint32_t owner,uint32_t request,uint32_t now) {
        if(owner!=owner_||request!=state_.request)return Result::unauthorized;
        return cancelPhysical(now);
    }
    Result cancelPhysical(uint32_t now) {
        if(state_.phase!=Phase::countdown||uint32_t(now-started_)>=countdown_ms)return Result::busy;
        state_.phase=Phase::idle;state_.action=Action::none;state_.reason=Reason::cancelled;
        state_.request=state_.remaining_ms=owner_=0;++state_.revision;return Result::cancelled;
    }
    Result local(Action action,uint32_t now,bool sender) {
        if(!valid(action))return Result::invalid;
        if(active()||backend_.busy())return Result::busy;
        state_.action=action;state_.request=0;owner_=0;commit(now,sender);return Result::accepted;
    }
    void poll(uint32_t now,int64_t now_us,bool sender) {
        if(state_.phase==Phase::countdown) {
            const auto seconds=remainingSeconds(now);
            if(seconds) {
                if(state_.remaining_ms!=seconds*1000){state_.remaining_ms=seconds*1000;++state_.revision;}
            } else commit(now,sender);
            return; // Publish committed state before hardware execution on the next owner turn.
        }
        if(!committed()||state_.phase==Phase::failed)return;
        if(state_.phase==Phase::executing){backend_.execute(state_.action);fail(Reason::returned);return;}
        if(uint32_t(now-started_)>=2000){fail(Reason::deadline);return;}
        if(state_.phase==Phase::zeroing) {
            const auto result=backend_.zeroFrame(now_us);
            if(result==Step::failed){fail(Reason::transport);return;}
            if(result!=Step::ready||++zeros_<3)return;
            state_.phase=Phase::quiescing;++state_.revision;
        }
        if(state_.phase==Phase::quiescing) {
            const auto result=backend_.quiesce(now_us);
            if(result==Step::failed)fail(Reason::transport);
            else if(result==Step::ready){state_.phase=Phase::executing;++state_.revision;}
        }
    }
 private:
    static bool valid(Action a){return a==Action::restart||a==Action::power_off;}
    void commit(uint32_t now,bool sender) {
        started_=now;zeros_=0;state_.reason=Reason::none;state_.remaining_ms=0;
        // Do not prepend the new Restart transport preparation to original Power Off.
        if(state_.action==Action::power_off)state_.phase=Phase::executing;
        else {state_.phase=sender?Phase::zeroing:Phase::quiescing;backend_.begin(sender);}
        ++state_.revision;
    }
    void fail(Reason reason){state_.phase=Phase::failed;state_.reason=reason;++state_.revision;}
    Backend& backend_;Snapshot state_;uint32_t owner_=0,started_=0;unsigned zeros_=0;
};
