#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>  
#include <dirent.h>
#include <sys/times.h>
#include "raft_test_utils.h"

unit_test_suite* unit_test_suite::instance() {
    static unit_test_suite _instance;
    return &_instance;
}

unit_test_case* unit_test_suite::register_test_case(const char* part, const char* name, unit_test_case* obj) {
    std::string spart(part);
    std::string sname(name);
    for (auto &part : cases) {
        if (part.first == spart) {
            part.second.push_back({sname, obj});
            return obj;
        }
    }
    std::vector<std::pair<std::string, unit_test_case*>> part_cases;
    part_cases.push_back({sname, obj});
    cases.push_back({spart, part_cases});
    return nullptr;
}

bool unit_test_suite::run_case(const std::string &part, const std::string &name, unit_test_case* obj) {
    if (is_counting) {
        count++;
        return true;
    }
    std::cout << "Test (" << part << "." << name << "): " << std::string(obj->message) << std::endl;
    static int clktck = -1;
    if (clktck == -1) clktck = sysconf(_SC_CLK_TCK);
    clock_t start, end;
    tms start_t, end_t;
    start = times(&start_t);
    obj->body();
    end = times(&end_t);
    std::cout << "Pass (" << part << "." << name << "). wall-time: " << (double)(end-start) / (double)clktck  
        << "s, user-time: " << (double)(end_t.tms_utime - start_t.tms_utime) /(double)clktck 
        << "s, sys-time: " << (double)(end_t.tms_stime - start_t.tms_stime) / (double)clktck << "s" << std::endl;
    passed++;
    return true;
}

bool unit_test_suite::run_part_case(const std::string &spart, const std::string &sname) {
    for (auto &part : cases) {
        if (part.first == spart) {
            for (auto &part_cases : part.second) {
                if (sname == "*" || sname == part_cases.first) {
                    run_case(spart, part_cases.first, part_cases.second);
                }
            }
        }
    }
    return true;
}

bool unit_test_suite::run(int argc, char** argv) {
    auto f = [=]() {
        if (argc == 1) {
            run_all();
        } else if (argc == 2) {
            std::string spart(argv[1]);
            run_part_case(spart, "*");
        } else {
            std::string spart(argv[1]), sname(argv[2]);
            run_part_case(spart, sname);
        }
    };
    is_counting = true;
    count = 0;
    passed = 0;
    f();
    
    std::cout << "Running " << count << " Tests ..." << std::endl;
    is_counting = false;
    static int clktck = -1;
    if (clktck == -1) clktck = sysconf(_SC_CLK_TCK);
    clock_t start, end;
    tms start_t, end_t;
    start = times(&start_t);
    f();
    end = times(&end_t);
    std::cout << "Pass " << passed << "/" << count << " tests. wall-time: " << (double)(end-start) / (double)clktck  
        << "s, user-time: " << (double)(end_t.tms_utime - start_t.tms_utime) /(double)clktck 
        << "s, sys-time: " << (double)(end_t.tms_stime - start_t.tms_stime) / (double)clktck << "s" << std::endl;
    return true;
}

bool unit_test_suite::run_all() {
    for (auto &parts : cases) {
        run_part_case(parts.first, "*");
    }
    return true;
}


marshall& operator<<(marshall &m, const list_command& cmd) {
    m << cmd.value;
    return m;
}

unmarshall& operator>>(unmarshall &u, list_command& cmd) {
    u >> cmd.value;
    return u;
}

list_state_machine::list_state_machine() {
    store.push_back(0); 
    num_append_logs = 0;
    // Because the log is start from 1, so we push back a value to align with the raft log.
}

void list_state_machine::apply_log(raft_command &cmd) {
    std::unique_lock<std::mutex> lock(mtx);
    const list_command& list_cmd = dynamic_cast<const list_command&>(cmd);
    store.push_back(list_cmd.value);
    num_append_logs++;
}

void list_state_machine::apply_snapshot(const std::vector<char>& snapshot) {
    std::unique_lock<std::mutex> lock(mtx);
    std::string str;
    str.assign(snapshot.begin(), snapshot.end());
    std::stringstream ss(str);
    store = std::vector<int>();
    int size;
    ss >> size;
    for (int i = 0; i < size; i++) {
        int temp;
        ss >> temp;
        store.push_back(temp);
    }
}

std::vector<char> list_state_machine::snapshot() {
    std::unique_lock<std::mutex> lock(mtx);
    std::vector<char> data;
    std::stringstream ss;
    ss << (int)store.size();
    for (auto value : store)
        ss << ' ' << value ;
    std::string str = ss.str();
    data.assign(str.begin(), str.end());
    return data;
}

std::vector<rpcs*> create_random_rpc_servers(int num) {
    std::vector<rpcs*> res(num);
    static int port = 3536;
    for (int i = 0; i < num; i++) {
        res[i] = new rpcs(port); // use random port
        port++;
    }
    return res;
}

std::vector<rpcc*> create_rpc_clients(const std::vector<rpcs*> &servers) {
    int num = servers.size();
    std::vector<rpcc*> res(num);
    for (int i = 0; i < num; i++) {
        struct sockaddr_in sin;
        make_sockaddr(std::to_string(servers[i]->port()).c_str(), &sin);
        res[i] = new rpcc(sin);
        int ret = res[i]->bind();
        ASSERT(ret >= 0, "bind fail " << ret);
    }
    return res;
}

void mssleep(int ms) {
    usleep(ms * 1000);
}

int remove_directory(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
             continue;

          len = path_len + strlen(p->d_name) + 2; 
          buf = (char*)malloc(len);

          if (buf) {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode))
                   r2 = remove_directory(buf);
                else
                   r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }

   if (!r)
      r = rmdir(path);

   return r;
}
