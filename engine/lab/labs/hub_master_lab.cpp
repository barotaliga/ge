// LAB `hub-master`: an evening of public servers on the simulated network as a hub sees it
// (docs/hub.md). Dedicated servers of the sample game (engine/server) beat to one master
// (net/master.hpp) through `sv_master`, beside broadcast (TV) nodes and a forwarder beating as
// their own kinds; a hub asks the master for the list every second and each listed node for its
// status (net/status.hpp) every five. On the way one server stops (it says bye), one falls silent
// (it does not), and new ones open. It answers whether the list follows them (a bye at once, the
// silent one after the master's expiry, the new ones within a beat), whether every listed node's
// status reaches the hub, and what it costs in datagrams. The list's size by kind is charted over
// the evening, and the hub's last view drawn as its Servers page would list it. Everything runs on
// a virtual clock off one seed.
//
// ge:summary Lab: servers, TV nodes and a forwarder beating to a master; the hub's list and status
// ge:keywords lab hub master heartbeat beat bye expire status list tv forward server-list
// ge:keywords sim-network evening digest picture
// ge:skill net labs
// ge:test tests/net_master_test.cpp
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/master.hpp"
#include "net/status.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace gu = ge::ui;

constexpr uint16_t kMasterPort = 27900;
constexpr uint32_t kMasterNode = 1, kHubNode = 2;
const gn::Address kMaster = gn::sim_address(kMasterNode, kMasterPort);
constexpr int64_t kTick = 1000000 / 60;

struct GameNode {
  std::unique_ptr<gn::Transport> port = std::make_unique<gn::Transport>();
  std::unique_ptr<gs::Server> server = std::make_unique<gs::Server>();
  bool open = false, stepping = false;
  int64_t clock_offset_us = 0;  // the server's own clock starts at 0 when it opens
};

// A broadcast node or a forwarder: a socket that beats and answers status asks.
struct OtherNode {
  gn::Transport port;
  gn::MasterBeat beat;
  gn::Status status;
};

struct Seen {
  gn::Address address;
  gn::NodeKind kind = gn::NodeKind::game;
  uint64_t cookie = 0;
  bool have = false;
  gn::Status status;
};

struct Hub {
  gn::Transport port;
  gn::MasterQuery query;
  bool querying = false;
  std::vector<gn::MasterEntry> list;  // the last whole answer
  std::vector<Seen> seen;
  uint64_t asks = 0, records = 0;
};

Seen& seen_of(Hub& h, const gn::MasterEntry& e) {
  for (Seen& s : h.seen) {
    if (s.address == e.address && s.kind == e.kind) return s;
  }
  h.seen.push_back({e.address, e.kind, 0, false, {}});
  return h.seen.back();
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

uint64_t fold(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }

// THE HUB'S SERVERS PAGE: every listed node with its status, as the hub would list it.
gl::Picture page_picture(const Hub& hub, double at_s) {
  constexpr int kW = 1280;
  const int rows = static_cast<int>(hub.list.size());
  const int kH = 200 + 40 * (rows < 30 ? rows : 30);
  gu::Image img = gu::make_image(kW, kH, {0.06f, 0.08f, 0.07f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.92f, 0.96f, 0.94f, 1}, dim{0.55f, 0.66f, 0.62f, 1};
  const gu::Color game{0.25f, 0.78f, 0.60f, 1}, tv{1.0f, 0.70f, 0.30f, 1}, fwd{0.55f, 0.70f, 1, 1};
  const float x0 = 32, y0 = 32;
  dl.rect({x0, y0, kW - 64, static_cast<float>(kH) - 64}, {0.10f, 0.13f, 0.12f, 1}, 14);
  int games = 0, tvs = 0, players = 0;
  for (const gn::MasterEntry& e : hub.list) {
    games += e.kind == gn::NodeKind::game ? 1 : 0;
    tvs += e.kind == gn::NodeKind::tv ? 1 : 0;
  }
  for (const Seen& s : hub.seen)
    players +=
        s.have && s.kind == gn::NodeKind::game ? static_cast<int>(s.status.players.size()) : 0;
  char head[160];
  std::snprintf(head, sizeof head, "%d servers  ·  %d broadcasts  ·  %d players  ·  at %.0f s",
                games, tvs, players, at_s);
  dl.text(bold, 30, x0 + 24, y0 + 46, "SERVERS", ink);
  dl.text(body, 20, x0 + 24, y0 + 76, head, dim);
  const float cols[6] = {x0 + 24, x0 + 130, x0 + 430, x0 + 600, x0 + 760, x0 + 930};
  const char* heads[6] = {"KIND", "NAME", "MODE", "PLAYERS", "PHASE", "ADDRESS"};
  for (int c = 0; c < 6; c++) dl.text(bold, 16, cols[c], y0 + 118, heads[c], dim);
  float y = y0 + 152;
  // Games first, then TV nodes, then forwarders; each kind in address order (a sim address's
  // node id is its first four bytes, little endian).
  std::vector<gn::MasterEntry> order = hub.list;
  const auto key = [](const gn::MasterEntry& e) {
    const uint32_t id = e.address.bytes[0] | e.address.bytes[1] << 8 | e.address.bytes[2] << 16 |
                        static_cast<uint32_t>(e.address.bytes[3]) << 24;
    return (static_cast<uint64_t>(e.kind) << 48) | (static_cast<uint64_t>(id) << 16) |
           e.address.port;
  };
  std::sort(order.begin(), order.end(),
            [&](const gn::MasterEntry& a, const gn::MasterEntry& b) { return key(a) < key(b); });
  int drawn = 0;
  for (const gn::MasterEntry& e : order) {
    if (drawn++ == 30) break;
    const Seen* s = nullptr;
    for (const Seen& o : hub.seen) {
      if (o.address == e.address && o.kind == e.kind) s = &o;
    }
    if (drawn % 2 == 0) dl.rect({x0 + 16, y - 26, kW - 96, 40}, {0.13f, 0.17f, 0.15f, 1}, 6);
    const gu::Color kc = e.kind == gn::NodeKind::game ? game
                         : e.kind == gn::NodeKind::tv ? tv
                                                      : fwd;
    dl.text(bold, 18, cols[0], y, gn::node_kind_name(e.kind), kc);
    if (s && s->have) {
      dl.text(body, 18, cols[1], y, s->status.name, ink);
      dl.text(body, 18, cols[2], y, s->status.mode.empty() ? "-" : s->status.mode, ink);
      const std::string seats =
          std::to_string(s->status.players.size()) + " / " + std::to_string(s->status.max_players);
      dl.text(body, 18, cols[3], y, e.kind == gn::NodeKind::game ? seats : "-", ink);
      dl.text(body, 18, cols[4], y, gn::status_phase_name(s->status.phase), dim);
    } else {
      dl.text(body, 18, cols[1], y, "(no status yet)", dim);
    }
    dl.text(body, 18, cols[5], y, gn::to_string(e.address), dim);
    y += 40;
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int servers = static_cast<int>(lab.arg_num("servers"));
  const int tvs = static_cast<int>(lab.arg_num("tvs"));
  const double seconds = lab.arg_num("seconds");
  gn::LinkConditions link;
  if (!gn::conditions_preset(lab.arg("preset"), link)) {
    lab.fail("unknown preset '" + lab.arg("preset") + "'",
             "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
             "skill: net.");
    return;
  }
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  const gs::ServerGame& game = gs::sample_game();

  gn::Transport master_port;
  gn::open_sim_transport(master_port, net, kMaster);
  gn::Master master;
  gn::master_init(master, gn::MasterConfig{}, master_port);

  // The evening's events: one server stops at a third, one falls silent at 40%, a few open at
  // half time.
  const int extra = servers / 8 > 0 ? servers / 8 : 1;
  const int stopper = 2, silent = 4;
  const double stop_at = seconds / 3, silent_at = seconds * 0.4, open_at = seconds / 2;
  std::vector<GameNode> nodes(static_cast<size_t>(servers + extra));
  auto open = [&](int i, int64_t now) {
    GameNode& g = nodes[static_cast<size_t>(i)];
    gn::open_sim_transport(*g.port, net, gn::sim_address(100 + static_cast<uint32_t>(i), 27600));
    gs::ServerConfig c;
    c.hostname = "Ring " + std::to_string(i + 1);
    c.map = "ring";
    c.bots = 1 + i % 7;
    c.maxclients = 8;
    c.seed = seed * 131u + static_cast<uint32_t>(i);
    c.masters = "sim:" + std::to_string(kMasterNode) + ":" + std::to_string(kMasterPort);
    std::string error;
    if (!gs::server_open(*g.server, game, c, g.port.get(), nullptr, error)) {
      lab.fail("server " + std::to_string(i + 1) + " did not open: " + error, "skill: net.");
      return;
    }
    g.open = g.stepping = true;
    g.clock_offset_us = now;
  };
  for (int i = 0; i < servers; i++) open(i, 0);

  std::vector<std::unique_ptr<OtherNode>> others;
  for (int i = 0; i <= tvs; i++) {
    auto o = std::make_unique<OtherNode>();
    const bool forward = i == tvs;
    const uint16_t port = forward ? 30000 : 28000;
    gn::open_sim_transport(o->port, net, gn::sim_address(300 + static_cast<uint32_t>(i), port));
    gn::MasterBeatConfig bc;
    bc.masters = {kMaster};
    bc.protocol = game.protocol;
    bc.kind = forward ? gn::NodeKind::forward : gn::NodeKind::tv;
    bc.version = 1;
    bc.seed = seed * 977u + static_cast<uint32_t>(i);
    gn::master_beat_init(o->beat, bc);
    o->status.kind = bc.kind;
    o->status.protocol = game.protocol;
    o->status.name = forward ? "Forward" : "TV " + std::to_string(i + 1);
    if (!forward) {
      for (int k = 0; k < 2; k++) {
        gn::StatusStream st;
        st.node = o->port.local;
        st.stream = static_cast<uint32_t>(k + 1);
        st.delay_s = 10;
        st.viewers = static_cast<uint16_t>(3 * i + k);
        o->status.streams.push_back(st);
      }
    }
    others.push_back(std::move(o));
  }

  Hub hub;
  gn::open_sim_transport(hub.port, net, gn::sim_address(kHubNode, 40000));

  gl::Trace t_game{"game servers", {}, {}}, t_tv{"tv nodes", {}, {}}, t_fwd{"forwarders", {}, {}};
  gl::Trace t_status{"statuses at the hub", {}, {}};
  double gone_bye_s = -1, gone_silent_s = -1, new_listed_s = -1;
  uint64_t digest = 1469598103934665603ull;
  const int64_t end_us = static_cast<int64_t>(seconds * 1e6);
  uint8_t buf[gn::kMaxDatagram], out[gn::kMaxDatagram];
  for (int64_t now = 0; now <= end_us; now += kTick) {
    gn::sim_advance(net, now);
    gn::master_update(master, now);
    // The evening's events.
    if (now >= static_cast<int64_t>(stop_at * 1e6) && nodes[stopper].open) {
      gs::server_close(*nodes[stopper].server);
      nodes[stopper].open = nodes[stopper].stepping = false;
    }
    if (now >= static_cast<int64_t>(silent_at * 1e6) && nodes[silent].stepping) {
      nodes[silent].stepping = false;  // the process hangs: no frame, no beat, no bye
    }
    if (now >= static_cast<int64_t>(open_at * 1e6) && !nodes[static_cast<size_t>(servers)].open) {
      for (int i = servers; i < servers + extra; i++) open(i, now);
    }
    for (GameNode& g : nodes) {
      if (!g.stepping) continue;
      while (g.clock_offset_us + gs::server_next_us(*g.server) <= now) gs::server_frame(*g.server);
    }
    for (auto& o : others) {
      gn::master_beat_update(o->beat, o->port, now);
      gn::Address from;
      int n = 0;
      while ((n = gn::transport_receive(o->port, buf, sizeof buf, from, now)) >= 0) {
        if (gn::master_beat_read(o->beat, buf, static_cast<size_t>(n), from, now)) continue;
        const size_t a = gn::status_answer(buf, static_cast<size_t>(n), from, game.protocol,
                                           0x5eed ^ gn::address_hash(o->port.local), now, o->status,
                                           out, sizeof out);
        if (a) gn::transport_send(o->port, from, out, a, now);
      }
    }
    // The hub: the list every second, every listed node's status every five.
    const bool second = now % 1000000 < kTick;
    if (second && !hub.querying) {
      gn::MasterQueryConfig qc;
      qc.masters = {kMaster};
      qc.protocol = game.protocol;
      qc.seed = static_cast<uint64_t>(now) + seed;
      gn::master_query_start(hub.query, qc, now);
      hub.querying = true;
    }
    if (hub.querying) gn::master_query_update(hub.query, hub.port, now);
    if (now % 5000000 < kTick) {
      for (const gn::MasterEntry& e : hub.list) {
        Seen& s = seen_of(hub, e);
        const size_t n = gn::status_ask(game.protocol, gn::address_hash(e.address), s.cookie, out);
        gn::transport_send(hub.port, e.address, out, n, now);
        hub.asks++;
      }
    }
    gn::Address from;
    int n = 0;
    while ((n = gn::transport_receive(hub.port, buf, sizeof buf, from, now)) >= 0) {
      if (gn::master_query_read(hub.query, buf, static_cast<size_t>(n), from, now)) continue;
      for (Seen& s : hub.seen) {
        if (s.address != from) continue;
        gn::Status st;
        const gn::StatusReply r = gn::status_read_reply(buf, static_cast<size_t>(n),
                                                        gn::address_hash(s.address), s.cookie, st);
        if (r == gn::StatusReply::cookie) {
          // Asked again at once with the cookie.
          const size_t k =
              gn::status_ask(game.protocol, gn::address_hash(s.address), s.cookie, out);
          gn::transport_send(hub.port, s.address, out, k, now);
          hub.asks++;
        } else if (r == gn::StatusReply::status) {
          s.status = std::move(st);
          s.have = true;
          hub.records++;
        }
      }
    }
    if (hub.querying && hub.query.done) {
      hub.list = hub.query.found;
      hub.querying = false;
    }
    if (second) {
      const double t = static_cast<double>(now) / 1e6;
      int count[3] = {0, 0, 0}, have = 0;
      bool bye_listed = false, silent_listed = false, new_listed = false;
      for (const gn::MasterEntry& e : hub.list) {
        count[static_cast<int>(e.kind)]++;
        for (const Seen& s : hub.seen) {
          if (s.address == e.address && s.kind == e.kind && s.have) have++;
        }
        if (e.address == nodes[stopper].port->local) bye_listed = true;
        if (e.address == nodes[silent].port->local) silent_listed = true;
        if (e.address == nodes[static_cast<size_t>(servers)].port->local) new_listed = true;
      }
      if (t > stop_at && !bye_listed && gone_bye_s < 0) gone_bye_s = t - stop_at;
      if (t > silent_at && !silent_listed && gone_silent_s < 0) gone_silent_s = t - silent_at;
      if (t >= open_at && new_listed && new_listed_s < 0) new_listed_s = t - open_at;
      t_game.x.push_back(t);
      t_game.y.push_back(count[0]);
      t_tv.x.push_back(t);
      t_tv.y.push_back(count[1]);
      t_fwd.x.push_back(t);
      t_fwd.y.push_back(count[2]);
      t_status.x.push_back(t);
      t_status.y.push_back(have);
      digest =
          fold(fold(fold(digest, static_cast<uint64_t>(count[0])), static_cast<uint64_t>(count[1])),
               static_cast<uint64_t>(have));
    }
  }

  const int want_games = servers - 2 + extra;
  int listed_games = 0, listed_tv = 0, listed_fwd = 0, with_status = 0, players = 0;
  for (const gn::MasterEntry& e : hub.list) {
    listed_games += e.kind == gn::NodeKind::game ? 1 : 0;
    listed_tv += e.kind == gn::NodeKind::tv ? 1 : 0;
    listed_fwd += e.kind == gn::NodeKind::forward ? 1 : 0;
    for (const Seen& s : hub.seen) {
      if (s.address == e.address && s.kind == e.kind && s.have) {
        with_status++;
        if (e.kind == gn::NodeKind::game) players += static_cast<int>(s.status.players.size());
      }
    }
  }
  lab.metric("listed_games", listed_games, "servers");
  lab.metric("listed_tv", listed_tv, "nodes");
  lab.metric("listed_forward", listed_fwd, "nodes");
  lab.metric("with_status", with_status, "nodes");
  lab.metric("players_seen", players, "players");
  lab.metric("bye_gone_after", gone_bye_s, "s");
  lab.metric("silent_gone_after", gone_silent_s, "s");
  lab.metric("new_listed_after", new_listed_s, "s");
  lab.metric("master_beats", static_cast<double>(master.stats.beats), "datagrams");
  lab.metric("master_pages", static_cast<double>(master.stats.pages), "datagrams");
  lab.metric("hub_status_asks", static_cast<double>(hub.asks), "datagrams");

  gl::LabTable events{"the evening's events and when the hub's list followed",
                      {"event", "at (s)", "the hub's list followed after (s)", "expected"},
                      {}};
  char a[32], b[32];
  std::snprintf(a, sizeof a, "%.1f", stop_at);
  std::snprintf(b, sizeof b, "%.0f", gone_bye_s);
  events.rows.push_back({"Ring 3 stops and says bye", a, b, "within a second or two"});
  std::snprintf(a, sizeof a, "%.1f", silent_at);
  std::snprintf(b, sizeof b, "%.0f", gone_silent_s);
  events.rows.push_back(
      {"Ring 5 hangs: no beat, no bye", a, b,
       "the master's expiry: 100 s after its last beat, which was up to 30 s before"});
  std::snprintf(a, sizeof a, "%.1f", open_at);
  std::snprintf(b, sizeof b, "%.0f", new_listed_s);
  events.rows.push_back({std::to_string(extra) + " new servers open", a, b, "within a second"});
  lab.table(std::move(events));
  gl::LabTable totals{"the hub's last list", {"kind", "listed", "expected"}, {}};
  totals.rows.push_back({"game", std::to_string(listed_games),
                         std::to_string(gone_silent_s >= 0 ? want_games : want_games + 1)});
  totals.rows.push_back({"tv", std::to_string(listed_tv), std::to_string(tvs)});
  totals.rows.push_back({"forward", std::to_string(listed_fwd), "1"});
  totals.rows.push_back({"with a status", std::to_string(with_status), "every listed node"});
  totals.rows.push_back({"trace digest", hex(digest), ""});
  lab.table(std::move(totals));

  if (gone_bye_s < 0 || gone_bye_s > 3) {
    lab.fail(
        "the server that said bye stayed on the hub's list (" + std::to_string(gone_bye_s) + " s)",
        "master_beat_stop sends the bye and master_update drops the entry; skill: net.");
  }
  if (new_listed_s < 0 || new_listed_s > 3) {
    lab.fail("a new server took " + std::to_string(new_listed_s) + " s to reach the hub's list",
             "its first beat goes out on its first frame (net/master.hpp); skill: net.");
  }
  if (listed_tv != tvs || listed_fwd != 1) {
    lab.fail("the hub lists " + std::to_string(listed_tv) + " TV nodes and " +
                 std::to_string(listed_fwd) + " forwarders, not " + std::to_string(tvs) + " and 1",
             "every kind beats the same master (net/master.hpp); skill: net.");
  }
  if (with_status != static_cast<int>(hub.list.size())) {
    lab.fail(std::to_string(hub.list.size() - static_cast<size_t>(with_status)) +
                 " listed nodes gave the hub no status",
             "the status ask needs its cookie round (net/status.hpp); skill: net.");
  }

  gl::LineChart line;
  line.frame = {"THE HUB'S LIST OVER THE EVENING, BY KIND",
                "seed " + lab.arg("seed") + ", preset " + lab.arg("preset"), "t (s)", "nodes"};
  line.spikes = false;
  line.traces = {t_game, t_tv, t_fwd, t_status};
  line.marks = {
      {stop_at, "Ring 3 says bye"}, {silent_at, "Ring 5 hangs"}, {open_at, "new servers"}};
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"list",
            chart.title,
            "How many nodes of each kind the hub's list held each second, and how many of them had "
            "answered its status ask. Dedicated servers of the sample game beat to the master "
            "through sv_master; TV nodes and a forwarder beat as their own kinds.",
            "a step down right after the bye; the hung server held until the master's expiry "
            "(100 s after its last beat), then a step down; a step up within a second of the new "
            "servers; the statuses following the list within five seconds.",
            {{"servers", lab.arg("servers")}, {"tvs", lab.arg("tvs")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
  lab.shot({"page",
            "THE HUB'S SERVERS PAGE AT THE END",
            "Every node on the hub's last list with the status it answered: kind, name, mode, "
            "players over slots, phase, address. The page M56c draws, here as a plain list.",
            "every server named Ring N with its walkers as players, the TV nodes and the forwarder "
            "below them, no row without a status.",
            {{"servers", lab.arg("servers")}, {"seed", lab.arg("seed")}}},
           page_picture(hub, seconds));
}

const gl::LabSpec kSpec{
    "hub-master",
    gl::LabKind::sim,
    "engine",
    "engine/net (master, status), engine/server (sv_master, the status answer)",
    "net",
    "Do game servers, TV nodes and forwarders beating to one master reach a hub's list, does the "
    "list follow a server that stops, one that hangs and new ones, and does every listed node's "
    "status reach the hub?",
    {
        {"servers", "24", "dedicated servers of the sample game at the start"},
        {"tvs", "4", "broadcast nodes (one forwarder beside them)"},
        {"seconds", "200", "the evening's length, s"},
        {"preset", "broadband", "the network's conditions (engine/net/conditions.cpp)"},
        {"seed", "1", "the network's and the servers' stream"},
    },
    "--servers=8 --tvs=2 --seconds=160",
    run,
};
GE_LAB(kSpec);

}  // namespace
