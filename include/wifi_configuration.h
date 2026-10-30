#pragma once
#include "softap_policy.h"
#include <string.h>

// Application-task authority. Secret candidates never form part of a public snapshot.
class WifiConfiguration {
 public:
    enum class Phase : uint8_t { idle, approval, applying, failed };
    enum class Reason : uint8_t { none, cancelled, expired, disconnected, storage, radio, rollback, unavailable };
    enum class Result : uint8_t { accepted, cancelled, no_change, invalid, conflict, busy, unauthorized, unavailable };
    enum class Work : uint8_t { pending, done, storage_error, radio_error, rollback_error };
    struct Snapshot {
        uint64_t revision=1,base=1;
        uint32_t request=0,remaining_ms=0;
        char ssid[33]{},proposed[33]{};
        softap_policy::Origin origin=softap_policy::Origin::legacy;
        Phase phase=Phase::idle;Reason reason=Reason::none;bool password_change=false,available=false;
    };
    struct Backend {
        virtual bool available()=0;
        virtual bool connected(uint32_t generation)=0;
        virtual void current(softap_policy::Credentials&)=0;
        virtual bool begin(const softap_policy::Credentials&)=0;
        virtual Work progress()=0;
        virtual void published()=0;
        virtual ~Backend()=default;
    };
    explicit WifiConfiguration(Backend& backend):backend_(backend){}
    static void erase(void* p,size_t n) {volatile uint8_t* v=static_cast<volatile uint8_t*>(p);while(n--)*v++=0;}
    static bool text(const char* p,size_t cap,size_t low,size_t high) {
        const char* end=static_cast<const char*>(memchr(p,0,cap));
        if(!end||size_t(end-p)<low||size_t(end-p)>high)return false;
        for(;p<end;++p)if(uint8_t(*p)<32||uint8_t(*p)>126)return false;
        return true;
    }
    Snapshot snapshot() const {return state_;}
    bool active() const {return state_.phase==Phase::approval||state_.phase==Phase::applying;}
    bool applying() const {return state_.phase==Phase::applying;}
    bool owned(uint32_t generation) const {return owner_==generation&&state_.phase==Phase::approval;}
    void published() {if(applying())backend_.published();}
    Result propose(const char* ssid,const char* password,uint64_t base,uint32_t generation,uint32_t id,uint32_t now) {
        if(active())return Result::busy;
        if(!generation||!id||!text(ssid,33,1,32)||!text(password,64,0,63)||
           (password[0]&&!text(password,64,8,63)))return Result::invalid;
        if(base!=state_.base)return Result::conflict;
        if(!backend_.available()||!backend_.connected(generation))return Result::unavailable;
        backend_.current(candidate_);
        state_.password_change=password[0]&&strcmp(password,candidate_.pass);
        if(!strcmp(ssid,candidate_.ssid)&&!state_.password_change){erase(&candidate_,sizeof(candidate_));return Result::no_change;}
        candidate_.version=2;candidate_.origin=softap_policy::Origin::custom;
        memcpy(candidate_.ssid,ssid,strlen(ssid)+1);
        if(password[0])memcpy(candidate_.pass,password,strlen(password)+1);
        // Zero trailing bytes: deterministic complete record, including former longer secrets.
        memset(candidate_.ssid+strlen(candidate_.ssid)+1,0,sizeof(candidate_.ssid)-strlen(candidate_.ssid)-1);
        memset(candidate_.pass+strlen(candidate_.pass)+1,0,sizeof(candidate_.pass)-strlen(candidate_.pass)-1);
        owner_=generation;started_=now;state_.request=id;state_.phase=Phase::approval;state_.reason=Reason::none;
        memcpy(state_.proposed,candidate_.ssid,sizeof(state_.proposed));state_.remaining_ms=60000;++state_.revision;
        return Result::accepted;
    }
    Result cancel(uint32_t generation,uint32_t request) {
        if(!owned(generation)||state_.request!=request)return Result::unauthorized;
        discard(Reason::cancelled);return Result::cancelled;
    }
    bool approve(uint32_t now) {
        if(state_.phase!=Phase::approval)return false;
        if(uint32_t(now-started_)>=60000){discard(Reason::expired);return false;}
        if(!backend_.connected(owner_)){discard(Reason::disconnected);return false;}
        if(!backend_.available()||!backend_.begin(candidate_)){discard(Reason::unavailable);state_.phase=Phase::failed;return false;}
        erase(&candidate_,sizeof(candidate_));state_.phase=Phase::applying;state_.remaining_ms=0;++state_.revision;return true;
    }
    void dismiss() {if(state_.phase==Phase::failed)discard(state_.reason);}
    void cancelPhysical() {if(state_.phase==Phase::approval)discard(Reason::cancelled);}
    void poll(uint32_t now) {
        const bool ready=backend_.available();
        if(state_.available!=ready){state_.available=ready;++state_.revision;}
        if(state_.phase==Phase::approval) {
            if(!backend_.connected(owner_)){discard(Reason::disconnected);return;}
            const uint32_t elapsed=now-started_;
            if(elapsed>=60000){discard(Reason::expired);return;}
            const uint32_t left=((60000-elapsed+999)/1000)*1000;
            if(left!=state_.remaining_ms){state_.remaining_ms=left;++state_.revision;}
        } else if(applying()) {
            const auto result=backend_.progress();if(result==Work::pending)return;
            softap_policy::Credentials c{};backend_.current(c);
            memcpy(state_.ssid,c.ssid,sizeof(state_.ssid));state_.origin=c.origin;erase(&c,sizeof(c));
            if(result==Work::done){++state_.base;discard(Reason::none);}
            else {discard(result==Work::storage_error?Reason::storage:result==Work::radio_error?Reason::radio:Reason::rollback);state_.phase=Phase::failed;}
        } else if(!initialized_&&ready) {
            softap_policy::Credentials c{};backend_.current(c);memcpy(state_.ssid,c.ssid,sizeof(state_.ssid));state_.origin=c.origin;
            erase(&c,sizeof(c));initialized_=true;++state_.revision;
        }
    }
 private:
    void discard(Reason reason) {
        erase(&candidate_,sizeof(candidate_));owner_=0;state_.request=state_.remaining_ms=0;state_.phase=Phase::idle;
        state_.reason=reason;state_.password_change=false;memset(state_.proposed,0,sizeof(state_.proposed));++state_.revision;
    }
    Backend& backend_;Snapshot state_;softap_policy::Credentials candidate_{};
    uint32_t owner_=0,started_=0;bool initialized_=false;
};
