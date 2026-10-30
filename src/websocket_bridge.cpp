#include "websocket_bridge.h"
#include <string.h>
#include <algorithm>

namespace websocket_bridge {
namespace { struct Guard { Lock& lock;explicit Guard(Lock& l):lock(l){lock.enter();}~Guard(){lock.leave();} }; }
Bridge::Bridge(DmxController& c,CommandDispatcher& d,Lock& l,DeviceSettings* settings,DeviceMaintenance* maintenance,WifiConfiguration* wifi):controller_(c),dispatcher_(d),lock_(l),settings_(settings),wifi_(wifi),maintenance_(maintenance) {
    dispatcher_.setCompletion(completed,this); // Leaves UI observer, admission and publisher intact.
}
Bridge::Client* Bridge::client(Token t) {
    if(t.slot>=max_clients||!t.generation)return nullptr;
    auto& c=clients_[t.slot];return c.active&&c.generation==t.generation?&c:nullptr;
}
namespace {bool update(Kind kind){return kind==Kind::channel||kind==Kind::universe||kind==Kind::mode;}}
void Bridge::drop(Client& c,Stop stop,uint64_t rejected_revision) {
    if(stop!=Stop::requested&&!metrics_.first.present) {
        auto& first=metrics_.first;first.present=true;first.ms=lock_.nowMs();first.stop=stop;
        first.token.generation=c.generation;first.token.slot=uint8_t(&c-clients_);
        first.queued=c.count;first.outstanding=c.outstanding;
        first.last_enqueued_revision=c.last_enqueued_revision;first.rejected_revision=rejected_revision;
    }
    c.stop=stop;c.active=false;c.count=c.outstanding=0;--metrics_.clients;++metrics_.closed;
}
void Bridge::push(Client& c,const Event& e) {
    if(c.count==outbox_capacity){++metrics_.slow_closes;++metrics_.queue_full;drop(c,Stop::queue_full,e.revision);return;}
    c.events[(c.head+c.count)%outbox_capacity]=e;
    metrics_.outbox_high=std::max(metrics_.outbox_high,++c.count);
    if(update(e.kind)){++metrics_.updates_enqueued;c.last_enqueued_revision=e.revision;}
}
Token Bridge::open(uint64_t now) {
    Guard guard(lock_);
    if(count_==inbox_capacity||generation_==UINT32_MAX)return {};
    for(unsigned i=0;i<max_clients;++i)if(!clients_[i].active) {
        auto& c=clients_[i];
        c.generation=++generation_;c.active=true;c.ready=false;c.settings=false;c.maintenance=c.wifi=false;c.head=c.count=c.last_id=c.outstanding=c.rate=0;c.window=now;
        c.stop=Stop::none;c.last_enqueued_revision=0;++metrics_.opened;
        Token t;t.slot=uint8_t(i);t.generation=c.generation;
        Request r;r.token=t;r.snapshot=r.initial=true;
        inbox_[(head_+count_)%inbox_capacity]=r;++count_;++metrics_.clients;
        metrics_.inbox_high=std::max(metrics_.inbox_high,count_);return t;
    }
    return {};
}
void Bridge::close(Token t) {Guard guard(lock_);if(auto* c=client(t))drop(*c,Stop::requested);}
bool Bridge::alive(Token t) {Guard guard(lock_);return client(t)!=nullptr;}
bool Bridge::hasGeneration(uint32_t generation) {Guard guard(lock_);for(const auto& c:clients_)if(c.active&&c.generation==generation)return true;return false;}
Metrics Bridge::metrics() {
    Guard guard(lock_);auto m=metrics_;m.inbox=count_;
    for(const auto& c:clients_)if(c.active){m.outbox+=c.count;m.outstanding+=c.outstanding;}
    return m;
}
ClientInfo Bridge::clientInfo(Token t) {
    Guard guard(lock_);ClientInfo info;
    if(t.slot>=max_clients||!t.generation)return info;
    const auto& c=clients_[t.slot];if(c.generation!=t.generation)return info;
    info.active=c.active;info.stop=c.stop;info.queued=c.count;info.outstanding=c.outstanding;
    info.last_enqueued_revision=c.last_enqueued_revision;return info;
}
bool Bridge::take(Token t,Event& e) {
    Guard guard(lock_);auto* c=client(t);if(!c||!c->count)return false;
    e=c->events[c->head];c->head=(c->head+1)%outbox_capacity;--c->count;
    if(update(e.kind))++metrics_.updates_taken;
    return true;
}
void Bridge::sendFailed(Token t) {Guard guard(lock_);++metrics_.send_failures;if(auto* c=client(t))drop(*c,Stop::send_failure);}
void Bridge::receive(Token t,const char* data,size_t length,uint64_t now) {
    Request r;const char* error=nullptr;const bool valid=decode(data,length,r,error);r.token=t;
    Guard guard(lock_);auto* c=client(t);if(!c)return;
    if(now-c->window>=1000){c->window=now;c->rate=0;}
    if(++c->rate>20){++metrics_.rejected;++metrics_.slow_closes;drop(*c,Stop::rate_limit);return;}
    if(!valid||!c->ready||r.id<=c->last_id||c->outstanding>=4||count_==inbox_capacity) {
        ++metrics_.rejected;Event e;e.id=r.id;e.reason=!valid?error:(!c->ready?"sync_pending":(r.id<=c->last_id?"id_order":"backpressure"));
        e.result=valid?CommandStatus::busy:CommandStatus::invalid;
        if(valid&&r.id>c->last_id)c->last_id=r.id;
        push(*c,e);return;
    }
    c->last_id=r.id;++c->outstanding;
    inbox_[(head_+count_)%inbox_capacity]=r;++count_;++metrics_.accepted;
    metrics_.inbox_high=std::max(metrics_.inbox_high,count_);
}
Event Bridge::state(Kind kind,uint32_t id) {
    Event e;e.kind=kind;e.id=id;e.revision=controller_.revision();e.sequence=++sequence_;
    e.blackout=controller_.blackoutActive();e.sender=sender_;
    e.loaded=controller_.loadedPreset();e.modified=controller_.loadedPresetModified();
    if(kind==Kind::snapshot||kind==Kind::universe) {
        memcpy(e.values,controller_.workingPacket()+1,512);
        if(!presets_ready_) {
            presets_ready_=true;
            for(unsigned i=0;i<8;++i) {
                presets_[i]=uint8_t(controller_.presetState(uint8_t(i)));
                if(presets_[i]==uint8_t(DmxPresetStorage::SlotState::error))presets_ready_=false;
            }
        }
        memcpy(e.presets,presets_,sizeof(presets_));
    }
    return e;
}
void Bridge::broadcast(Event& e) {
    Guard guard(lock_);++metrics_.broadcasts;
    for(auto& c:clients_)if(c.active&&c.ready)push(c,e);
}
void Bridge::reply(const Request& r,CommandStatus status,uint8_t domain) {
    Event e=state(status==CommandStatus::applied||status==CommandStatus::no_change?Kind::ack:Kind::error,r.id);
    e.result=status;e.domain=domain;
    Guard guard(lock_);
    if(status==CommandStatus::applied||status==CommandStatus::no_change)++metrics_.applied;
    else ++metrics_.rejected;
    if(auto* c=client(r.token)){if(c->outstanding)--c->outstanding;push(*c,e);}
}
void Bridge::ownerPoll(bool sender) {
    if(wifi_) {
        wifi_->poll(lock_.nowMs());
        if(wifi_->snapshot().revision!=wifi_revision_){auto e=wifiState(Kind::wifi_update);wifi_revision_=e.revision;broadcastWifi(e);wifi_->published();}
    }
    if(settings_) {
        const auto owner=settings_->previewOwner();bool connected=!owner;
        {Guard guard(lock_);for(const auto& c:clients_)if(c.active&&c.generation==owner)connected=true;}
        settings_->servicePreview(lock_.nowMs(),connected);
    }
    if(settings_&&settings_->snapshot().revision!=settings_revision_) {
        auto e=settingsState(Kind::settings_update);settings_revision_=e.revision;broadcastSettings(e);
    }
    if(maintenance_&&maintenance_->snapshot().revision!=maintenance_revision_) {
        auto e=maintenanceState(Kind::maintenance_update);maintenance_revision_=e.revision;broadcastMaintenance(e);
    }
    if(sender_!=sender){sender_=sender;auto e=state(Kind::mode);broadcast(e);}
    if(in_flight_)return;
    Request r;
    {Guard guard(lock_);if(!count_)return;r=inbox_[head_];inbox_[head_]=Request{};head_=(head_+1)%inbox_capacity;--count_;if(!client(r.token))return;}
    if(r.snapshot) {
        // Capture before marking ready. All mutations and this boundary run on
        // the same owner task; concurrent HTTP work only copies the queues.
        Event hello;
        if(r.initial)hello=state(Kind::hello);
        uint8_t previous_presets[8];memcpy(previous_presets,presets_,8);
        Event e=state(Kind::snapshot,r.id);
        if(memcmp(previous_presets,e.presets,8)!=0) {
            // A read after a persistence error may discover metadata changes
            // without a domain revision. Notify peers before the newer snapshot.
            e.kind=Kind::universe;e.id=0;broadcast(e);
            e.kind=Kind::snapshot;e.id=r.id;e.sequence=++sequence_;
        }
        Guard guard(lock_);
        if(auto* c=client(r.token)) {
            if(r.initial)push(*c,hello);
            if(c->active)push(*c,e);
            if(c->active){c->ready=true;if(c->outstanding)--c->outstanding;}
        }
        return;
    }
    if(r.wifi) {
        bool subscribed=false;
        {Guard guard(lock_);if(auto* c=client(r.token)){subscribed=c->wifi;if(r.wifi==1)c->wifi=true;}}
        auto result=WifiConfiguration::Result::unavailable;
        if(wifi_&&r.wifi!=1) {
            if(!subscribed)result=WifiConfiguration::Result::unauthorized;
            else if(maintenance_&&maintenance_->active())result=WifiConfiguration::Result::busy;
            else if(r.wifi==2)result=wifi_->propose(r.wifi_ssid,r.wifi_password,r.wifi_base,r.token.generation,r.id,lock_.nowMs());
            else result=wifi_->cancel(r.token.generation,r.maintenance_request);
        }
        if(wifi_&&wifi_->snapshot().revision!=wifi_revision_){auto changed=wifiState(Kind::wifi_update);wifi_revision_=changed.revision;broadcastWifi(changed);}
        auto e=wifiState(r.wifi==1?Kind::wifi_snapshot:Kind::wifi_result,r.id);e.wifi_result=result;
        Guard guard(lock_);if(auto* c=client(r.token)){if(c->outstanding)--c->outstanding;push(*c,e);}return;
    }
    if(r.maintenance) {
        bool subscribed=false;
        {Guard guard(lock_);if(auto* c=client(r.token)){subscribed=c->maintenance;if(r.maintenance==1)c->maintenance=true;}}
        auto result=DeviceMaintenance::Result::unsupported;
        if(maintenance_&&r.maintenance!=1) {
            if(!subscribed)result=DeviceMaintenance::Result::unauthorized;
            else if(!sender_)result=DeviceMaintenance::Result::unsupported;
            else if(wifi_&&wifi_->active())result=DeviceMaintenance::Result::busy;
            else if(r.maintenance==2) {
                result=maintenance_->request(r.maintenance_action,r.token.generation,r.id,lock_.nowMs());
                if(result==DeviceMaintenance::Result::accepted&&settings_) {
                    settings_->cancelPreviewForMaintenance();
                    if(settings_->snapshot().revision!=settings_revision_) {
                        auto changed=settingsState(Kind::settings_update);settings_revision_=changed.revision;broadcastSettings(changed);
                    }
                }
            }
            else result=maintenance_->cancel(r.token.generation,r.maintenance_request,lock_.nowMs());
        }
        if(maintenance_&&maintenance_->snapshot().revision!=maintenance_revision_) {
            auto e=maintenanceState(Kind::maintenance_update);maintenance_revision_=e.revision;broadcastMaintenance(e);
        }
        auto e=maintenanceState(r.maintenance==1?Kind::maintenance_snapshot:Kind::maintenance_result,r.id);e.maintenance_result=result;
        Guard guard(lock_);if(auto* c=client(r.token)){if(c->outstanding)--c->outstanding;push(*c,e);}return;
    }
    if(maintenance_&&maintenance_->committed()&&(!r.settings||r.settings_write)) {reply(r,CommandStatus::busy);return;}
    if(r.settings) {
        bool subscribed=false;
        {Guard guard(lock_);if(auto* c=client(r.token)){subscribed=c->settings;if(!r.settings_write)c->settings=true;}}
        DeviceSettings::Result result=DeviceSettings::Result::invalid;
        if(settings_&&r.settings_write&&subscribed) {
            if(r.settings_edit==DeviceSettings::Edit::none)result=settings_->set(r.setting,r.setting_value);
            else result=settings_->edit(r.settings_edit,r.token.generation,r.settings_gesture,r.setting,
                r.setting_value,r.settings_base,lock_.nowMs(),r.settings_gesture==r.id);
        }
        if(settings_&&settings_->snapshot().revision!=settings_revision_) {
            auto changed=settingsState(Kind::settings_update);settings_revision_=changed.revision;broadcastSettings(changed);
        }
        auto e=settingsState(r.settings_write||!settings_?Kind::settings_result:Kind::settings_snapshot,r.id);
        e.setting=r.setting;e.settings_result=result;
        e.settings_edit=r.settings_edit;e.settings_gesture=r.settings_gesture;
        Guard guard(lock_);if(auto* c=client(r.token)){if(c->outstanding)--c->outstanding;push(*c,e);}return;
    }
    if(++internal_id_==0)++internal_id_;
    r.command.request_id=internal_id_;
    const auto admitted=dispatcher_.submit(r.command);
    if(admitted.status!=CommandStatus::accepted){reply(r,admitted.status);return;}
    flight_=r;in_flight_=true;
}
void Bridge::completed(void* context,const ControllerCommand& command,const CommandDispatcher::Result& result) {
    auto& self=*static_cast<Bridge*>(context);
    if(command.type==CommandType::load_preset||command.type==CommandType::save_preset||
       command.type==CommandType::copy_preset||command.type==CommandType::clear_preset||
       command.type==CommandType::reset_sender)self.presets_ready_=false;
    const bool changed=result.revision_after!=result.revision_before;
    if(changed) {
        {Guard guard(self.lock_);++self.metrics_.controller_events;self.metrics_.last_controller_revision=result.revision_after;}
    }
    // Even an unchanged successful load supersedes older browser gestures.
    // Publish the existing bulk shape/sequence; never fabricate a revision.
    const bool loaded=command.type==CommandType::load_preset&&result.domain==DmxController::Result::ok&&
        (result.status==CommandStatus::applied||result.status==CommandStatus::no_change||result.status==CommandStatus::transport_error);
    const bool storage_refresh=result.status==CommandStatus::storage_error&&
        (command.type==CommandType::load_preset||command.type==CommandType::save_preset||
         command.type==CommandType::clear_preset||command.type==CommandType::copy_preset);
    if(changed||loaded||storage_refresh) {
        const bool channel=command.type==CommandType::set_channel||command.type==CommandType::adjust_channel;
        auto e=self.state(channel?Kind::channel:Kind::universe);
        if(channel){e.channel=uint16_t(command.payload.channel.channel);e.value=self.controller_.channel(e.channel);}
        self.broadcast(e);
    }
    if(self.in_flight_&&command.source==CommandSource::external&&command.request_id==self.flight_.command.request_id) {
        self.in_flight_=false;self.reply(self.flight_,result.status,uint8_t(result.domain));
    }
}
Event Bridge::wifiState(Kind kind,uint32_t id) {
    Event e;e.kind=kind;e.id=id;e.sequence=++sequence_;if(wifi_)e.wifi=wifi_->snapshot();e.revision=e.wifi.revision;return e;
}
void Bridge::broadcastWifi(Event& e) {Guard guard(lock_);for(auto& c:clients_)if(c.active&&c.ready&&c.wifi)push(c,e);}
Event Bridge::maintenanceState(Kind kind,uint32_t id) {
    Event e;e.kind=kind;e.id=id;e.sequence=++sequence_;
    if(maintenance_)e.maintenance=maintenance_->snapshot();
    e.revision=e.maintenance.revision;return e;
}
void Bridge::broadcastMaintenance(Event& e) {
    Guard guard(lock_);for(auto& c:clients_)if(c.active&&c.ready&&c.maintenance)push(c,e);
}
Event Bridge::settingsState(Kind kind,uint32_t id) {
    Event e;e.kind=kind;e.id=id;e.sequence=++sequence_;
    const auto snapshot=settings_?settings_->snapshot():DeviceSettings::Snapshot{};
    memcpy(e.values,snapshot.values,3);e.domain=snapshot.persisted;
    e.revision=snapshot.revision;return e;
}
void Bridge::broadcastSettings(Event& e) {
    Guard guard(lock_);for(auto& c:clients_)if(c.active&&c.ready&&c.settings)push(c,e);
}
}
