#ifndef raft_state_machine_h
#define raft_state_machine_h

#include "rpc.h"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <condition_variable>

class raft_command {
public:
    virtual ~raft_command() {}
    
    // These interfaces will be used to persistent the command.
    virtual int size() const = 0;
    virtual void serialize(char* buf, int size) const = 0;
    virtual void deserialize(const char* buf, int size) = 0;
};

class raft_state_machine {
public:
    virtual ~raft_state_machine() {}

    // Apply a log to the state machine.
    virtual void apply_log(raft_command&) = 0;

    // Generate a snapshot of the current state.
    virtual std::vector<char> snapshot() = 0;
    // Apply the snapshot to the state mahine.
    virtual void apply_snapshot(const std::vector<char>&) = 0;
};


class kv_command : public raft_command {
public:
    enum command_type {
        CMD_NONE, // Do nothing
        CMD_GET, // Get a key-value pair
        CMD_PUT, // Put a key-value pair
        CMD_DEL  // Delete a key-value pair
    };

    struct result {
        std::chrono::system_clock::time_point start;
        // Ops     Succ     Key     Value
        // Get       T      key     value
        // Get       F      key      ""
        // Del       T      key   old_value
        // Del       F      key      ""
        // Put       T      key   new_value
        // Put  F(replace)  key   old_value
        std::string key, value;
        bool succ;
        bool done;
        std::mutex mtx; // protect the struct
        std::condition_variable cv; // notify the caller
    };

    kv_command();
    kv_command(command_type tp, const std::string& key, const std::string& value);
    kv_command(const kv_command &);

    virtual ~kv_command();


    command_type cmd_tp;
    std::string key, value;
    std::shared_ptr<result> res;
    
    virtual int size() const override;
    virtual void serialize(char* buf, int size) const override;
    virtual void deserialize(const char* buf, int size);
};

marshall& operator<<(marshall &m, const kv_command& cmd);
unmarshall& operator>>(unmarshall &u, kv_command& cmd);

class kv_state_machine : public raft_state_machine {
public:
    virtual ~kv_state_machine();

    // Apply a log to the state machine.
    virtual void apply_log(raft_command&) override;

    // Generate a snapshot of the current state.
    virtual std::vector<char> snapshot() override;
    // Apply the snapshot to the state mahine.
    virtual void apply_snapshot(const std::vector<char>&) override;
};

#endif // raft_state_machine_h