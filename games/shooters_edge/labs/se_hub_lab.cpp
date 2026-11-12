// LAB `se-hub`: SHOOTER'S EDGE'S HUB LISTING THE CLASSIC SERVERS ALREADY OUT THERE (docs/hub.md §
// The classic adapter, engine/hub/browse.hpp). Five of the game's own dedicated servers on the
// classic protocol (net/classic_game.hpp on the engine's server core) run on the simulated network,
// classic clients on four of them; none of them speaks the engine's status record or knows the
// engine's master. A stand-in classic master lists four of them and one that is not running; the
// fifth is named by hand. The hub reads them the way the game's server browser does (the classic
// status query, `classic_query()`), and its index fills:
//
//   listed     every running server in the index with its record: name, map, mode, slots, the
//              players with their teams and the team game's totals, the match's phase
//   silent     the master's dead entry never listed; the engine's status ask sent to nobody
//   gone       one server stops answering: dropped once no refresh has heard it for `drop_us`
//
// and the Servers and Players pages are written from that index (attached, hashed) and, when a
// headless Chromium is on the machine, drawn at a desktop's and a phone's width (measured).
//
// ge:summary Lab: Shooter's Edge's hub lists running classic servers read by the classic query
// ge:keywords lab se-hub hub browse classic adapter master status-query index pages chromium
// ge:skill net labs
// ge:test tests/hub_browse_test.cpp
#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hub/api.hpp"
#include "hub/browse.hpp"
#include "hub/pages.hpp"
#include "hub/poller.hpp"
#include "hub/spec.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "net/packet.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace gh = ge::hub;
namespace se = shooters_edge::net;
namespace fs = std::filesystem;

constexpr uint16_t kNetPort = 27510, kMasterPort = 27000;
constexpr uint32_t kHub = 1, kMaster = 60, kSilent = 25;

struct Plan {
  const char* name;
  int teamplay;
  int maxclients;
  int clients;
  double latency_ms;
  uint32_t node;
  bool listed;  // by the master; else named by hand
};

constexpr Plan kPlans[] = {
    {"Friday night 4on4", 2, 8, 6, 12, 20, true},   {"Duel corner", 0, 2, 2, 22, 21, true},
    {"Newcomers welcome", 0, 16, 3, 38, 22, true},  {"Late night slipgate", 0, 12, 0, 64, 23, true},
    {"Home of the brave", 1, 10, 1, 15, 30, false},
};
constexpr int kRunning = sizeof kPlans / sizeof kPlans[0];

struct Box {
  std::unique_ptr<gn::Transport> port;
  gs::Server server;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  std::vector<std::unique_ptr<se::ClassicClientEnd>> ends;
  bool running = true;
};

gn::Address at(uint32_t node) { return gn::sim_address(node, kNetPort); }

// The address the classic master hands out for `node` (its IPv4 form), as the hub keys it.
gn::Address listed_at(uint32_t node) {
  uint8_t ip[4];
  gn::sim_to_ipv4(at(node), ip);
  return gn::ipv4_or_sim(ip, kNetPort);
}

struct Evening {
  gn::SimNetwork net = gn::make_sim_network(11, gn::LinkConditions{});
  std::vector<std::unique_ptr<Box>> boxes;
  gn::Transport master, hub;
  std::string error;
  int64_t now_us = 0;

  bool open() {
    gn::open_sim_transport(master, net, gn::sim_address(kMaster, kMasterPort));
    gn::open_sim_transport(hub, net, gn::sim_address(kHub, 27001));
    for (const Plan& p : kPlans) {
      gn::LinkConditions lc;
      lc.latency_ms = p.latency_ms;
      gn::sim_set_path(net, kHub, p.node, lc);
      auto b = std::make_unique<Box>();
      b->port = std::make_unique<gn::Transport>();
      gn::open_sim_transport(*b->port, net, at(p.node));
      gs::ServerConfig c;
      c.hostname = p.name;
      c.map = "arena";
      c.maxclients = p.maxclients;
      c.seed = p.node;
      if (p.teamplay != 0) c.rules["teamplay"] = std::to_string(p.teamplay);
      if (!gs::server_open(b->server, se::classic_game(), c, b->port.get(), nullptr, error)) {
        error = std::string(p.name) + ": " + error;
        return false;
      }
      for (int i = 0; i < p.clients; i++) {
        const uint32_t node = p.node * 10 + static_cast<uint32_t>(i) + 100;
        b->ct.push_back(std::make_unique<gn::Transport>());
        gn::open_sim_transport(*b->ct.back(), net, gn::sim_address(node, 27001));
        b->ends.push_back(std::make_unique<se::ClassicClientEnd>());
        se::ClientConfig cc;
        static constexpr const char* kNames[] = {"Vex", "Moss", "Rook", "Tally", "Juno", "Brisk"};
        cc.name = kNames[static_cast<size_t>(i) % 6];
        cc.files = {"skins/base.pcx"};
        cc.seed = static_cast<int>(node);
        cc.qport = static_cast<uint16_t>(node);
        if (p.teamplay != 0) cc.team = i % 2 ? "blue" : "red";
        se::client_start(*b->ends.back(), *b->ct.back(), at(p.node), cc, 0);
      }
      boxes.push_back(std::move(b));
    }
    return true;
  }

  // The stand-in classic master: `c` asked, the listed servers answered (the silent one too).
  void master_frame() {
    uint8_t buf[256];
    gn::Address from;
    while (gn::transport_receive(master, buf, sizeof buf, from, now_us) >= 0) {
      if (buf[0] != 'c') continue;
      std::vector<uint8_t> r = {0xff, 0xff, 0xff, 0xff, 'd', '\n'};
      auto add = [&](uint32_t node) {
        uint8_t ip[4];
        gn::sim_to_ipv4(at(node), ip);
        r.insert(r.end(), ip, ip + 4);
        r.push_back(static_cast<uint8_t>(kNetPort >> 8));
        r.push_back(static_cast<uint8_t>(kNetPort & 0xff));
      };
      for (const Plan& p : kPlans) {
        if (p.listed) add(p.node);
      }
      add(kSilent);
      gn::transport_send(master, from, r.data(), r.size(), now_us);
    }
  }

  void frame() {
    gn::sim_advance(net, now_us);
    master_frame();
    for (auto& b : boxes) {
      if (!b->running) continue;
      for (auto& e : b->ends) se::client_tick(*e, now_us);
      gs::server_frame(b->server);
    }
    now_us = boxes.front()->server.now_us;
  }
};

gh::HubSpec spec() {
  gh::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.protocol = gn::protocol_id("shooters_edge", 1);
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"team", 6}, {"ffa", 3}};
  s.score = "frags";
  s.browse = se::classic_query();
  return s;
}

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

void run(gl::Lab& lab) {
  Evening ev;
  if (!ev.open()) {
    lab.fail("a server did not open: " + ev.error,
             "run the se-browser lab, which opens the same servers, and read its failure");
    return;
  }
  const gh::HubSpec hs = spec();
  gh::Poller poller;
  gh::poller_init(poller, gh::PollerConfig{}, 0);  // no engine master: the classic ones alone
  gh::Browse browse;
  gh::BrowseConfig bc;
  bc.masters = {gn::sim_address(kMaster, kMasterPort)};
  bc.servers = {at(30)};
  bc.refresh_us = 5000000;
  bc.drop_us = 12000000;
  // The clients join first, so the status answers carry players.
  const int64_t warm_us = static_cast<int64_t>(std::stod(lab.arg("warm")) * 1e6);
  while (ev.now_us < warm_us) ev.frame();
  gh::browse_init(browse, bc, hs.browse, ev.hub, ev.now_us);
  const int64_t opened = ev.now_us;
  int64_t full_us = -1;
  while (ev.now_us < opened + 4500000) {
    ev.frame();
    gh::browse_update(browse, poller, ev.now_us);
    gh::poller_update(poller, ev.hub, ev.now_us);
    int have = 0;
    for (const gh::HubNode& n : poller.nodes) have += n.have ? 1 : 0;
    if (full_us < 0 && have == kRunning) full_us = ev.now_us - opened;
  }

  // LISTED: what the index holds.
  int players = 0, joined = 0, browsed = 0;
  for (const Plan& p : kPlans) joined += p.clients;
  gl::LabTable t{"the hub's index after the first refresh, by the interest rule",
                 {"name", "mode", "map", "players", "teams", "phase", "address"},
                 {}};
  std::vector<const gh::HubNode*> order;
  order.reserve(poller.nodes.size());
  for (const gh::HubNode& n : poller.nodes) order.push_back(&n);
  std::stable_sort(order.begin(), order.end(), [&](const gh::HubNode* a, const gh::HubNode* b) {
    return gh::interest(hs, *a, ev.now_us) > gh::interest(hs, *b, ev.now_us);
  });
  for (const gh::HubNode* n : order) {
    browsed += n->browsed ? 1 : 0;
    players += static_cast<int>(n->status.players.size());
    std::string teams;
    for (const gn::StatusTeam& tm : n->status.teams) {
      teams += (teams.empty() ? "" : " ") + tm.name + " " + std::to_string(tm.score);
    }
    t.rows.push_back(
        {n->status.name, n->status.mode, n->status.map,
         std::to_string(n->status.players.size()) + "/" + std::to_string(n->status.max_players),
         teams.empty() ? "-" : teams, gn::status_phase_name(n->status.phase),
         gn::to_string(n->address)});
  }
  lab.table(std::move(t));
  lab.metric("servers.listed", static_cast<double>(poller.nodes.size()), "of 5");
  lab.metric("servers.browsed", browsed, "of 5");
  lab.metric("players.listed", players, "players");
  lab.metric("players.joined", joined, "players");
  lab.metric("first_full_ms", static_cast<double>(full_us) / 1000.0, "ms");
  lab.metric("browse.queries", static_cast<double>(browse.list.stats.queries), "datagrams");
  if (static_cast<int>(poller.nodes.size()) != kRunning || full_us < 0) {
    lab.fail("the hub listed " + std::to_string(poller.nodes.size()) + " of the " +
                 std::to_string(kRunning) + " running classic servers",
             "run tests/hub_browse_test and the se-browser lab: the server list or the merge "
             "missed one");
  }
  if (players != joined) {
    lab.fail("the index holds " + std::to_string(players) + " players; " + std::to_string(joined) +
                 " joined",
             "check status_of (engine/hub/browse.cpp) against classic_read_status's roster");
  }
  const gh::HubNode* team = gh::poller_find(poller, listed_at(20));
  lab.metric("team_game.teams", team ? static_cast<double>(team->status.teams.size()) : 0, "teams");
  if (!team || team->status.teams.size() != 2) {
    lab.fail("the 4on4's record has no two teams", "check status_of's team totals");
  }
  // SILENT: nobody asked in the engine's protocol; the dead entry never listed.
  lab.metric("engine_asks", static_cast<double>(poller.stats.asks), "datagrams");
  if (poller.stats.asks != 0) {
    lab.fail("the poller sent the engine's status ask to a browsed server",
             "skip browsed nodes in poller_update (engine/hub/poller.cpp)");
  }
  if (gh::poller_find(poller, listed_at(kSilent))) {
    lab.fail("the master's dead entry reached the index", "merge only answered entries");
  }

  // The pages from that index.
  const std::string servers = gh::servers_page(poller, hs, "", ev.now_us);
  const std::string players_html = gh::players_page(poller, hs, ev.now_us);
  const gh::Answer api = gh::api_answer(poller, hs, "/v1/servers", ev.now_us);
  lab.attach("servers.html", bytes(servers));
  lab.attach("players.html", bytes(players_html));
  lab.attach("servers.json", bytes(api.body));

  // GONE: the slipgate server stops; the next refreshes miss it, and it is dropped.
  ev.boxes[3]->running = false;
  const int64_t stopped = ev.now_us;
  int64_t gone_us = -1;
  while (ev.now_us < stopped + 22000000 && gone_us < 0) {
    ev.frame();
    gh::browse_update(browse, poller, ev.now_us);
    if (!gh::poller_find(poller, listed_at(23))) gone_us = ev.now_us - stopped;
  }
  lab.metric("gone.dropped_after_s", static_cast<double>(gone_us) / 1e6, "s");
  lab.metric("gone.left", static_cast<double>(poller.nodes.size()), "servers");
  if (gone_us < 0 || poller.nodes.size() != kRunning - 1) {
    lab.fail("the stopped server stayed in the index",
             "browse_update drops a browsed node no refresh heard for drop_us");
  }

  const std::string browser = lab.arg("browser") == "0" ? "" : gl::hub_browser::find_browser();
  if (browser.empty()) {
    lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium): pages not drawn");
    return;
  }
  std::error_code ec;
  const fs::path dir = fs::temp_directory_path(ec) / "ge-se-hub";
  fs::create_directories(dir, ec);
  struct View {
    const char* id;
    const char* title;
    const std::string* page;
    int w, h;
    const char* what;
    const char* look;
  };
  const View views[] = {
      {"servers-desktop", "THE CLASSIC SERVERS ON THE HUB", &servers, 1280, 760,
       "GET / of Shooter's Edge's hub fed only by a classic master and one named server: each "
       "running classic server a card, read by the classic status query.",
       "the full duel first (a full match of people leads), the 4on4 with its red and blue "
       "totals, the public game and the named server after; the empty slipgate server below."},
      {"servers-phone", "THE SAME ON A PHONE", &servers, 390, 1900,
       "The same page at a phone's width.", "one card a row, nothing wider than the screen."},
      {"players-desktop", "EVERY CLASSIC PLAYER", &players_html, 1280, 760,
       "GET /players: the twelve who joined, by their server's interest.",
       "names with their teams and frags, the server each is on."},
  };
  int drawn = 0;
  for (const View& v : views) {
    const std::optional<gl::Picture> pic =
        gl::hub_browser::hub_draw(browser, dir, v.id, *v.page, v.w, v.h, false);
    if (!pic) {
      lab.note(std::string("Chromium did not draw ") + v.id);
      continue;
    }
    drawn++;
    lab.shot({v.id, v.title, v.what, v.look, {{"width", std::to_string(v.w)}}}, *pic,
             gl::Det::measured);
  }
  lab.metric("pages_drawn", drawn, "of 3", gl::Det::measured);
  fs::remove_all(dir, ec);
}

const gl::LabSpec kSpec{
    "se-hub",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "engine/hub/browse (the classic adapter) with games/shooters_edge/net/classic_browse",
    "net",
    "Does Shooter's Edge's hub list the classic servers already running, found by a classic master "
    "and by name and read by the classic status query, with their players and teams, and drop one "
    "that stops?",
    {
        {"warm", "2", "seconds the servers run, and the clients join, before the hub opens"},
        {"browser", "1", "draw the pages with a headless Chromium when one is found (0: never)"},
    },
    "--browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
