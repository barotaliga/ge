// The rules runner's handle and the QC server's table: each call the host makes through a runner
// is the QC server's own entry point (qc_server.hpp, commands.hpp), unchanged.
//
// The C++ mods on top of any runner on the loop's program seam: each map's hosts, laid over the
// program the runner made the loop with, their log lines and their part of the digest.
//
// ge:summary The QC server's table of runner calls, the runner handle, the C++ mods on top
// ge:keywords rules-runner qc-runner runner-calls qc-server host start_mods log_mods rules_hash
// ge:skill progs shooter
// ge:test tests/shooters_edge_rules_runner_test.cpp
#include "shooters_edge/server/rules_runner.hpp"

#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace {

QcServer& qc(void* rules) { return *static_cast<QcServer*>(rules); }

bool qc_start(void* r, const MapSpec& map, std::string& error) {
  return load_map(qc(r), map, error);
}
void qc_connect(void* r, int slot, const std::string& name, bool spectator, bool bot,
                const float* parms) {
  connect(qc(r), slot, name, spectator, bot, parms);
}
void qc_drop(void* r, int slot) { disconnect(qc(r), slot); }
void qc_step(void* r, const sh::Packet* packets, int count) { qc_frame(qc(r), packets, count); }
CommandResult qc_client_command(void* r, int slot, const std::string& line) {
  return client_command(qc(r), slot, line);
}
bool qc_console(void* r, const std::string& line) { return server_command(qc(r), line); }
void qc_user_info(void* r, int slot, const std::string& key, const std::string& value) {
  set_user_info(qc(r), slot, key, value);
}
bool qc_pause(void* r, int slot, bool on) { return request_pause(qc(r), slot, on); }
void qc_carry(void* r) { save_change_parms(qc(r)); }
void qc_destroy(void* r) { delete static_cast<QcServer*>(r); }

constexpr RunnerCalls kQcCalls = {"qc",      &qc_start,          &qc_connect, &qc_drop,
                                  &qc_step,  &qc_client_command, &qc_console, &qc_user_info,
                                  &qc_pause, &qc_carry,          &qc_destroy, true};
// The same calls over a QC server someone else owns (`destroy` none).
constexpr RunnerCalls kQcOverCalls = {"qc",      &qc_start,          &qc_connect, &qc_drop,
                                      &qc_step,  &qc_client_command, &qc_console, &qc_user_info,
                                      &qc_pause, &qc_carry,          nullptr,     true};

}  // namespace

RulesRunner& RulesRunner::operator=(RulesRunner&& o) noexcept {
  if (this != &o) {
    if (rules != nullptr && calls->destroy != nullptr) calls->destroy(rules);
    calls = o.calls;
    rules = o.rules;
    common = o.common;
    o.calls = nullptr;
    o.rules = nullptr;
    o.common = nullptr;
  }
  return *this;
}

RulesRunner::~RulesRunner() {
  if (rules != nullptr && calls->destroy != nullptr) calls->destroy(rules);
}

RulesRunner qc_runner(std::unique_ptr<QcServer> s) {
  QcServer* p = s.release();
  return {&kQcCalls, p, p};
}

RulesRunner qc_runner_over(QcServer& s) { return {&kQcOverCalls, &s, &s}; }

bool start_mods(RulesCommon& s, const RunnerCalls& calls, std::string& error) {
  RulesMods& m = s.mods;
  m.hosts.clear();
  if (m.tables.empty()) return true;
  if (!calls.program_seam && calls.own_mods != nullptr) {
    if (!calls.own_mods(s, error)) {
      m.hosts.clear();
      return false;
    }
    log_mods(s);
    return true;
  }
  if (!calls.program_seam) {
    error = std::string("This map's rules (") + calls.form +
            ") can't run C++ mods yet. Take the mod out of sv_mod, or play it on a classic map.";
    return false;
  }
  for (size_t k = 0; k < m.tables.size(); k++) {
    auto h = std::make_unique<sim::ModHost>();
    const std::vector<std::string> refused =
        sim::create_mod_host(*h, m.tables[k], s.sv.program, m.seed + static_cast<uint32_t>(k));
    if (!refused.empty()) {
      error = "the mod " + (k < m.names.size() ? m.names[k] : std::to_string(k + 1)) +
              " was refused: " + refused.front();
      m.hosts.clear();
      return false;
    }
    s.sv.program = sim::mod_program(*h);
    m.hosts.push_back(std::move(h));
  }
  for (const auto& h : m.hosts) sim::mod_match_start(*h, s.sv);
  log_mods(s);
  return true;
}

void log_mods(RulesCommon& s) {
  RulesMods& m = s.mods;
  for (size_t k = 0; k < m.hosts.size(); k++) {
    sim::ModHost& h = *m.hosts[k];
    if (h.log.empty() && h.log_dropped == 0) continue;
    std::string who = "mod ";
    who.append(k < m.names.size() ? m.names[k] : std::to_string(k + 1)).append(": ");
    for (const std::string& line : h.log) s.console.push_back(who + line);
    if (h.log_dropped > 0)
      s.console.push_back(who + std::to_string(h.log_dropped) + " more lines dropped");
    h.log.clear();
    h.log_dropped = 0;
  }
}

uint64_t rules_hash(const RulesCommon& s) {
  uint64_t h = sh::server_hash(s.sv);
  for (const auto& m : s.mods.hosts) h = sim::mod_hash(m->run, h);
  return h;
}

QcServer* qc_of(const RulesRunner& r) {
  return r.calls == &kQcCalls || r.calls == &kQcOverCalls ? static_cast<QcServer*>(r.rules)
                                                          : nullptr;
}

}  // namespace shooters_edge::server
