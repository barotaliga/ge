// Launching a dedicated server, defined (spawn.hpp): the record, the argv, the cfg handed over,
// the ping that finds it again, the stop.
//
// ge:summary Launching a dedicated server, defined: records, argv, cfg, ping, stop
// ge:keywords spawn_server hosted_list hosted_read hosted_write server_answers stop_hosted
// ge:skill net
// ge:test tests/server_spawn_test.cpp
#include "server/spawn.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>

#include "core/num_text.hpp"
#include "net/transport.hpp"
#include "server/process.hpp"

namespace ge::server {

namespace fs = std::filesystem;
namespace gn = ge::net;

namespace {

std::string cfg_path(const std::string& state, int port) {
  return (fs::path(state) / ("server-" + std::to_string(port) + ".cfg")).string();
}

}  // namespace

std::string hosted_path(const std::string& state, int port) {
  return (fs::path(state) / ("hosted-" + std::to_string(port) + ".txt")).string();
}

bool hosted_read(const std::string& path, Hosted& h) {
  std::ifstream f(path);
  if (!f) return false;
  std::string key;
  while (f >> key) {
    std::string rest;
    std::getline(f, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    if (key == "pid") h.pid = std::atol(rest.c_str());
    if (key == "port") h.port = std::atoi(rest.c_str());
    if (key == "kind") h.kind = rest;
    if (key == "log") h.log = rest;
    if (key == "started") h.started = std::atoll(rest.c_str());
    if (key == "command") h.command = rest;
  }
  return h.pid > 0 && h.port > 0;
}

bool hosted_write(const std::string& state, const Hosted& h) {
  std::error_code ec;
  fs::create_directories(state, ec);
  std::ofstream f(hosted_path(state, h.port));
  f << "pid " << h.pid << "\nport " << h.port << "\nkind " << h.kind << "\nlog " << h.log
    << "\nstarted " << h.started << "\ncommand " << h.command << "\n";
  return static_cast<bool>(f);
}

std::vector<Hosted> hosted_list(const std::string& state) {
  std::vector<Hosted> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(state, ec)) {
    const std::string n = e.path().filename().string();
    if (n.rfind("hosted-", 0) != 0) continue;
    Hosted h;
    if (hosted_read(e.path().string(), h)) out.push_back(h);
  }
  std::sort(out.begin(), out.end(),
            [](const Hosted& a, const Hosted& b) { return a.port < b.port; });
  return out;
}

void hosted_forget(const std::string& state, int port) {
  std::error_code ec;
  fs::remove(hosted_path(state, port), ec);
  fs::remove(cfg_path(state, port), ec);
}

bool hosted_alive(const Hosted& h) { return h.pid > 0 && process_alive(h.pid); }

std::vector<std::string> spawn_argv(const SpawnAsk& ask) {
  std::vector<std::string> argv = {ask.program};
  if (!ask.root.empty()) argv.push_back("--root=" + ask.root);
  argv.push_back("--port=" + std::to_string(ask.port));
  argv.push_back("--for=" + core::num::to_string(ask.seconds));
  argv.push_back("--realtime");
  argv.push_back("--status=60");
  if (!ask.mods.empty()) {
    std::string m;
    for (const std::string& x : ask.mods) m.append(m.empty() ? "" : ",").append(x);
    argv.push_back("--mod=" + m);
  }
  if (!ask.map.empty()) argv.push_back("--map=" + ask.map);
  if (!ask.name.empty()) argv.push_back("--name=" + ask.name);
  if (ask.maxclients > 0) argv.push_back("--maxclients=" + std::to_string(ask.maxclients));
  if (ask.bots >= 0) argv.push_back("--bots=" + std::to_string(ask.bots));
  if (!ask.password.empty()) argv.push_back("--password=" + ask.password);
  argv.insert(argv.end(), ask.extra.begin(), ask.extra.end());
  // The cfg handed over runs after the flags, as the command line's last word.
  if (!ask.cfg.empty()) {
    argv.push_back("+exec");
    argv.push_back(fs::absolute(cfg_path(ask.state, ask.port)).string());
  }
  return argv;
}

std::string shown_command(const std::vector<std::string>& argv, const std::string& password) {
  std::string s;
  for (const std::string& a : argv) {
    std::string w = a;
    if (!password.empty() && w == "--password=" + password) w = "--password=***";
    const bool q = w.find_first_of(" \t\"") != std::string::npos || w.empty();
    s += (s.empty() ? "" : " ") + (q ? "\"" + w + "\"" : w);
  }
  return s;
}

bool spawn_server(const SpawnAsk& ask, Hosted& out, std::string& error) {
  if (ask.port <= 0 || ask.port > 65535) {
    error = "port " + std::to_string(ask.port) + " is no port: pick one in 1..65535";
    return false;
  }
  if (ask.state.empty()) {
    error = "no state folder to keep the record in: name the game's state folder";
    return false;
  }
  Hosted was;
  if (hosted_read(hosted_path(ask.state, ask.port), was) && hosted_alive(was)) {
    error = "a server this machine started already runs on port " + std::to_string(ask.port) +
            " (process " + std::to_string(was.pid) + "): stop it, or pick another port";
    return false;
  }
  if (!fs::exists(ask.program)) {
    error = ask.program + " is not there: build the game's dedicated server";
    return false;
  }
  std::error_code ec;
  fs::create_directories(ask.state, ec);
  const std::string logs = ask.logs.empty() ? ask.state : ask.logs;
  fs::create_directories(logs, ec);
  if (!ask.cfg.empty()) {
    std::ofstream f(cfg_path(ask.state, ask.port));
    f << ask.cfg << (ask.cfg.back() == '\n' ? "" : "\n");
    if (!f) {
      error = "the cfg for port " + std::to_string(ask.port) + " cannot be written in " +
              ask.state + ": check the folder's permissions";
      return false;
    }
  }
  const std::vector<std::string> argv = spawn_argv(ask);
  const std::string log =
      (fs::path(logs) / ("dedicated-" + std::to_string(ask.port) + ".log")).string();
  long pid = 0;
  if (!spawn_detached(argv, ask.root, log, pid, error)) return false;
  out = Hosted{pid,
               ask.port,
               "dedicated",
               log,
               static_cast<long long>(std::time(nullptr)),
               shown_command(argv, ask.password)};
  if (!hosted_write(ask.state, out)) {
    error = "the server started (process " + std::to_string(pid) +
            ") but its record cannot be written in " + ask.state +
            ": check the folder's permissions, then stop it by its process";
    return false;
  }
  return true;
}

bool server_answers(int port, uint32_t protocol, int wait_ms, net::Advert* advert) {
  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), nullptr)) return false;
  const gn::Address to = gn::loopback_v4(static_cast<uint16_t>(port));
  uint8_t buf[gn::kMaxDatagram];
  const uint64_t nonce = 0x5e57a11ull ^ static_cast<uint64_t>(port);
  // Timed by the clock, not by counting sleeps: a loaded machine sleeps longer than asked.
  const auto start = std::chrono::steady_clock::now();
  const auto elapsed_ms = [&] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
  };
  bool ok = false;
  for (long long sent_at = -400; elapsed_ms() < wait_ms && !ok;) {
    if (elapsed_ms() - sent_at >= 400) {
      gn::lan_probe(t, to, protocol, nonce, elapsed_ms() * 1000);
      sent_at = elapsed_ms();
    }
    gn::Address from;
    for (int got; (got = gn::transport_receive(t, buf, sizeof buf, from, 0)) > 0;) {
      uint64_t n = 0;
      gn::Advert a;
      if (gn::lan_read_advert(buf, static_cast<size_t>(got), protocol, n, a) && n == nonce) {
        ok = true;
        if (advert != nullptr) *advert = a;
      }
    }
    if (!ok) std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  gn::close_transport(t);
  return ok;
}

bool stop_hosted(const std::string& state, const Hosted& h, std::string& error, int grace_ms) {
  if (!process_stop(h.pid, grace_ms, error)) return false;
  hosted_forget(state, h.port);
  return true;
}

}  // namespace ge::server
