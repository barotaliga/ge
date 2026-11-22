// serverstatus (qw_serverstatus.hpp).
//
// ge:summary serverstatus's ask, the answer read and the block formatted
// ge:keywords serverstatus format_status serverstatus_step
// ge:skill net console
// ge:test tests/shooters_edge_qw_serverstatus_test.cpp
#include "shooters_edge/net/qw_serverstatus.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "classic/protocol/info.hpp"
#include "net/udp_socket.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_wire.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

namespace {

struct Player {
  int score = 0, ping = 0;
  std::string name;
};

// A player's line: Quake II's `score ping "name"`, or QuakeWorld's `userid frags minutes ping
// "name" ...`; false for a line that is neither.
bool read_player(const std::string& line, Player& p) {
  const size_t quote = line.find('"');
  if (quote == std::string::npos) return false;
  const size_t close = line.find('"', quote + 1);
  p.name =
      line.substr(quote + 1, close == std::string::npos ? std::string::npos : close - quote - 1);
  std::vector<int> n;
  const char* at = line.c_str();
  const char* end = at + quote;
  while (at < end) {
    char* next = nullptr;
    const long v = std::strtol(at, &next, 10);
    if (next == at) {
      at++;
      continue;
    }
    n.push_back(static_cast<int>(v));
    at = next;
  }
  if (n.size() == 2) {  // Quake II
    p.score = n[0];
    p.ping = n[1];
    return true;
  }
  if (n.size() >= 4) {  // QuakeWorld
    p.score = n[1];
    p.ping = n[3];
    return true;
  }
  return false;
}

double steady_s() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

bool format_status(std::string_view where, std::string_view text, std::vector<std::string>& out) {
  std::vector<std::string> lines;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    lines.emplace_back(text.substr(at, end - at));
    at = end + 1;
  }
  if (lines.empty() || lines[0].empty() || lines[0][0] != '\\') return false;
  std::string info = lines[0].substr(0, 511);
  std::vector<std::pair<std::string, std::string>> keys;
  if (!ge::classic::protocol::info_pairs(info, keys)) keys = {{"hostname", "badinfo"}};
  out.push_back("Status response from " + std::string(where));
  out.emplace_back();
  for (const auto& [k, v] : keys) {
    std::string key = k.empty() ? "<MISSING KEY>" : k;
    if (key.size() < 20) key.resize(20, ' ');
    out.push_back(key + " " + (v.empty() ? "<MISSING VALUE>" : v));
  }
  out.emplace_back();
  out.emplace_back("Num Score Ping Name");
  std::vector<Player> players;
  for (size_t i = 1; i < lines.size(); i++)
    if (Player p; read_player(lines[i], p)) players.push_back(p);
  std::stable_sort(players.begin(), players.end(),
                   [](const Player& a, const Player& b) { return a.score > b.score; });
  for (size_t i = 0; i < players.size(); i++) {
    char head[64];
    std::snprintf(head, sizeof head, "%3zu %5d %4d ", i + 1, players[i].score, players[i].ping);
    out.push_back(head + players[i].name);
  }
  return true;
}

void add_serverstatus_command(ge::console::Console& con, Remote& r) {
  con.add_command("serverstatus", "serverstatus [address]: a server's settings and players",
                  [&r](ge::console::Console& c, const ge::console::Args& a) {
                    std::string address = a.size() >= 2 ? a[1] : std::string();
                    if (address.empty() && r.active && !r.watch) address = gn::to_string(r.server);
                    if (address.empty()) {
                      c.print("Usage: serverstatus [address]");
                      return;
                    }
                    const bool q2 = r.q2_game && r.q2_game();
                    const uint16_t port = q2 ? kPortQ2 : kPortServer;
                    std::string where = address;
                    if (where.rfind("localhost", 0) == 0) where = "127.0.0.1" + where.substr(9);
                    if (where.find(':') == std::string::npos) where += ":" + std::to_string(port);
                    gn::Address to;
                    if (std::string why;
                        !gn::parse_address(where, to) && !gn::resolve_ipv4(where, port, to, &why)) {
                      c.print("Bad address: " + address);
                      return;
                    }
                    if (!r.status_ask) r.status_ask = std::make_unique<StatusAsk>();
                    StatusAsk& s = *r.status_ask;
                    if (!s.opened) {
                      std::string why;
                      if (!gn::open_udp_transport(s.own, gn::ipv4(0, 0, 0, 0, 0), &why)) {
                        c.print("Couldn't open a network socket to ask the server.");
                        return;
                      }
                      s.opened = true;
                    }
                    s.to = to;
                    s.until_s = steady_s() + kStatusWaitS;
                    static constexpr uint8_t kAsk[] = {0xff, 0xff, 0xff, 0xff, 's', 't',
                                                       'a',  't',  'u',  's',  '\n'};
                    gn::transport_send(s.own, to, kAsk, sizeof kAsk, r.now_us);
                  });
}

void serverstatus_step(Remote& r, double, std::vector<std::string>& said) {
  if (!r.status_ask || r.status_ask->until_s < 0) return;
  StatusAsk& s = *r.status_ask;
  uint8_t buf[4096];
  gn::Address from;
  for (int n; (n = gn::transport_receive(s.own, buf, sizeof buf, from, r.now_us)) >= 0;) {
    if (n < 5 || buf[0] != 0xff || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff) continue;
    if (!(from == s.to)) continue;
    std::string text(reinterpret_cast<const char*>(buf) + 4,
                     strnlen(reinterpret_cast<const char*>(buf) + 4, static_cast<size_t>(n) - 4));
    if (text.rfind("print\n", 0) == 0)
      text.erase(0, 6);
    else if (!text.empty() && text[0] == 'n')
      text.erase(0, 1);
    else
      continue;
    if (format_status(gn::to_string(from), text, said)) s.until_s = 0;  // answered: stop
  }
  if (steady_s() < s.until_s) return;
  s.until_s = -1;
  gn::close_transport(s.own);
  s.opened = false;
}

}  // namespace shooters_edge::net
