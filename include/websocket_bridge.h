#pragma once
#include "command_dispatcher.h"

// Fixed storage, no sockets/RTOS in the tested core. Only ownerPoll/completed
// access the domain. Lock protects copied messages, never domain or socket I/O.
namespace websocket_bridge {
constexpr unsigned max_clients=4, inbox_capacity=16, outbox_capacity=8;
constexpr size_t max_message=256, max_json=2560;
struct Lock { virtual void enter()=0; virtual void leave()=0; virtual uint32_t nowMs() const {return 0;} virtual ~Lock()=default; };
struct Token { uint32_t generation=0; uint8_t slot=0; };
enum class Stop : uint8_t { none, requested, queue_full, rate_limit, send_failure };
struct ClientInfo {
    bool active=false;Stop stop=Stop::none;
    uint32_t queued=0,outstanding=0;
    uint64_t last_enqueued_revision=0;
};
struct Failure {
    bool present=false;Token token;Stop stop=Stop::none;
    uint32_t ms=0,queued=0,outstanding=0;
    uint64_t last_enqueued_revision=0,rejected_revision=0;
};
enum class Kind : uint8_t { hello, snapshot, channel, universe, mode, ack, error };
struct Event {
    Kind kind=Kind::error;
    uint64_t revision=0, sequence=0;
    uint32_t id=0;
    CommandStatus result=CommandStatus::invalid;
    uint8_t domain=0;
    bool blackout=false, sender=false;
    uint16_t channel=0;
    uint8_t value=0, values[512]{}, presets[8]{};
    const char* reason=nullptr; // Only compile-time protocol error literals.
};
struct Metrics {
    uint32_t clients=0, inbox=0, inbox_high=0, outbox_high=0;
    uint32_t accepted=0, rejected=0, applied=0, broadcasts=0, send_failures=0, slow_closes=0;
    uint32_t opened=0,closed=0,outbox=0,outstanding=0,updates_enqueued=0,updates_taken=0,queue_full=0,controller_events=0;
    uint64_t last_controller_revision=0;
    Failure first;
};
struct Request { Token token; uint32_t id=0; bool snapshot=false, initial=false; ControllerCommand command; };
bool decode(const char* data,size_t length,Request& request,const char*& error);
size_t encode(const Event& event,char* output,size_t capacity);
bool validFrame(unsigned opcode,bool final,size_t length);
const char* statusName(CommandStatus status);

class Bridge {
 public:
    Bridge(DmxController& controller,CommandDispatcher& dispatcher,Lock& lock);
    // HTTP task. Tokens include a generation: descriptors never cross tasks.
    Token open(uint64_t now_ms);
    void close(Token token);
    bool alive(Token token);
    void receive(Token token,const char* data,size_t length,uint64_t now_ms);
    bool take(Token token,Event& event);
    void sendFailed(Token token);
    Metrics metrics();
    ClientInfo clientInfo(Token token);
    // Application task, before Dispatcher::process; one in-flight web command.
    void ownerPoll(bool sender_active);
 private:
    struct Client {
        uint32_t generation=0, last_id=0, count=0, head=0, outstanding=0, rate=0;
        uint64_t window=0;
        bool active=false, ready=false;
        Stop stop=Stop::none;
        uint64_t last_enqueued_revision=0;
        Event events[outbox_capacity];
    } clients_[max_clients];
    DmxController& controller_;
    CommandDispatcher& dispatcher_;
    Lock& lock_;
    Request inbox_[inbox_capacity], flight_;
    unsigned head_=0,count_=0;
    bool in_flight_=false, sender_=false;
    uint32_t generation_=0, internal_id_=0;
    uint64_t sequence_=0;
    uint8_t presets_[8]{};
    bool presets_ready_=false; // Owner-only derived metadata, invalidated by preset operations.
    Metrics metrics_;
    Client* client(Token token);
    void push(Client& client,const Event& event);
    void drop(Client& client,Stop stop,uint64_t rejected_revision=0);
    void reply(const Request& request,CommandStatus status,uint8_t domain=0);
    Event state(Kind kind,uint32_t id=0);
    void broadcast(Event& event);
    static void completed(void*,const ControllerCommand&,const CommandDispatcher::Result&);
};
}
