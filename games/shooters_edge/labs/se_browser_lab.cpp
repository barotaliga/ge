// LAB `se-browser`: THE SERVER BROWSER (MULTIPLAYER ▸ JOIN A GAME, menu/servers_page.hpp over the
// engine's server list, net/server_list.hpp) finding real servers. Seven of the game's own
// dedicated servers (net/classic_game.hpp on the engine's server core) run on the simulated
// network, each on its own map, rules and link: two on the LAN, four a master lists, one only
// the player names (`sb_servers`); the master also lists one that is not running. Classic clients
// join them first, so the status queries see players. Then the menu opens on the browser
// (`menu_servers`), the way a player opens it, and the same presses walk it:
//
//   refreshing  the first moments: the LAN's answers in, the internet's still coming
//   ping        every server in, sorted by ping; the selection's facts and players at the foot
//   players     RIGHT: sorted by players, the busiest first
//   locked      DOWN to the server that wants a password: its `*`
//   empty       the LAN off and no master named: what the page says
//
// and ENTER on a server runs `connect <its address>` on the console (checked). Every layout is
// held to the page's problems check. With `--basedir` the servers play that install's maps (a
// classic pack's: the pictures stay private), else the game's built-in arena; `--backdrop` draws
// the page over a picture (a frame of the game, as the menu draws over the darkened world).
//
// ge:summary Lab: the server browser listing real servers: LAN, master, named, sorts, join
// ge:keywords lab se-browser server-browser servers status-query master-server lan ping sort
// ge:keywords join connect menu_servers players mode map viewports
// ge:skill net ui labs
// ge:test tests/net_server_list_test.cpp tests/shooters_edge_classic_browse_test.cpp
#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "server/server.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;
namespace sm = shooters_edge::menu;
namespace ui = ge::ui;

constexpr uint16_t kLanPort = 27500, kNetPort = 27510, kMasterPort = 27000;
constexpr uint32_t kBrowser = 1, kMaster = 60;

struct Plan {
  const char* name;
  const char* map;  // with --basedir
  int teamplay;
  int maxclients;
  int clients;  // classic clients that join it
  double latency_ms;
  uint32_t node;
  bool lan, listed, named;
  const char* password;
};

// The evening's servers. The maps are the first episode's, which the shareware pack holds.
constexpr Plan kPlans[] = {
    {"Friday night 4on4", "e1m2", 2, 8, 6, 2, 10, true, false, false, ""},
    {"Office LAN", "e1m1", 0, 8, 1, 1, 11, true, false, false, ""},
    {"Duel corner", "e1m5", 0, 2, 2, 22, 20, false, true, false, ""},
    {"Newcomers welcome", "e1m3", 0, 16, 3, 38, 21, false, true, false, ""},
    {"Late night slipgate", "e1m7", 0, 12, 0, 64, 22, false, true, false, ""},
    {"Clan practice", "e1m4", 2, 8, 2, 30, 23, false, true, false, "letmein"},
    {"Home of the brave", "e1m6", 1, 10, 1, 15, 30, false, false, true, ""},
};
constexpr uint32_t kSilent = 25;  // listed by the master, not running

struct Box {
  std::unique_ptr<gn::Transport> port;
  gs::Server server;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  std::vector<std::unique_ptr<se::ClassicClientEnd>> ends;
};

struct Evening {
  gn::SimNetwork net = gn::make_sim_network(7, gn::LinkConditions{});
  std::vector<std::unique_ptr<Box>> boxes;
  gn::Transport master;
  gn::Transport browser;
  std::string error;
  double frame_s = 1.0 / 77;
  int64_t now_us = 0;

  bool open(const std::string& basedir) {
    gn::open_sim_transport(master, net, gn::sim_address(kMaster, kMasterPort));
    gn::open_sim_transport(browser, net, gn::sim_address(kBrowser, 27001));
    for (const Plan& p : kPlans) {
      gn::LinkConditions lc;
      lc.latency_ms = p.latency_ms;
      gn::sim_set_path(net, kBrowser, p.node, lc);
      auto b = std::make_unique<Box>();
      b->port = std::make_unique<gn::Transport>();
      gn::open_sim_transport(*b->port, net, gn::sim_address(p.node, p.lan ? kLanPort : kNetPort));
      gs::ServerConfig c;
      c.hostname = p.name;
      c.map = basedir.empty() ? "arena" : p.map;
      if (!basedir.empty()) c.root = basedir;
      c.maxclients = p.maxclients;
      c.password = p.password;
      c.seed = p.node;
      if (p.teamplay != 0) c.rules["teamplay"] = std::to_string(p.teamplay);
      if (!gs::server_open(b->server, se::classic_game(), c, b->port.get(), nullptr, error)) {
        error = std::string(p.name) + ": " + error;
        return false;
      }
      frame_s = b->server.frame_s;
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
        cc.password = p.password;
        if (p.teamplay != 0) cc.team = i % 2 ? "blue" : "red";
        if (!basedir.empty()) {
          auto files = std::make_shared<shooters_edge::client::SearchPath>();
          auto mounted = std::make_shared<std::string>();
          cc.read_file = [basedir, files, mounted](const std::string& gamedir,
                                                   const std::string& path) {
            std::vector<uint8_t> store;
            if (*mounted != gamedir) {
              std::string why;
              *files = {};
              if (!shooters_edge::client::open_game_dir(basedir, gamedir, *files, why))
                return store;
              *mounted = gamedir;
            }
            const std::span<const uint8_t> got =
                shooters_edge::client::find_file(*files, path, store);
            return std::vector<uint8_t>(got.begin(), got.end());
          };
        }
        se::client_start(*b->ends.back(), *b->ct.back(),
                         gn::sim_address(p.node, p.lan ? kLanPort : kNetPort), cc, 0);
      }
      boxes.push_back(std::move(b));
    }
    return true;
  }

  // The master: `c` asked, the listed servers' addresses answered (the silent one too).
  void master_frame() {
    uint8_t buf[256];
    gn::Address from;
    while (gn::transport_receive(master, buf, sizeof buf, from, now_us) >= 0) {
      if (buf[0] != 'c') continue;
      std::vector<uint8_t> r = {0xff, 0xff, 0xff, 0xff, 'd', '\n'};
      auto add = [&](uint32_t node) {
        uint8_t ip[4];
        gn::sim_to_ipv4(gn::sim_address(node, kNetPort), ip);
        r.insert(r.end(), ip, ip + 4);
        r.push_back(static_cast<uint8_t>(kNetPort >> 8));
        r.push_back(static_cast<uint8_t>(kNetPort & 0xff));
      };
      for (const Plan& p : kPlans)
        if (p.listed) add(p.node);
      add(kSilent);
      gn::transport_send(master, from, r.data(), r.size(), now_us);
    }
  }

  void frame() {
    gn::sim_advance(net, now_us);
    master_frame();
    for (auto& b : boxes) {
      for (auto& e : b->ends) se::client_tick(*e, now_us);
      gs::server_frame(b->server);
    }
    now_us = boxes.front()->server.now_us;
  }
};

struct Viewport {
  int w, h;
  std::string name() const { return std::to_string(w) + "x" + std::to_string(h); }
};

std::vector<Viewport> parse_viewports(const std::string& text) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find(',', at);
    if (end == std::string::npos) end = text.size();
    const std::string one = text.substr(at, end - at);
    const size_t x = one.find('x');
    if (x != std::string::npos) {
      const int w = std::atoi(one.substr(0, x).c_str()), h = std::atoi(one.substr(x + 1).c_str());
      if (w > 0 && h > 0) out.push_back({w, h});
    }
    at = end + 1;
  }
  return out;
}

// The picture under the page: the backdrop scaled to cover the screen, or a dark wall.
ui::Image under(const std::optional<gl::Picture>& backdrop, int w, int h) {
  ui::Image img = ui::make_image(w, h, ui::hex(0x1a1714));
  if (!backdrop || backdrop->width <= 0) return img;
  const double sx = static_cast<double>(backdrop->width) / w;
  const double sy = static_cast<double>(backdrop->height) / h;
  const double s = std::min(sx, sy);
  const double ox = (backdrop->width - w * s) * 0.5, oy = (backdrop->height - h * s) * 0.5;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const int bx = std::clamp(static_cast<int>(ox + (x + 0.5) * s), 0, backdrop->width - 1);
      const int by = std::clamp(static_cast<int>(oy + (y + 0.5) * s), 0, backdrop->height - 1);
      const size_t src = (static_cast<size_t>(by) * static_cast<size_t>(backdrop->width) +
                          static_cast<size_t>(bx)) *
                         4;
      const size_t dst =
          (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
      for (int k = 0; k < 4; k++)
        img.rgba[dst + static_cast<size_t>(k)] = backdrop->rgba[src + static_cast<size_t>(k)];
    }
  return img;
}

gl::Picture picture_of(ui::Image img) {
  gl::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

struct Game {
  ge::console::Console con;
  sm::GameMenu menu;
  sm::Host host;
  std::string connected;
  Game() {
    con.add_command("connect", "", [this](ge::console::Console&, const ge::console::Args& a) {
      connected = a.size() > 1 ? a[1] : "";
    });
    menu.add_cvars(con);
    host = {.console = &con};
  }
};

void run(gl::Lab& lab) {
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=se-browser ARGS=--viewports=1280x720,390x844");
    return;
  }
  std::optional<gl::Picture> backdrop;
  if (!lab.arg("backdrop").empty()) {
    backdrop = gl::read_png(lab.arg("backdrop"));
    if (!backdrop)
      lab.fail("--backdrop=" + lab.arg("backdrop") + " did not read as a PNG",
               "next: give a PNG file (a frame of the game, from the se-client lab)");
  }
  const double warm = lab.arg_num("warm");
  int problems = 0;
  bool table_done = false;
  for (const Viewport& v : viewports) {
    Evening ev;
    if (!ev.open(lab.arg("basedir"))) {
      lab.fail("a server did not open: " + ev.error,
               "check --basedir names a folder holding id1/pak0.pak with the plan's maps, or "
               "drop it for the built-in arena; skill: net.");
      return;
    }
    while (static_cast<double>(ev.now_us) * 1e-6 < warm) ev.frame();
    Game g;
    g.menu.servers().set_query(se::classic_query());
    g.menu.servers().use_transport(&ev.browser);
    g.menu.servers().set_lan_ports(kLanPort, kLanPort);
    g.con.execute("sb_masters sim:" + std::to_string(kMaster) + ":" + std::to_string(kMasterPort));
    g.con.execute("sb_servers sim:30:" + std::to_string(kNetPort));
    g.con.execute("menu_servers");
    const auto step = [&](ui::MenuInput in) {
      ev.frame();
      g.menu.update(in, false, g.host, ev.frame_s);
    };
    const auto settle = [&](double seconds) {
      const int64_t until = ev.now_us + static_cast<int64_t>(seconds * 1e6);
      while (ev.now_us < until) step({});
    };
    step({});
    if (!g.menu.in_servers()) {
      lab.fail("menu_servers did not open the server browser",
               "check GameMenu::show in games/shooters_edge/menu/game_menu.cpp. Skill: ui.");
      return;
    }
    const sm::ServersPage& page = g.menu.servers();
    const auto snap = [&](const std::string& id, const std::string& title,
                          const std::string& caption, const std::string& look_for) {
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      g.menu.draw(dl, v.w, v.h, 0);
      for (const std::string& p : g.menu.problems()) {
        problems++;
        std::string what = v.name();
        what += ", ";
        what += id;
        what += ": ";
        what += p;
        lab.fail(what,
                 "shorten the words in games/shooters_edge/menu/servers_page.cpp or fix its "
                 "layout; rerun `make lab LAB=se-browser`. Skill: ui.");
      }
      ui::Image img = under(backdrop, v.w, v.h);
      ui::render(dl, img);
      lab.shot({id + "-" + v.name(),
                title + " AT " + v.name(),
                caption,
                look_for,
                {{"viewport", v.name()},
                 {"answered", std::to_string(gn::server_list_answered(page.list()))},
                 {"asked", std::to_string(page.list().entries.size())},
                 {"sort", gn::server_sort_name(page.sort())}}},
               picture_of(std::move(img)));
    };
    settle(0.05);
    snap("refreshing", "THE FIRST MOMENTS",
         "MULTIPLAYER > JOIN A GAME just opened: the LAN's two servers answered the broadcast "
         "at once; the master's list and the internet's servers are still on their way.",
         "two rows, REFRESHING in the title row");
    settle(4.0);
    const gn::ServerList& sl = page.list();
    snap("ping", "EVERY SERVER, SORTED BY PING",
         "Four seconds on: the LAN's two, the four the master listed and the one the player "
         "named, each queried and timed; the master's fifth address never answers. The "
         "selection's address, game and players by name at the foot.",
         "seven rows, the LAN's first; the mode, map, players over slots and ping in columns");
    ui::MenuInput right, down, accept;
    right.right = true;
    down.down = true;
    accept.accept = true;
    step(right);
    snap("players", "SORTED BY PLAYERS",
         "RIGHT: the sort moves to players, the busiest first; the column's head lit.",
         "the 4on4 with six on top, the empty server last");
    // DOWN to the locked one.
    for (int i = 0; i < static_cast<int>(page.rows().size()); i++) {
      const gn::ServerEntry& e =
          sl.entries[static_cast<size_t>(page.rows()[static_cast<size_t>(page.selected())])];
      if (e.info.locked) break;
      step(down);
    }
    snap("locked", "A SERVER WITH A PASSWORD",
         "DOWN to the clan's practice server: its name marked with a * (it wants a password), "
         "its two players on their teams at the foot.",
         "the selected row starts with *");
    // The table of what the browser read, and the metrics.
    if (!table_done) {
      table_done = true;
      gl::LabTable t{"the servers the browser read, by ping",
                     {"name", "mode", "map", "players", "ping", "found"},
                     {}};
      std::vector<int> rows;
      gn::server_list_view(sl, gn::ServerFilter{}, gn::ServerSort::ping, rows);
      for (int i : rows) {
        const gn::ServerEntry& e = sl.entries[static_cast<size_t>(i)];
        const std::string found = e.from & gn::kServerFromLan      ? "lan"
                                  : e.from & gn::kServerFromMaster ? "master"
                                                                   : "named";
        t.rows.push_back({e.info.name, e.info.mode, e.info.map,
                          std::to_string(e.info.players) + "/" + std::to_string(e.info.max_players),
                          std::to_string(e.ping_ms), found});
      }
      lab.table(std::move(t));
      lab.metric("servers.asked", static_cast<double>(sl.entries.size()), "servers");
      lab.metric("servers.answered", gn::server_list_answered(sl), "servers");
      lab.metric("master.addresses", static_cast<double>(sl.stats.master_addresses), "servers");
      lab.metric("queries", static_cast<double>(sl.stats.queries), "datagrams");
      int players = 0;
      for (const gn::ServerEntry& e : sl.entries) players += e.info.players;
      lab.metric("players.listed", players, "players");
    }
    const std::string want = gn::to_string(
        sl.entries[static_cast<size_t>(page.rows()[static_cast<size_t>(page.selected())])].address);
    step(accept);
    const bool joined = g.connected == want && !g.menu.open();
    if (!joined)
      lab.fail("ENTER on " + want + " ran connect '" + g.connected + "'",
               "check ServersPage::input and GameMenu::update_servers; skill: ui.");
    lab.metric(v.name() + ".join", joined ? 1 : 0, "bool");
    // Nothing to look at: the LAN off and no master.
    g.con.execute("sb_lan 0");
    g.con.execute("sb_masters \"\"");
    g.con.execute("sb_servers \"\"");
    g.con.execute("menu_servers");
    step({});
    settle(3.0);
    snap("empty", "NO SERVERS FOUND",
         "The LAN switched off (sb_lan 0) and no master or server named: the page says how to "
         "name some.",
         "the words centred and inside the screen");
  }
  lab.metric("problems", problems, "count");
}

const gl::LabSpec kSpec{
    "se-browser",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/menu/servers_page (the server browser) over engine/net/server_list",
    "net",
    "Does JOIN A GAME list every server on the LAN, at the master and named, with its mode, map, "
    "players and ping, sort them, and join the one chosen?",
    {
        {"viewports", "1280x720,390x844", "the screen sizes, comma-separated WxH"},
        {"basedir", "", "an install (a folder holding id1/pak0.pak): the servers play its maps"},
        {"backdrop", "", "a PNG drawn under the page (a frame of the game)"},
        {"warm", "2", "seconds the servers run, and the clients join, before the browser opens"},
    },
    "--viewports=1280x720",
    run,
};
GE_LAB(kSpec);

}  // namespace
