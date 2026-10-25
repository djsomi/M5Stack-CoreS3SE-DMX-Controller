#include "websocket_bridge.h"
#include <string.h>
#include <algorithm>

namespace websocket_bridge {
namespace { struct Guard { Lock& lock;explicit Guard(Lock& l):lock(l){lock.enter();}~Guard(){lock.leave();} }; }
Bridge::Bridge(DmxController& c,CommandDispatcher& d,Lock& l):controller_(c),dispatcher_(d),lock_(l) {
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
        c.generation=++generation_;c.active=true;c.ready=false;c.head=c.count=c.last_id=c.outstanding=c.rate=0;c.window=now;
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
    if(kind==Kind::snapshot||kind==Kind::universe) {
        memcpy(e.values,controller_.workingPacket()+1,512);
        if(!presets_ready_) {
            for(unsigned i=0;i<8;++i)presets_[i]=uint8_t(controller_.presetState(uint8_t(i)));
            presets_ready_=true;
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
    if(sender_!=sender){sender_=sender;auto e=state(Kind::mode);broadcast(e);}
    if(in_flight_)return;
    Request r;
    {Guard guard(lock_);if(!count_)return;r=inbox_[head_];head_=(head_+1)%inbox_capacity;--count_;if(!client(r.token))return;}
    if(r.snapshot) {
        // Capture before marking ready. All mutations and this boundary run on
        // the same owner task; concurrent HTTP work only copies the queues.
        Event hello;
        if(r.initial)hello=state(Kind::hello);
        Event e=state(Kind::snapshot,r.id);
        Guard guard(lock_);
        if(auto* c=client(r.token)) {
            if(r.initial)push(*c,hello);
            if(c->active)push(*c,e);
            if(c->active){c->ready=true;if(c->outstanding)--c->outstanding;}
        }
        return;
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
    if(result.revision_after!=result.revision_before) {
        {Guard guard(self.lock_);++self.metrics_.controller_events;self.metrics_.last_controller_revision=result.revision_after;}
        const bool channel=command.type==CommandType::set_channel||command.type==CommandType::adjust_channel;
        auto e=self.state(channel?Kind::channel:Kind::universe);
        if(channel){e.channel=uint16_t(command.payload.channel.channel);e.value=self.controller_.channel(e.channel);}
        self.broadcast(e);
    }
    if(self.in_flight_&&command.source==CommandSource::external&&command.request_id==self.flight_.command.request_id) {
        self.in_flight_=false;self.reply(self.flight_,result.status,uint8_t(result.domain));
    }
}
}
