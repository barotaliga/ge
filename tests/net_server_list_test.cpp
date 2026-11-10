// engine/net/server_list.hpp over the simulated network, with a toy protocol of the test's own
// (a query `?`, a reply `!name|map|mode|players|max`, a master's `M` answered `L` then four-byte
// node ids): servers found on the LAN by the broadcast, from two masters (one lost), and by name;
// each pinged by its link's round trip; a lossy server asked again; a silent one given up; the
// filters and the sorts; the cap on servers kept; a refresh replayed to the bit.
//
// ge:summary Test: the server list: LAN, masters and named servers, pings, retries, view, cap
// ge:keywords test net server-list status-query master-server lan broadcast ping retry filter sort
// ge:skill net testing
#include "net/server_list.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

constexpr uint16_t kPort = 26000, kLanPort = 26001;  // the LAN's servers listen apart

size_t toy_query(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = '?';
  return 1;
}

bool toy_read(const uint8_t* data, size_t size, ServerInfo& out) {
  if (size < 1 || data[0] != '!') return false;
  const std::string text(reinterpret_cast<const char*>(data) + 1, size - 1);
  std::vector<std::string> f;
  size_t at = 0;
  for (;;) {
    const size_t bar = text.find('|', at);
    f.push_back(text.substr(at, bar == std::string::npos ? std::string::npos : bar - at));
    if (bar == std::string::npos) break;
    at = bar + 1;
  }
  if (f.size() != 5) return false;
  out = ServerInfo{};
  out.name = f[0];
  out.map = f[1];
  out.mode = f[2];
  out.players = std::atoi(f[3].c_str());
  out.max_players = std::atoi(f[4].c_str());
  return true;
}

size_t toy_master(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = 'M';
  return 1;
}

bool toy_read_master(const uint8_t* data, size_t size, std::vector<Address>& out) {
  if (size < 1 || data[0] != 'L') return false;
  for (size_t at = 1; at + 4 <= size; at += 4) {
    uint32_t node = 0;
    std::memcpy(&node, data + at, 4);
    out.push_back(sim_address(node, kPort));
  }
  return true;
}

ServerQuery toy() { return {toy_query, toy_read, toy_master, toy_read_master}; }

struct Toy {
  Transport t;
  std::string reply;
};

struct Master {
  Transport t;
  std::vector<uint32_t> nodes;
};

struct World {
  SimNetwork net = make_sim_network(9, LinkConditions{});
  std::vector<Toy> servers;
  std::vector<Master> masters;
  Transport bt;
  ServerList list;

  void add_server(uint32_t node, const std::string& reply, double latency_ms, double loss = 0,
                  uint16_t port = kPort) {
    LinkConditions lc;
    lc.latency_ms = latency_ms;
    lc.loss = loss;
    sim_set_path(net, 1, node, lc);
    servers.push_back({});
    open_sim_transport(servers.back().t, net, sim_address(node, port));
    servers.back().reply = reply;
  }
  void add_master(uint32_t node, std::vector<uint32_t> nodes) {
    masters.push_back({});
    open_sim_transport(masters.back().t, net, sim_address(node, 27950));
    masters.back().nodes = std::move(nodes);
  }
  void run(int64_t from, int64_t to) {
    uint8_t buf[2048];
    Address from_addr;
    for (int64_t now = from; now <= to; now += 5000) {
      sim_advance(net, now);
      for (Toy& s : servers) {
        while (transport_receive(s.t, buf, sizeof buf, from_addr, now) >= 0) {
          std::string r = "!" + s.reply;
          transport_send(s.t, from_addr, reinterpret_cast<const uint8_t*>(r.data()), r.size(), now);
        }
      }
      for (Master& m : masters) {
        while (transport_receive(m.t, buf, sizeof buf, from_addr, now) >= 0) {
          std::vector<uint8_t> r = {'L'};
          for (uint32_t n : m.nodes) {
            const auto* p = reinterpret_cast<const uint8_t*>(&n);
            r.insert(r.end(), p, p + 4);
          }
          transport_send(m.t, from_addr, r.data(), r.size(), now);
        }
      }
      server_list_update(list, now);
    }
  }
};

void build(World& w, ServerListConfig& cfg) {
  // Two LAN servers (nodes 10, 11: the broadcast), four the masters know (20..23), one named
  // (30), one named that never answers (31: nothing bound there), one lossy (24: 30 % lost each
  // way).
  w.add_server(10, "lan duel|arena|duel|1|2", 1, 0, kLanPort);
  w.add_server(11, "lan team|keep|team|6|8", 2, 0, kLanPort);
  w.add_server(20, "far ffa|arena|ffa|8|8", 60);
  w.add_server(21, "near ffa|halls|ffa|3|16", 15);
  w.add_server(22, "empty coop|start|coop|0|8", 30);
  w.add_server(23, "mid team|keep|team|4|8", 40);
  w.add_server(24, "lossy duel|arena|duel|2|2", 25, 0.3);
  w.add_server(30, "named|halls|ffa|5|12", 10);
  w.add_master(50, {20, 21, 22});
  w.add_master(51, {21, 22, 23, 24});
  open_sim_transport(w.bt, w.net, sim_address(1, 27001));
  cfg.lan = {sim_broadcast(kLanPort)};
  cfg.masters = {sim_address(50, 27950), sim_address(51, 27950), sim_address(52, 27950)};
  cfg.servers = {sim_address(30, kPort), sim_address(31, kPort)};
  cfg.tries = 6;
}

std::vector<std::string> names(const ServerList& l, const std::vector<int>& rows) {
  std::vector<std::string> out;
  for (int i : rows) out.push_back(l.entries[static_cast<size_t>(i)].info.name);
  return out;
}

}  // namespace

TEST(server_list_finds_lan_master_and_named_servers) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 10000);
  w.run(15000, 6000000);
  CHECK(server_list_done(w.list, 6000000));
  CHECK(w.list.entries.size() == 9);  // 2 LAN + 5 from masters + 2 named
  CHECK(server_list_answered(w.list) == 8);
  CHECK(w.list.stats.master_replies == 2);
  CHECK(w.list.stats.master_asks == 2 + 3);  // the third master asked three times, given up
  int lan = 0;
  for (const ServerEntry& e : w.list.entries) {
    if (e.from & kServerFromLan) lan++;
    if (e.info.name == "near ffa") {
      CHECK(e.from == kServerFromMaster);
      CHECK(e.ping_ms >= 30 && e.ping_ms <= 36);  // 15 ms each way, a 5 ms update
    }
    if (e.info.name == "named") CHECK(e.from == kServerFromNamed);
    if (e.address == sim_address(31, kPort)) {
      CHECK(!e.answered);
      CHECK(e.tries == cfg.tries);
    }
  }
  CHECK(lan == 2);
}

TEST(server_list_view_filters_and_sorts) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 6000000);
  std::vector<int> rows;
  ServerFilter f;
  server_list_view(w.list, f, ServerSort::ping, rows);
  const std::vector<std::string> by_ping = names(w.list, rows);
  CHECK(by_ping.size() == 8);
  CHECK(by_ping.front() == "lan duel");
  CHECK(by_ping.back() == "far ffa");
  server_list_view(w.list, f, ServerSort::players, rows);
  CHECK(names(w.list, rows).front() == "far ffa");  // 8 players
  server_list_view(w.list, f, ServerSort::name, rows);
  CHECK(names(w.list, rows).front() == "empty coop");
  f.hide_full = true;
  f.hide_empty = true;
  server_list_view(w.list, f, ServerSort::name, rows);
  CHECK(rows.size() == 5);  // far ffa, lossy duel, empty coop gone
  f = ServerFilter{};
  f.mode = "TEAM";
  server_list_view(w.list, f, ServerSort::ping, rows);
  CHECK((names(w.list, rows) == std::vector<std::string>{"lan team", "mid team"}));
  f = ServerFilter{};
  f.name_has = "HALLS";  // a map
  server_list_view(w.list, f, ServerSort::name, rows);
  CHECK((names(w.list, rows) == std::vector<std::string>{"named", "near ffa"}));
  f = ServerFilter{};
  f.max_ping_ms = 40;
  server_list_view(w.list, f, ServerSort::ping, rows);
  CHECK(rows.size() == 4);  // the two LAN, named (20), near (30)
  f = ServerFilter{};
  f.hide_silent = false;
  server_list_view(w.list, f, ServerSort::ping, rows);
  CHECK(rows.size() == 9);
  CHECK(!w.list.entries[static_cast<size_t>(rows.back())].answered);  // the silent one last
}

TEST(server_list_asks_a_lossy_server_again) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 8000000);
  bool found = false;
  for (const ServerEntry& e : w.list.entries) {
    if (e.address != sim_address(24, kPort)) continue;
    found = true;
    CHECK(e.answered);
    CHECK(e.tries >= 1);
  }
  CHECK(found);
}

TEST(server_list_keeps_at_most_its_cap) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  cfg.most = 4;
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 6000000);
  CHECK(w.list.entries.size() == 4);
  CHECK(w.list.stats.dropped > 0);
}

TEST(server_list_refresh_replays_to_the_bit) {
  std::vector<int> pings[2];
  for (int run = 0; run < 2; run++) {
    World w;
    ServerListConfig cfg;
    build(w, cfg);
    server_list_init(w.list, cfg, toy(), w.bt);
    server_list_refresh(w.list, 0);
    w.run(5000, 6000000);
    for (const ServerEntry& e : w.list.entries) pings[run].push_back(e.ping_ms);
  }
  CHECK(pings[0] == pings[1]);
}

TEST(server_list_second_refresh_starts_again) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 6000000);
  const size_t first = w.list.entries.size();
  server_list_refresh(w.list, 6005000);
  CHECK(w.list.entries.size() == 2);  // the named, before any answer
  w.run(6010000, 12000000);
  CHECK(w.list.entries.size() == first);
  CHECK(server_list_done(w.list, 12000000));
}
