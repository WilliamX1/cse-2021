#include "raft_state_machine.h"


kv_command::kv_command() : kv_command(CMD_NONE, "", "") { }

kv_command::kv_command(command_type tp, const std::string &key, const std::string &value) : 
    cmd_tp(tp), key(key), value(value), res(std::make_shared<result>())
{
    res->start = std::chrono::system_clock::now();
    res->key = key;
}

kv_command::kv_command(const kv_command &cmd) :
    cmd_tp(cmd.cmd_tp), key(cmd.key), value(cmd.value), res(cmd.res) {}

kv_command::~kv_command() { }

int kv_command::size() const {
    // Your code here:
    return sizeof(command_type) + key.size() + value.size() + sizeof(int) * 2;
}


void kv_command::serialize(char* buf, int size) const {
    // Your code here:
    if (size != this->size()) return;
    int key_size = key.size();
    int value_size = value.size();
    int pos = 0;

    memcpy(buf + pos, (char *) &cmd_tp, sizeof(command_type));
    pos += sizeof(command_type);
    memcpy(buf + pos, (char *) &key_size, sizeof(int));
    pos += sizeof(int);
    memcpy(buf + pos, (char *) &value_size, sizeof(int));
    pos += sizeof(int);
    memcpy(buf + pos, key.c_str(), key_size);
    pos += key_size;
    memcpy(buf + pos, value.c_str(), value_size);
    return;
}

void kv_command::deserialize(const char* buf, int size) {
    // Your code here:
    int key_size;
    int value_size;
    int pos = 0;

    memcpy((char *) &cmd_tp, buf + pos, sizeof(command_type));
    pos += sizeof(command_type);
    memcpy((char *) &key_size, buf + pos, sizeof(int));
    pos += sizeof(int);
    memcpy((char *) &value_size, buf + pos, sizeof(int));
    pos += sizeof(int);
    key = std::string(buf + pos, key_size);
    pos += key_size;
    value = std::string(buf + pos, value_size);
    return;
}

marshall& operator<<(marshall &m, const kv_command& cmd) {
    // Your code here:
    m << (int) cmd.cmd_tp << cmd.key << cmd.value;
    return m;
}

unmarshall& operator>>(unmarshall &u, kv_command& cmd) {
    // Your code here:
    int type;
    u >> type >> cmd.key >> cmd.value;
    cmd.cmd_tp = kv_command::command_type(type);
    return u;
}

kv_state_machine::~kv_state_machine() {

}

void kv_state_machine::apply_log(raft_command &cmd) {
    kv_command &kv_cmd = dynamic_cast<kv_command&>(cmd);
    std::unique_lock<std::mutex> lock(kv_cmd.res->mtx);
    // Your code here:
    if (kv_cmd.cmd_tp == kv_command::command_type::CMD_DEL) {
        auto it1 = state.find(kv_cmd.key);
        if (it1 != state.end()) {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = it1->second;
            state.erase(it1);
        }
        else {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = "";
        }
    }
    else if (kv_cmd.cmd_tp == kv_command::command_type::CMD_GET) {
        auto it2 = state.find(kv_cmd.key);
        if (it2 != state.end()) {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = it2->second;
        }
        else {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = "";
        }
    }
    else if (kv_cmd.cmd_tp == kv_command::command_type::CMD_PUT) {
        auto it3 = state.find(kv_cmd.key);
        if (it3 == state.end()) {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = kv_cmd.value;
        }
        else {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = it3->second;
        }
        state[kv_cmd.key] = kv_cmd.value;
    }
    else {
        kv_cmd.res->succ = true;
    }
    kv_cmd.res->done = true;
    kv_cmd.res->cv.notify_all();
    return;
}

std::vector<char> kv_state_machine::snapshot() {
    // Your code here:
    return std::vector<char>();
}

void kv_state_machine::apply_snapshot(const std::vector<char>& snapshot) {
    // Your code here:
    return;    
}
