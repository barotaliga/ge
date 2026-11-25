// pingservers (q2_pingservers.hpp).
//
// ge:summary pingservers' asks and answers; passive's listening and the call taken
// ge:keywords pingservers pingservers_step info broadcast passive passive_step
// ge:skill net console
// ge:test tests/shooters_edge_q2_pingservers_test.cpp
#include "shooters_edge/net/q2_pingservers.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include "net/udp_socket.hpp"
#include "shooters_edge/net/q2_net.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

namespace {

constexpr uint8_t kAsk[] = {0xff, 0xff, 0xff, 0xff, 'i', 'n', 'f', 'o', ' ', '3', '4', 0};

gn::Transport& socket_of(PingSweep& s) { return s.link != nullptr ? *s.link : s.own; }

}  // namespace

void add_pingservers_command(ge::console::Console& con, PingSweep& sweep, double (*now_s)()) {
  con.add_command(
      "pingservers", "pingservers: the servers on the LAN and in the address book, each one's line",
      [&sweep, now_s](ge::console::Console& c, const ge::console::Args&) {
        if (sweep.link == nullptr && !sweep.opened) {
          if (!gn::open_udp_transport(sweep.own, gn::ipv4(0, 0, 0, 0, 0), nullptr)) {
            c.print("Couldn't open a network socket to ask the servers.");
            return;
          }
          sweep.opened = true;
        }
        const double now = now_s();
        gn::Transport& t = socket_of(sweep);
        const auto ask = [&](const gn::Address& to, bool broadcast) {
          gn::transport_send(t, to, kAsk, sizeof kAsk, 0);
          sweep.asks.push_back({to, broadcast, now + (broadcast ? kPingBroadcastS : kPingDirectS)});
        };
        if (sweep.link == nullptr) ask(gn::ipv4(255, 255, 255, 255, kPortQ2), true);
        for (int k = 0;; k++) {
          const ge::console::Cvar* adr = c.cvars().find("adr" + std::to_string(k));
          if (adr == nullptr) break;
          if (adr->value.empty()) continue;
          std::string where = adr->value;
          if (where.find(':') == std::string::npos) where += ":" + std::to_string(kPortQ2);
          gn::Address to;
          if (!gn::parse_address(where, to) && !gn::resolve_ipv4(where, kPortQ2, to, nullptr)) {
            c.print("Bad address: " + adr->value);
            continue;
          }
          ask(to, false);
        }
      });
}

void pingservers_step(PingSweep& s, double now_s, int64_t now_us, std::vector<std::string>& said) {
  if (s.asks.empty()) return;
  gn::Transport& t = socket_of(s);
  uint8_t buf[2048];
  gn::Address from;
  for (int n; (n = gn::transport_receive(t, buf, sizeof buf, from, now_us)) >= 0;) {
    if (n < 9 || std::memcmp(buf, "\xff\xff\xff\xffinfo\n", 9) != 0) continue;
    // The newest ask it answers: a broadcast takes any answer, an address its own server's.
    auto it = std::find_if(s.asks.rbegin(), s.asks.rend(), [&](const PingSweep::Ask& a) {
      return now_s <= a.until_s && (a.broadcast || gn::same_host(a.to, from));
    });
    if (it == s.asks.rend()) continue;
    std::string text(reinterpret_cast<const char*>(buf) + 9,
                     strnlen(reinterpret_cast<const char*>(buf) + 9, static_cast<size_t>(n) - 9));
    while (!text.empty() && text.back() == '\n') text.pop_back();
    said.push_back(text);
    if (!it->broadcast) s.asks.erase(std::next(it).base());  // one answer each
  }
  std::erase_if(s.asks, [&](const PingSweep::Ask& a) { return now_s > a.until_s; });
  if (s.asks.empty() && s.opened) {
    gn::close_transport(s.own);
    s.opened = false;
  }
}

void add_passive_command(ge::console::Console& con, PassiveListen& p) {
  con.add_command(
      "passive", "passive: wait for a server to call and join it (typed again: stop waiting)",
      [&p](ge::console::Console& c, const ge::console::Args&) {
        if (p.on) {
          p.on = false;
          if (p.link == nullptr) gn::close_transport(p.own);
          c.print("No longer listening for passive connections.");
          return;
        }
        if (c.has_command("disconnect")) c.execute("disconnect");
        if (p.link == nullptr && !gn::open_udp_transport(p.own, gn::ipv4(0, 0, 0, 0, 0), nullptr))
          return;
        p.on = true;
        const gn::Transport& t = p.link != nullptr ? *p.link : p.own;
        c.print("Listening for passive connections at " + gn::to_string(t.local) + ".");
      });
}

std::string passive_step(PassiveListen& p, bool joined, int64_t now_us,
                         std::vector<std::string>& said) {
  if (!p.on) return {};
  gn::Transport& t = p.link != nullptr ? *p.link : p.own;
  std::string to;
  uint8_t buf[256];
  gn::Address from;
  for (int n; to.empty() && (n = gn::transport_receive(t, buf, sizeof buf, from, now_us)) >= 0;) {
    static constexpr char kCall[] = "\xff\xff\xff\xffpassive_connect";
    if (n >= 19 && std::memcmp(buf, kCall, 19) == 0) to = gn::to_string(from);
  }
  if (to.empty() && !joined) return {};
  p.on = false;
  if (p.link == nullptr) gn::close_transport(p.own);
  if (!to.empty()) said.push_back("Received passive connect from " + to + ".");
  return to;
}

}  // namespace shooters_edge::net
