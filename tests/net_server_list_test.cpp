// engine/net/server_list.hpp over the simulated network, with a toy protocol of the test's own
// (a query `?`, a reply `!name|map|mode|players|max`, a master's `M` answered `L` then four-byte
// node ids): servers found on the LAN by the broadcast, from two masters (one lost), and by name;
// each pinged by its link's round trip; a lossy server asked again; a silent one given up; the
// filters and the sorts; the cap on servers kept; a refresh replayed to the bit; servers that beat
// to the engine's own master (net/master.hpp) listed beside them, only the game's and only games.
//
// ge:summary Test: the server list: LAN, masters and named servers, pings, retries, view, cap
// ge:keywords test net server-list status-query master-server lan broadcast ping retry filter sort
// ge:keywords own-masters sv-master beat
// ge:skill net testing
#include "net/server_list.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/packet.hpp"

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
  out.keys = {{"map", out.map}, {"mode", out.mode}};
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

ServerQuery toy() {
  return {toy_query, toy_read, toy_master, toy_read_master, protocol_id("toy", 1)};
}

struct Toy {
  Transport t;
  std::string reply;
};

struct ToyMaster {
  Transport t;
  std::vector<uint32_t> nodes;
};

struct World {
  SimNetwork net = make_sim_network(9, LinkConditions{});
  std::vector<Toy> servers;
  std::vector<ToyMaster> masters;
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
      for (ToyMaster& m : masters) {
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
  out.reserve(rows.size());
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

TEST(server_list_lists_the_servers_that_beat_to_the_engine_s_master) {
  // Our master on node 60; three nodes beat to it: two game servers of the game (40, 41), a TV node
  // of the game (42) and a server of another game (43). Beside it the toy master of the classic
  // list (50) names node 20, which also beats (one server, listed once, from both).
  World w;
  const uint32_t game = protocol_id("toy", 1);
  w.add_server(40, "ours one|arena|ffa|2|8", 20);
  w.add_server(41, "ours two|keep|team|0|8", 30);
  w.add_server(42, "a tv|arena|ffa|0|0", 10);
  w.add_server(43, "other game|arena|ffa|1|8", 10);
  w.add_server(20, "both|arena|ffa|3|8", 15);
  w.add_master(50, {20});
  Transport mt;
  open_sim_transport(mt, w.net, sim_address(60, kMasterPort));
  Master master;
  master_init(master, MasterConfig{}, mt);
  struct Beat {
    uint32_t node;
    uint32_t protocol;
    NodeKind kind;
  };
  const Beat beats[] = {{40, game, NodeKind::game},
                        {41, game, NodeKind::game},
                        {42, game, NodeKind::tv},
                        {43, protocol_id("other", 1), NodeKind::game},
                        {20, game, NodeKind::game}};
  std::vector<MasterBeat> b(std::size(beats));
  for (size_t i = 0; i < b.size(); i++) {
    MasterBeatConfig c;
    c.masters = {sim_address(60, kMasterPort)};
    c.protocol = beats[i].protocol;
    c.kind = beats[i].kind;
    c.seed = i + 1;
    master_beat_init(b[i], c);
  }
  // The servers beat for a second first (the toy servers' loop in World::run answers queries, so
  // the beats are sent here, the acks left unread: an unacked beat is only sent again).
  auto server_of = [&](uint32_t node) -> Transport& {
    for (Toy& t : w.servers)
      if (t.t.local == sim_address(node, kPort)) return t.t;
    return w.servers.front().t;
  };
  for (int64_t now = 0; now <= 1000000; now += 5000) {
    sim_advance(w.net, now);
    for (size_t i = 0; i < b.size(); i++) master_beat_update(b[i], server_of(beats[i].node), now);
    master_update(master, now);
  }
  CHECK(master_count(master, game, kAllKinds) == 4);
  open_sim_transport(w.bt, w.net, sim_address(1, 27001));
  ServerListConfig cfg;
  cfg.masters = {sim_address(50, 27950)};
  cfg.own_masters = {sim_address(60, kMasterPort)};
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 1010000);
  for (int64_t now = 1015000; now <= 4000000; now += 5000) {
    master_update(master, now);
    w.run(now, now);
  }
  CHECK(server_list_done(w.list, 4000000));
  std::vector<int> rows;
  server_list_view(w.list, ServerFilter{}, ServerSort::name, rows);
  CHECK(names(w.list, rows) == (std::vector<std::string>{"both", "ours one", "ours two"}));
  CHECK(w.list.stats.own_addresses == 3);
  for (const ServerEntry& e : w.list.entries) {
    CHECK(e.from == kServerFromMaster);
    if (e.info.name == "ours one") CHECK(e.ping_ms >= 40 && e.ping_ms <= 46);
  }
}

TEST(server_list_info_filter_columns_and_live_update) {
  // The info filter's words: the first that decides, a lacking key passed over.
  std::vector<std::string> problems;
  const auto rules = parse_info_filter("+map=^dm6$ \"+map=e1m2\" bad -x=( +", &problems);
  CHECK_MSG(rules.size() == 2 && problems.size() == 3, std::to_string(problems.size()));
  using Keys = std::vector<std::pair<std::string, std::string>>;
  CHECK(info_filter_passes(rules, Keys{{"map", "DM6"}}));
  CHECK(info_filter_passes(rules, Keys{{"map", "xe1m2x"}}));
  CHECK(!info_filter_passes(rules, Keys{{"map", "dm66"}}));
  CHECK(!info_filter_passes(rules, Keys{}));  // under + words, a server lacking the key: hidden
  const auto hide = parse_info_filter("-*gamedir=fortress", nullptr);
  CHECK(!info_filter_passes(hide, Keys{{"*gamedir", "Fortress"}}));
  CHECK(info_filter_passes(hide, Keys{}));  // under a - word: shown
  CHECK(info_filter_passes(parse_info_filter("-ktx +*", nullptr), Keys{{"ktx", ""}}));
  CHECK(!info_filter_passes(parse_info_filter("-ktx +*", nullptr), Keys{{"ktx", "1.4"}}));
  CHECK(info_filter_passes(parse_info_filter("+map=*dm", nullptr), Keys{{"map", "e1m1"}}));

  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 6000000);
  std::vector<int> rows;
  ServerFilter f;
  f.info = parse_info_filter("+map=halls", nullptr);
  server_list_view(w.list, f, ServerSort::name, rows);
  CHECK((names(w.list, rows) == std::vector<std::string>{"named", "near ffa"}));
  f = ServerFilter{};
  f.hide_not_empty = true;
  server_list_view(w.list, f, ServerSort::name, rows);
  CHECK((names(w.list, rows) == std::vector<std::string>{"empty coop"}));
  // By columns: map, then players high to low; none: the busy first, the full next, the empty last.
  server_list_view(w.list, ServerFilter{},
                   {{ServerColumn::map, false, {}}, {ServerColumn::players, true, {}}}, rows);
  CHECK(names(w.list, rows).front() == "far ffa");  // arena, 8 players
  server_list_view(w.list, ServerFilter{}, std::vector<ServerSortKey>{}, rows);
  const std::vector<std::string> busy = names(w.list, rows);
  CHECK_MSG(busy.front() == "lan duel" && busy.back() == "empty coop", busy.front());
  // The live update: one server asked again, its row kept meanwhile.
  const int near = rows[1];
  const uint64_t queries = w.list.stats.queries;
  server_list_requery(w.list, near);
  w.run(5000, 7000000);
  CHECK(w.list.stats.queries == queries + 1 && w.list.entries[static_cast<size_t>(near)].answered);
}

TEST(the_list_kept_as_text_reads_back_and_a_master_s_list_is_kept) {
  World w;
  ServerListConfig cfg;
  build(w, cfg);
  server_list_init(w.list, cfg, toy(), w.bt);
  server_list_refresh(w.list, 0);
  w.run(5000, 6000000);
  const std::string text = server_list_cache_text(w.list);
  ServerList kept;
  server_list_init(kept, cfg, toy(), w.bt);
  int bad = -1;
  const int n = server_list_read_cache(kept, text + "not an address\nsim:1:2\tx\n", &bad);
  CHECK(n == server_list_answered(w.list) && bad == 2);
  CHECK(server_list_answered(kept) == n);
  for (const ServerEntry& e : kept.entries) {
    const ServerEntry* was = nullptr;
    for (const ServerEntry& o : w.list.entries)
      if (o.address == e.address) was = &o;
    CHECK(was != nullptr && was->info.name == e.info.name && was->ping_ms == e.ping_ms &&
          was->info.players == e.info.players && e.from == kServerFromCache);
  }
  // A master's answer is kept on the list for its cache, and its lines read back.
  bool listed = false;
  for (const ServerList::Master& m : w.list.masters)
    if (!m.got.empty()) {
      listed = true;
      CHECK(parse_address_lines(address_lines(m.got) + "\nrubbish\n") == m.got);
    }
  CHECK(listed || w.list.config.masters.empty());
  // Addresses alone read as servers not yet answered.
  ServerList bare;
  server_list_init(bare, cfg, toy(), w.bt);
  CHECK(server_list_read_cache(bare, "sim:9:27500\n", nullptr) == 1 && !bare.entries[0].answered);
}
