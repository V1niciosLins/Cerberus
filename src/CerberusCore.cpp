#include "CerberusCore.hpp"
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <linux/limits.h>
#include <mutex>
#include <print>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>

constexpr int BUFFER_LEN{(sizeof(struct inotify_event) + NAME_MAX + 1) * 10};

Cerberus::Cerberus()
    : ifs{CERBERUS_PATH.data()}, fd{inotify_init1(IN_CLOEXEC)} {
  if (!ifs) {
    std::ofstream ofs{CERBERUS_PATH.data()};
    ofs << "# Arquivo base do cerberus "
        << "Para usar vc deve seguir a estrutura: [nome_do_processo] "
           "[maximo_de_memoria]\n";
    ofs.flush();

    if (ofs.bad()) {
      std::cerr << "Erro fatal de escrita. Há espaço no disco?\n";
      throw std::runtime_error(
          "CERBERUS FATAL: Falha ao escrever arquivo base de configuração.");
    }
  }

  wd_cerberus = {inotify_add_watch(fd, CERBERUS_PATH.data(), IN_CLOSE_WRITE)};
  run();
}

Cerberus::~Cerberus() {
  inotify_rm_watch(fd, wd_cerberus);
  close(fd);
  ws_running = false, wp_running = false;

  if (watch_self_thread.joinable())
    watch_self_thread.join();

  if (watch_proc_thread.joinable())
    watch_proc_thread.join();
}

int Cerberus::rd_conf() {
  std::string line_buffer{};
  ifs.clear();
  ifs.seekg(0);
  confs.clear();

  if (!ifs)
    return -1;
  while (std::getline(ifs, line_buffer)) {

    if (line_buffer.empty())
      continue;
    if (line_buffer.starts_with("#"))
      continue;

    std::istringstream iss{line_buffer};
    Conf c;

    if (iss >> c.process_name >> c.limite_mb) {
      confs.emplace_back(c);
    } else {
      std::print(stderr, "[ CERBERUS ] -> Erro de sintaxe na linha {}\n",
                 line_buffer);
      return -2;
    }
  }

  if (!confs.empty()) {
    std::print("{} {}\n", confs[0].process_name, confs[0].limite_mb);
  }

  return 0;
}

void Cerberus::watch_self() {
  if (rd_conf() < 0) {
    has_an_error = true;
  }

  ws_running = true;

  watch_self_thread = std::thread([this]() {
    char buffer[BUFFER_LEN];
    while (ws_running && !has_an_error) {
      int length = read(fd, buffer, BUFFER_LEN);
      if (length < 0) {
        std::cerr << "error in read";
        break;
      }

      int i = 0;
      while (i < length) {
        struct inotify_event *event = (struct inotify_event *)&buffer[i];

        if (event->mask & IN_CLOSE_WRITE) {
          std::cout << "Aquivo Cerberus modificado" << std::endl;

          // LEMBRAR: Escopo temporáio, o lockguard vai morrer qnd a chave
          // fechar. N tem problema chamar a função, nd ali vai fechar o
          // programa, no máximo vai retornar -1 ou -2
          {
            std::lock_guard<std::mutex> lock(mtx_configs);
            if (rd_conf() < 0) {
              has_an_error = true;
            }
          }
          has_update = true;
        }
        i += sizeof(struct inotify_event) + event->len;
      }
    }
  });
}

void Cerberus::watch_proc() {
  wp_running = true;

  watch_proc_thread = std::thread([this]() {
    std::vector<Conf> local_confs;
    std::vector<Infractor> infractors{};
    long page_size = sysconf(_SC_PAGESIZE);

    char path_buffer[512];
    char data_buffer[256];

    {
      std::lock_guard<std::mutex> lock(mtx_configs);
      local_confs = confs;
    }

    DIR *dir = opendir("/proc");
    while (wp_running && !has_an_error) {
      rewinddir(dir);
      if (has_update) {

        {
          std::lock_guard<std::mutex> lock(mtx_configs);
          local_confs = confs;
        }
        has_update = false;
      }

      std::erase_if(infractors, [](const Infractor &inf) {
        return (kill(inf.pid, 0) == -1 && errno == ESRCH);
      });

      while (auto entry = readdir(dir)) {
        if (!(entry->d_name[0] >= '0' && entry->d_name[0] <= '9'))
          continue;

        snprintf(path_buffer, sizeof(path_buffer), "/proc/%s/comm",
                 entry->d_name);

        int c_fd = open(path_buffer, O_RDONLY);
        if (c_fd < 0)
          continue;

        ssize_t c_bytes = read(c_fd, data_buffer, sizeof(data_buffer) - 1);
        close(c_fd);

        if (c_bytes <= 0)
          continue;

        if (data_buffer[c_bytes - 1] == '\n') {
          data_buffer[c_bytes - 1] = '\0';
          c_bytes--;

        } else {
          data_buffer[c_bytes] = '\0';
        }

        std::string_view proc_name{data_buffer, static_cast<size_t>(c_bytes)};

        const Conf *target_rule = nullptr;
        for (const auto &regra : local_confs) {
          if (proc_name == regra.process_name) {
            target_rule = &regra;
            break;
          }
        }

        if (!target_rule)
          continue;

        snprintf(path_buffer, sizeof(path_buffer), "/proc/%s/statm",
                 entry->d_name);
        int s_fd = open(path_buffer, O_RDONLY);
        if (s_fd < 0)
          continue;
        ssize_t s_bytes = read(s_fd, data_buffer, sizeof(data_buffer) - 1);

        close(s_fd);

        if (s_bytes <= 0)
          continue;

        data_buffer[s_bytes] = '\0';

        std::string_view sv{data_buffer, static_cast<size_t>(s_bytes)};
        size_t espace = sv.find(' ');

        if (espace == std::string_view::npos)
          continue;

        long rss_pages{};

        std::from_chars(data_buffer + espace + 1, data_buffer + s_bytes,
                        rss_pages);

        size_t memory_in_mb = (rss_pages * page_size) / (1024 * 1024);

        if (memory_in_mb > target_rule->limite_mb) {
          std::print("[ CERBERUS ] Limite estourado! Alvo: {} | PID: {} | RAM: "
                     "{}MB / Limite: {}MB\n",
                     target_rule->process_name, entry->d_name, memory_in_mb,
                     target_rule->limite_mb);

          int pid_alvo = 0;
          std::string_view pid_str{entry->d_name};

          auto convert_result = std::from_chars(
              pid_str.data(), pid_str.data() + pid_str.size(), pid_alvo);

          if (convert_result.ec == std::errc()) {

            auto it = std::find_if(infractors.begin(), infractors.end(),
                                   [pid_alvo, proc_name](const Infractor &inf) {
                                     return inf.pid == pid_alvo &&
                                            inf.proc_name == proc_name;
                                   });

            if (it != infractors.end()) {
              if (kill(pid_alvo, SIGKILL) == 0) {
                std::print("[ CERBERUS ] Morte forçada lançada: PID {}\n",
                           pid_alvo);
                infractors.erase(it);
              } else {
                std::print(stderr,
                           "[ CERBERUS ] Falha ao executar PID {}. Permissão "
                           "negada?\n",
                           pid_alvo);
              }
            } else {
              if (kill(pid_alvo, SIGTERM) == 0) {
                std::print("[ CERBERUS ] Pedido de execução lançado: PID {}\n",
                           pid_alvo);
                infractors.push_back({proc_name.data(), pid_alvo});
              } else {
                std::print(stderr,
                           "[ CERBERUS ] Falha ao executar PID {}. Permissão "
                           "negada?\n",
                           pid_alvo);
              }
            }
          }
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    closedir(dir);
  });
}

void Cerberus::run() {
  watch_self();

  watch_proc();
}
