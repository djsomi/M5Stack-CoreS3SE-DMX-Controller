#pragma once

#include "controller_types.h"
#include "dmx_controller.h"

// Application-task only, including submit(). The busy/reentrancy guard is NOT
// cross-task synchronization. Callbacks must not mutate/reenter the dispatcher.
class CommandDispatcher {
 public:
    static constexpr size_t capacity = 32;
    static constexpr size_t default_budget = 4;
    struct Submission {
        CommandStatus status;
        uint32_t ticket;
    };
    struct Result {
        CommandStatus status = CommandStatus::invalid;
        DmxController::Result domain = DmxController::Result::ok;
        uint32_t ticket = 0;
        uint64_t revision_before = 0;
        uint64_t revision_after = 0;
        int transport_result = 0;
    };
    using Publisher = int (*)(void*);
    using Completion = void (*)(void*, const ControllerCommand&, const Result&);

    explicit CommandDispatcher(DmxController& controller) : controller_(controller) {}
    CommandDispatcher(const CommandDispatcher&) = delete;
    CommandDispatcher& operator=(const CommandDispatcher&) = delete;
    void setSenderActive(bool active) { sender_active_ = active; }
    void setPublisher(Publisher publisher, void* context);
    void setCompletion(Completion completion, void* context);
    Submission submit(const ControllerCommand& command);
    size_t process(size_t budget = default_budget);
    Result executeLocal(const ControllerCommand& command);
    size_t pending() const { return count_; }
    bool executingLocal() const { return local_execution_; }
    static constexpr size_t queueStorageBytes() { return sizeof(Entry) * capacity; }

 private:
    struct Entry {
        ControllerCommand command;
        uint32_t ticket = 0;
    };
    CommandStatus validate(const ControllerCommand& command) const;
    Result processOne(bool local);
    Result apply(const ControllerCommand& command, uint32_t ticket);
    DmxController& controller_;
    Entry queue_[capacity];
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t next_ticket_ = 0;
    bool sender_active_ = false;
    bool processing_ = false;
    bool local_execution_ = false;
    Publisher publisher_ = nullptr;
    void* publisher_context_ = nullptr;
    Completion completion_ = nullptr;
    void* completion_context_ = nullptr;
};
