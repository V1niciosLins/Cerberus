#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

inline static constexpr std::string_view CERBERUS_PATH{"./mocks/cerberus.conf"};

struct Conf {
  std::string process_name;
  size_t limite_mb;
};

class Cerberus {
private:
  std::ifstream ifs;
  std::vector<Conf> confs{};
  std::thread watch_self_thread;
  std::thread watch_proc_thread;
  std::atomic<bool> ws_running, wp_running, has_update;
  std::mutex mtx_configs;

  int fd;
  int wd_cerberus;

public:
  std::atomic<bool> has_an_error;
  Cerberus();
  ~Cerberus();

  int rd_conf();

  inline void ls_confs() {
    for (const auto &conf : confs) {
      std::cout << conf.process_name << " " << conf.limite_mb << '\n';
    }
  }

  void watch_self();
  void watch_proc();
  void run();
};
