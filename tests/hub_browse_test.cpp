// engine/hub/browse.hpp over the simulated network, with a toy status protocol of the test's own
// (a query `?`, a reply `!name|map|mode|state|max|roster`, the roster `name:team:frags:s;...`; a
// master's `M` answered `L` then four-byte node ids): a server's answer read into the record
// (spectators flagged, teams added up, the phase from its state); servers from a master and by
// name land in the index as browsed nodes the poller never asks; a change is counted; a server
// gone silent is dropped; the engine's own master taking a browsed node over; a refresh replayed
// to the bit.
//
// ge:summary Test: the hub's browse: foreign status answers as records, merged, dropped, replayed
// ge:keywords test hub browse classic adapter status_of server-list merge drop browsed replay
// ge:skill net testing
#include "hub/browse.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"

using namespace ge::hub;
namespace gn = ge::net;

namespace {

constexpr uint16_t kPort = 26000;

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> f;
  size_t at = 0;
  for (;;) {
    const size_t bar = text.find(sep, at);
    f.push_back(text.substr(at, bar == std::string::npos ? std::string::npos : bar - at));
    if (bar == std::string::npos) return f;
    at = bar + 1;
  }
}

size_t toy_query(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = '?';
  return 1;
}

bool toy_read(const uint8_t* data, size_t size, gn::ServerInfo& out) {
  if (size < 1 || data[0] != '!') return false;
  const std::vector<std::string> f =
      split(std::string(reinterpret_cast<const char*>(data) + 1, size - 1), '|');
  if (f.size() != 6) return false;
  out = gn::ServerInfo{};
  out.name = f[0];
  out.map = f[1];
  out.mode = f[2];
  out.state = f[3];
  out.max_players = std::atoi(f[4].c_str());
  if (!f[5].empty()) {
    for (const std::string& p : split(f[5], ';')) {
      const std::vector<std::string> q = split(p, ':');
      if (q.size() != 4) return false;
      out.roster.push_back({q[0], q[1], std::atoi(q[2].c_str()), 20, q[3] == "s"});
    }
  }
  out.keys = {{"hostname", out.name}, {"map", out.map}};
  return true;
}

size_t toy_master(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = 'M';
  return 1;
}

bool toy_read_master(const uint8_t* data, size_t size, std::vector<gn::Address>& out) {
  if (size < 1 || data[0] != 'L') return false;
  for (size_t at = 1; at + 4 <= size; at += 4) {
    uint32_t node = 0;
    std::memcpy(&node, data + at, 4);
    out.push_back(gn::sim_address(node, kPort));
  }
  return true;
}

gn::ServerQuery toy() { return {toy_query, toy_read, toy_master, toy_read_master}; }

struct Toy {
  gn::Transport t;
  std::string reply;
  bool silent = false;
};

struct World {
  gn::SimNetwork net = gn::make_sim_network(5, gn::LinkConditions{});
  std::vector<Toy> servers;
  gn::Transport master, hub;
  std::vector<uint32_t> listed;
  Poller poller;
  Browse browse;

  World() {
    servers.reserve(8);
    for (uint32_t n : {20u, 21u, 30u}) {
      gn::LinkConditions lc;
      lc.latency_ms = 10.0 + n;
      gn::sim_set_path(net, 1, n, lc);
    }
    add(20, "Duel Hall|tower|1on1|Countdown|2|ann::12:;bo::9:");
    add(21, "Team Two|keep|4on4||8|cy:red:20:;di:red:5:;ed:blue:11:;fy::0:s");
    add(30, "Named|halls|ffa|Standby|16|");
    gn::open_sim_transport(master, net, gn::sim_address(50, 27000));
    listed = {20, 21};
    gn::open_sim_transport(hub, net, gn::sim_address(1, 27001));
    PollerConfig pc;  // no engine master: the browse alone fills the index
    poller_init(poller, pc, 0);
    BrowseConfig bc;
    bc.masters = {gn::sim_address(50, 27000)};
    bc.servers = {gn::sim_address(30, kPort)};
    bc.refresh_us = 10000000;
    bc.drop_us = 25000000;
    browse_init(browse, bc, toy(), hub, 0);
  }
  void add(uint32_t node, const std::string& reply) {
    servers.push_back({});
    gn::open_sim_transport(servers.back().t, net, gn::sim_address(node, kPort));
    servers.back().reply = reply;
  }
  void run(int64_t from, int64_t to) {
    uint8_t buf[2048];
    gn::Address at;
    gn::Transport none;
    for (int64_t now = from; now <= to; now += 5000) {
      gn::sim_advance(net, now);
      for (Toy& s : servers) {
        while (gn::transport_receive(s.t, buf, sizeof buf, at, now) >= 0) {
          if (s.silent) continue;
          const std::string r = "!" + s.reply;
          gn::transport_send(s.t, at, reinterpret_cast<const uint8_t*>(r.data()), r.size(), now);
        }
      }
      while (gn::transport_receive(master, buf, sizeof buf, at, now) >= 0) {
        std::vector<uint8_t> r = {'L'};
        for (uint32_t n : listed) {
          const auto* p = reinterpret_cast<const uint8_t*>(&n);
          r.insert(r.end(), p, p + 4);
        }
        gn::transport_send(master, at, r.data(), r.size(), now);
      }
      browse_update(browse, poller, now);
      poller_update(poller, hub, now);  // asks nothing: every node is browsed
    }
  }
  const HubNode* node(uint32_t n) const { return poller_find(poller, gn::sim_address(n, kPort)); }
};

}  // namespace

TEST(an_answer_reads_into_a_record) {
  gn::ServerInfo info;
  const std::string reply =
      "!Team Two|keep|4on4|Countdown|8|cy:red:20:;di:red:5:;ed:blue:11:;fy::0:s";
  CHECK(toy_read(reinterpret_cast<const uint8_t*>(reply.data()), reply.size(), info));
  const gn::Status s = status_of(info);
  CHECK(s.name == "Team Two" && s.map == "keep" && s.mode == "4on4" && s.max_players == 8);
  CHECK(s.phase == gn::StatusPhase::countdown);
  CHECK(s.players.size() == 4 && s.players[3].flags == gn::kPlayerSpectator);
  CHECK(s.players[0].score == 20 && s.players[0].ping_ms == 20);
  CHECK(s.teams.size() == 2 && s.teams[0].name == "red" && s.teams[0].score == 25);
  CHECK(s.teams[1].name == "blue" && s.teams[1].score == 11);
  CHECK(s.keys.size() == 2 && s.keys[0].first == "hostname");
  gn::ServerInfo one;
  one.state = "Standby";
  one.roster = {{"a", "red", 3, 0, false}, {"b", "red", 4, 0, false}};
  const gn::Status solo = status_of(one);
  CHECK(solo.teams.empty());  // one team named is no team game
  CHECK(solo.phase == gn::StatusPhase::lobby);
  one.state = "";
  CHECK(status_of(one).phase == gn::StatusPhase::playing);
}

TEST(servers_from_the_master_and_by_name_are_browsed_nodes) {
  World w;
  w.run(0, 3000000);
  CHECK(w.poller.nodes.size() == 3);
  for (const HubNode& n : w.poller.nodes) {
    CHECK(n.browsed && n.have);
  }
  CHECK(w.node(21)->status.teams.size() == 2);
  CHECK(w.node(20)->status.phase == gn::StatusPhase::countdown);
  CHECK(w.node(30)->status.phase == gn::StatusPhase::lobby);
  CHECK(w.poller.stats.asks == 0);  // the engine's status ask goes to none of them
  CHECK(w.browse.stats.added == 3 && w.browse.stats.merged == 3);
  // The next refresh hears the same: merged again, nothing changed.
  w.run(3005000, 13000000);
  CHECK(w.browse.stats.refreshes == 2 && w.browse.stats.merged == 6);
  CHECK(w.browse.stats.changes == 3);  // the first record of each
  // A frag lands: a change.
  w.servers[0].reply = "Duel Hall|tower|1on1||2|ann::13:;bo::9:";
  w.run(13005000, 23000000);
  CHECK(w.browse.stats.changes == 4);
  CHECK(w.node(20)->status.players[0].score == 13);
  CHECK(w.node(20)->status.phase == gn::StatusPhase::playing);
}

TEST(a_silent_server_is_dropped) {
  World w;
  w.run(0, 3000000);
  CHECK(w.poller.nodes.size() == 3);
  w.servers[2].silent = true;
  w.listed = {20};  // and the master forgets the team game
  w.run(3005000, 40000000);
  CHECK(w.poller.nodes.size() == 1 && w.node(20) != nullptr);
  CHECK(w.browse.stats.dropped == 2);
}

TEST(a_refresh_replays_to_the_bit) {
  World a, b;
  a.run(0, 23000000);
  b.run(0, 23000000);
  CHECK(a.poller.nodes.size() == b.poller.nodes.size());
  for (size_t i = 0; i < a.poller.nodes.size(); i++) {
    CHECK(a.poller.nodes[i].address == b.poller.nodes[i].address);
    CHECK(a.poller.nodes[i].record_hash == b.poller.nodes[i].record_hash);
    CHECK(a.poller.nodes[i].heard_us == b.poller.nodes[i].heard_us);
  }
  CHECK(a.poller.serial == b.poller.serial);
}

TEST(no_masters_and_no_servers_browses_nothing) {
  Browse b;
  Poller p;
  poller_init(p, PollerConfig{}, 0);
  gn::Transport t;
  browse_init(b, BrowseConfig{}, toy(), t, 0);
  CHECK(!b.on);
  browse_update(b, p, 1000000);
  CHECK(p.nodes.empty() && b.stats.refreshes == 0);
}
