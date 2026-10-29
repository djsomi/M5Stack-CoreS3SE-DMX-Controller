#pragma once
#include <stdint.h>

// Application-task only. No sockets, DMX state, allocation or RTOS ownership.
enum class DeviceSetting : uint8_t { brightness, timeout, volume };
class DeviceSettingsBackend {
 public:
    virtual bool read(DeviceSetting key,uint8_t& value)=0;
    virtual bool write(DeviceSetting key,uint8_t value)=0;
    virtual void apply(DeviceSetting key,uint8_t value)=0;
    virtual ~DeviceSettingsBackend()=default;
};
class DeviceSettings {
 public:
    enum class Result : uint8_t { applied, no_change, storage_error, invalid, conflict };
    enum class Edit : uint8_t { none, preview, commit, cancel };
    struct Snapshot { uint64_t revision=1;uint8_t values[3]{3,3,2};uint8_t persisted=0; };
    explicit DeviceSettings(DeviceSettingsBackend& backend):backend_(backend){}
    static bool valid(DeviceSetting key,uint32_t value) {
        return unsigned(key)<3&&value<=(key==DeviceSetting::brightness?5U:4U)&&
               (key!=DeviceSetting::brightness||value>=1);
    }
    uint8_t value(DeviceSetting key) const {return state_.values[unsigned(key)];}
    Snapshot snapshot() const {return state_;}
    void begin() {
        for(unsigned i=0;i<3;++i) {
            const auto key=DeviceSetting(i);uint8_t value=state_.values[i];
            if(backend_.read(key,value)) {
                if(valid(key,value)){durable_[i]=value;known_|=1U<<i;state_.persisted|=1U<<i;}
                if(i==0&&!valid(key,value))value=3;
                if(i==1&&!valid(key,value))value=3;
                if(i==2&&value>4)value=4; // Preserve older volume-5 compatibility.
                state_.values[i]=value;
            }
            backend_.apply(key,state_.values[i]);
        }
    }
    Result set(DeviceSetting key,uint32_t value,bool persist=true) {
        if(!valid(key,value))return Result::invalid;
        cancelPreview(); // Physical/legacy intent supersedes a remote preview.
        const unsigned i=unsigned(key);
        if(state_.values[i]==value)return Result::no_change;
        state_.values[i]=uint8_t(value);pending_|=1U<<i;
        state_.persisted&=~(1U<<i);++state_.revision;
        backend_.apply(key,uint8_t(value));
        return persist?commit(key):Result::applied;
    }
    // Release-time physical persistence: a failed write is not retried per idle frame.
    void commitPending() {for(unsigned i=0;i<3;++i)if(pending_&(1U<<i))commit(DeviceSetting(i));}
    uint32_t previewOwner() const {return preview_.owner;}
    void servicePreview(uint32_t now,bool owner_alive=true) {
        if(preview_.owner&&(!owner_alive||uint32_t(now-preview_.touched)>=10000||
            uint32_t(now-preview_.started)>=30000))cancelPreview();
    }
    Result edit(Edit operation,uint32_t owner,uint32_t gesture,DeviceSetting key,
                uint32_t value,uint64_t base,uint32_t now,bool first=false) {
        if(!owner||!gesture||unsigned(key)>=3||operation==Edit::none||unsigned(operation)>unsigned(Edit::cancel)||
            (operation==Edit::preview&&!valid(key,value)))return Result::invalid;
        servicePreview(now);
        if(operation==Edit::preview&&first) {
            if(base!=state_.revision||pending_)return Result::conflict;
            cancelPreview();
            preview_.owner=owner;preview_.gesture=gesture;preview_.key=key;
            preview_.value=this->value(key);preview_.saved=state_.persisted&(1U<<unsigned(key));
            preview_.started=preview_.touched=now;
        } else if(preview_.owner!=owner||preview_.gesture!=gesture||preview_.key!=key||
                  (operation!=Edit::cancel&&base!=state_.revision))return Result::conflict;
        if(operation==Edit::cancel){cancelPreview();return Result::applied;}
        if(operation==Edit::commit){preview_.owner=0;return commit(key);}
        preview_.touched=now;
        if(this->value(key)==value)return Result::no_change;
        state_.values[unsigned(key)]=uint8_t(value);state_.persisted&=~(1U<<unsigned(key));
        ++state_.revision;backend_.apply(key,uint8_t(value));return Result::applied;
    }
 private:
    void cancelPreview() {
        if(!preview_.owner)return;
        const auto key=preview_.key;const unsigned i=unsigned(key),bit=1U<<i;
        preview_.owner=0;
        if(state_.values[i]!=preview_.value)backend_.apply(key,preview_.value);
        state_.values[i]=preview_.value;
        state_.persisted=uint8_t((state_.persisted&~bit)|preview_.saved);
        ++state_.revision; // Also announces cancellation at an unchanged value.
    }
    Result commit(DeviceSetting key) {
        const unsigned i=unsigned(key),bit=1U<<i;pending_&=~bit;
        const bool saved=((known_&bit)&&durable_[i]==state_.values[i])||backend_.write(key,state_.values[i]);
        if(saved){durable_[i]=state_.values[i];known_|=bit;state_.persisted|=bit;++state_.revision;}
        else known_&=~bit; // A failed confirmation may follow a completed write.
        return saved?Result::applied:Result::storage_error;
    }
    DeviceSettingsBackend& backend_;Snapshot state_;uint8_t durable_[3]{},known_=0,pending_=0;
    struct Preview {
        uint32_t owner=0,gesture=0,started=0,touched=0;
        DeviceSetting key=DeviceSetting::brightness;uint8_t value=0,saved=0;
    } preview_;
};
