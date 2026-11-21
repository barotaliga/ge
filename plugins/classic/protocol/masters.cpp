// The classic masters (masters.hpp): the list, the heartbeats, the shutdown.
//
// ge:summary The classic master list and its heartbeats, Quake's and Quake II's forms
// ge:keywords setmaster heartbeat master-server shutdown
// ge:skill classic net
// ge:test tests/classic_masters_test.cpp
#include "classic/protocol/masters.hpp"

#include <cstdio>

#include "net/udp_socket.hpp"

namespace ge::classic::protocol {

namespace {

std::vector<uint8_t> bytes_of(std::string_view text) { return {text.begin(), text.end()}; }

std::vector<uint8_t> oob(std::string_view text) {
  std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
  b.insert(b.end(), text.begin(), text.end());
  return b;
}

}  // namespace

std::vector<uint8_t> qw_heartbeat(int64_t sequence, int clients) {
  return bytes_of("a\n" + std::to_string(sequence) + "\n" + std::to_string(clients) + "\n");
}
std::vector<uint8_t> qw_master_shutdown() { return bytes_of("C\n"); }
std::vector<uint8_t> qw_master_ping() { return {'k', 0}; }
std::vector<uint8_t> q2_heartbeat(const std::string& status) { return oob("heartbeat\n" + status); }
std::vector<uint8_t> q2_master_shutdown() { return oob("shutdown"); }

bool resolve_master(std::string_view text, uint16_t port, net::Address& out) {
  if (text.empty()) return false;
  if (net::parse_address(text, out)) return true;
  if (text.starts_with("sim:"))
    return net::parse_address(std::string(text) + ":" + std::to_string(port), out);
  return net::resolve_ipv4(text, port, out, nullptr);
}

bool masters_started(const Masters& m) { return m.last_s > -1e8; }

void masters_start(Masters& m, MasterFamily family, double now_s) {
  m.family = family;  // a list set before the first frame stays
  m.last_s = family == MasterFamily::qw ? now_s - kBeatEvery : now_s - kBeatEvery + kQ2FirstRound;
}

void set_masters(Masters& m, const std::vector<std::string>& args, const MasterResolve& resolve,
                 std::vector<std::string>& said, std::vector<MasterSend>& out) {
  m.list.clear();
  m.names.clear();
  m.heard_s.clear();
  if (m.family == MasterFamily::qw) {
    for (const std::string& a : args) {
      net::Address to;
      if (a == "none" || !resolve(a, kQwMasterPort, to)) {
        said.emplace_back("Setting nomaster mode.");
        return;
      }
      if (static_cast<int>(m.list.size()) >= kMostMasters) break;  // the ninth and on: not kept
      m.list.push_back(to);
      said.push_back("Master server at " + net::to_string(to));
      said.emplace_back("Sending a ping.");
      out.push_back({to, qw_master_ping()});
    }
    m.now = true;
    return;
  }
  for (const std::string& a : args) {
    if (static_cast<int>(m.list.size()) >= kMostMasters) {
      said.emplace_back("Too many masters.");
      break;
    }
    net::Address to;
    if (!resolve(a, kQ2MasterPort, to)) {
      said.push_back("Couldn't resolve master: " + a);
      continue;
    }
    m.list.push_back(to);
    m.names.push_back(a);
    m.heard_s.push_back(-1);
    said.push_back("Master server at " + net::to_string(to) + ".");
  }
  m.is_public = true;
  m.now = true;
}

void force_heartbeat(Masters& m) { m.now = true; }

void masters_heard(Masters& m, const net::Address& from, double now_s) {
  for (size_t i = 0; i < m.list.size() && i < m.heard_s.size(); i++)
    if (m.list[i] == from) m.heard_s[i] = now_s;
}

std::string masters_list_text(const Masters& m, double now_s, bool running) {
  if (m.list.empty()) return "There are no masters.\n";
  std::string t = "num hostname              lastmsg address\n";
  t += "--- --------------------- ------- ---------------------\n";
  char row[160];
  for (size_t i = 0; i < m.list.size(); i++) {
    const double heard = i < m.heard_s.size() ? m.heard_s[i] : -1;
    const std::string last = !running ? "down"
                             : heard < 0
                                 ? "never"
                                 : std::to_string(static_cast<int64_t>((now_s - heard) * 1000));
    const std::string name = i < m.names.size() ? m.names[i] : net::to_string(m.list[i]);
    std::snprintf(row, sizeof row, "%3d %-21.21s %7s %-21s\n", static_cast<int>(i + 1),
                  name.c_str(), last.c_str(), net::to_string(m.list[i]).c_str());
    t += row;
  }
  return t;
}

void masters_frame(Masters& m, double now_s, int clients, const std::string& status, bool dedicated,
                   std::vector<MasterSend>& out, std::vector<std::string>& said) {
  if (m.family == MasterFamily::qw) {
    if (!m.now && now_s - m.last_s < kBeatEvery) return;
    m.now = false;
    m.last_s = now_s;
    m.sequence++;  // counted even when no master hears it
    for (const net::Address& to : m.list) {
      said.push_back("Sending heartbeat to " + net::to_string(to));
      out.push_back({to, qw_heartbeat(m.sequence, clients)});
    }
    return;
  }
  if (!dedicated || !m.is_public) return;
  if (!m.in_round && (m.now || now_s - m.last_s >= kBeatEvery)) {
    m.now = false;
    m.last_s = now_s;
    m.in_round = true;
    m.round_next = 0;
  }
  if (!m.in_round) return;
  if (m.round_next < m.list.size()) {  // one master a frame
    const net::Address& to = m.list[m.round_next++];
    said.push_back("Sending heartbeat to " + net::to_string(to));
    out.push_back({to, q2_heartbeat(status)});
  }
  if (m.round_next >= m.list.size()) m.in_round = false;
}

void masters_shutdown(const Masters& m, std::vector<MasterSend>& out,
                      std::vector<std::string>& said) {
  if (m.family == MasterFamily::q2 && !m.is_public) return;
  for (const net::Address& to : m.list) {
    if (m.family == MasterFamily::qw) said.push_back("Sending heartbeat to " + net::to_string(to));
    out.push_back({to, m.family == MasterFamily::qw ? qw_master_shutdown() : q2_master_shutdown()});
  }
}

}  // namespace ge::classic::protocol
