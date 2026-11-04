// The rules runner's handle and the QC server's table: each call the host makes through a runner
// is the QC server's own entry point (qc_server.hpp, commands.hpp), unchanged.
//
// ge:summary The QC server's table of runner calls and the runner handle that owns it
// ge:keywords rules-runner qc-runner runner-calls qc-server host
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
                                  &qc_pause, &qc_carry,          &qc_destroy};
// The same calls over a QC server someone else owns (`destroy` none).
constexpr RunnerCalls kQcOverCalls = {"qc",      &qc_start,          &qc_connect, &qc_drop,
                                      &qc_step,  &qc_client_command, &qc_console, &qc_user_info,
                                      &qc_pause, &qc_carry,          nullptr};

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

QcServer* qc_of(const RulesRunner& r) {
  return r.calls == &kQcCalls || r.calls == &kQcOverCalls ? static_cast<QcServer*>(r.rules)
                                                          : nullptr;
}

}  // namespace shooters_edge::server
