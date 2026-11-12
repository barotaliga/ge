// LAB `hub-pages`: the hub's pages (docs/hub.md § What a game's community gets) as a browser draws
// them. An index of a busy evening (a full team match with its teams and a broadcast, a duel, a
// capture match, a public deathmatch half bots, a server of bots alone, a match on its score
// screen, empty servers and a TV node) is written as the Servers page, the Players page, a filtered
// page, one scoreboard overlay and the broadcast's Watch page; their HTML is attached and hashed.
// When a headless Chromium is on the machine (GE_CHROMIUM, or the container's
// /opt/pw-browsers/chromium), each page is drawn at a desktop's and a phone's width and shown:
// those pictures are the browser's, so they are measured, never hashed. It answers whether the
// pages say what the index holds, in the interest rule's order, and whether they read at both
// widths.
//
// ge:summary Lab: the hub's HTML pages written from a sample index and drawn by headless Chromium
// ge:keywords lab hub pages html browser chromium screenshot phone desktop cards overlay
// ge:skill net labs
// ge:test tests/hub_pages_test.cpp
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "hub/pages.hpp"
#include "hub/poller.hpp"
#include "hub/spec.hpp"
#include "hub/watch.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "lab/png.hpp"
#include "net/packet.hpp"
#include "net/status.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gh = ge::hub;
namespace fs = std::filesystem;

gh::HubSpec sample_spec() {
  gh::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.protocol = 42;
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ctf", 6}, {"ffa", 3}};
  s.score = "frags";
  return s;
}

struct Seat {
  const char* name;
  int score;
  uint8_t flags;
  const char* team;
  int ping;
};

gh::HubNode server(uint32_t at, const char* name, const char* mode, const char* map,
                   gn::StatusPhase phase, int left_s, int max_players, const char* country,
                   const std::vector<Seat>& seats) {
  gh::HubNode n;
  n.address = gn::sim_address(at, 27500);
  n.kind = gn::NodeKind::game;
  n.listed = n.have = true;
  n.changes = 1;
  gn::Status& s = n.status;
  s.name = name;
  s.mode = mode;
  s.map = map;
  s.phase = phase;
  s.time_left_s = left_s;
  s.max_players = max_players;
  std::snprintf(s.country, sizeof s.country, "%s", country);
  for (const Seat& seat : seats) {
    gn::StatusPlayer p;
    p.name = seat.name;
    p.score = seat.score;
    p.flags = seat.flags;
    p.team = seat.team;
    p.ping_ms = static_cast<uint16_t>(seat.ping);
    s.players.push_back(p);
  }
  return n;
}

gh::Poller sample_index() {
  constexpr uint8_t B = gn::kPlayerBot, S = gn::kPlayerSpectator;
  gh::Poller p;
  gh::HubNode four =
      server(10, "Night Arena <EU>", "4on4", "tower", gn::StatusPhase::playing, 731, 8, "SE",
             {{"vale", 41, 0, "red", 22},
              {"orrin", 37, 0, "red", 31},
              {"kit", 25, 0, "red", 18},
              {"moss", 19, 0, "red", 40},
              {"sable", 39, 0, "blue", 27},
              {"wren", 33, 0, "blue", 25},
              {"ash", 30, 0, "blue", 35},
              {"fen", 12, 0, "blue", 51},
              {"lurker", 0, S, "", 60}});
  four.status.teams = {{"red", 122, 0xd94a4a}, {"blue", 114, 0x4a7ad9}};
  gh::HubNode tv;
  tv.address = gn::sim_address(40, 28000);
  tv.kind = gn::NodeKind::tv;
  tv.listed = tv.have = true;
  tv.status.kind = gn::NodeKind::tv;
  tv.status.keys = {{"viewers", "23"}, {"delay_s", "30"}, {"web", "28080"}};
  gn::StatusStream st;
  st.node = tv.address;
  st.stream = 1;
  st.viewers = 23;
  four.status.streams.push_back(st);
  p.nodes.push_back(four);
  p.nodes.push_back(server(11, "Duel Hall", "1on1", "crypt", gn::StatusPhase::playing, 312, 2, "DE",
                           {{"quill", 14, 0, "", 12}, {"brand", 11, 0, "", 15}}));
  p.nodes.push_back(server(12, "Flag Yard", "ctf", "bridges", gn::StatusPhase::countdown, 900, 16,
                           "NL",
                           {{"pike", 0, 0, "red", 33},
                            {"rook", 0, 0, "red", 29},
                            {"lark", 0, 0, "blue", 41},
                            {"dune", 0, 0, "blue", 38}}));
  p.nodes.push_back(server(13, "Open Pit", "ffa", "foundry", gn::StatusPhase::playing, 455, 12,
                           "US",
                           {{"tamsin", 17, 0, "", 88},
                            {"cole", 9, 0, "", 95},
                            {"Grunt", 21, B, "", 0},
                            {"Brute", 13, B, "", 0},
                            {"Scrap", 6, B, "", 0}}));
  p.nodes.push_back(
      server(14, "Bot Practice", "ffa", "cellar", gn::StatusPhase::playing, -1, 8, "FI",
             {{"Grunt", 30, B, "", 0}, {"Brute", 28, B, "", 0}, {"Scrap", 19, B, "", 0}}));
  p.nodes.push_back(server(15, "Team Two", "2on2", "aqueduct", gn::StatusPhase::intermission, 9, 4,
                           "SE",
                           {{"nyx", 25, 0, "red", 20},
                            {"oak", 22, 0, "red", 24},
                            {"ivy", 18, 0, "blue", 19},
                            {"elm", 15, 0, "blue", 30}}));
  p.nodes.push_back(
      server(16, "Quiet Corner", "4on4", "tower", gn::StatusPhase::lobby, -1, 8, "PL", {}));
  p.nodes.push_back(
      server(17, "Late Night FFA", "ffa", "foundry", gn::StatusPhase::lobby, -1, 12, "US", {}));
  p.nodes.push_back(tv);
  return p;
}

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

void run(gl::Lab& lab) {
  const gh::HubSpec spec = sample_spec();
  const gh::Poller index = sample_index();
  const int64_t now = 0;
  const std::string servers = gh::servers_page(index, spec, "", now);
  const std::string players = gh::players_page(index, spec, now);
  const std::string ffa = gh::servers_page(index, spec, "ffa", now);
  const std::string overlay = gh::scoreboard_page(spec, index.nodes[0]);
  // The Watch page of the team match's broadcast, on a hub that serves no viewer yet: the
  // picture's place says so, the scoreboard and the other broadcasts beside it.
  gh::Answer watch;
  gh::watch_answer(index, spec, gh::watch_path(index.nodes[0].status.streams[0].node, 1), now, {},
                   watch);
  const std::string& watch_html = watch.body;
  lab.attach("servers.html", bytes(servers));
  lab.attach("players.html", bytes(players));
  lab.attach("servers-ffa.html", bytes(ffa));
  lab.attach("scoreboard.html", bytes(overlay));
  lab.attach("watch.html", bytes(watch_html));
  lab.attach("style.css", bytes(gh::hub_stylesheet()));
  lab.metric("servers_page", static_cast<double>(servers.size()), "bytes");
  lab.metric("players_page", static_cast<double>(players.size()), "bytes");
  lab.metric("overlay_page", static_cast<double>(overlay.size()), "bytes");
  lab.metric("watch_page", static_cast<double>(watch_html.size()), "bytes");
  lab.metric("stylesheet", static_cast<double>(gh::hub_stylesheet().size()), "bytes");

  // The cards' order on the page: the interest rule's (full human matches first, by humans; then
  // by humans and the mode's weight; the bots last).
  const char* expect[] = {
      "Night Arena &lt;EU&gt;", "Team Two", "Duel Hall", "Flag Yard", "Open Pit", "Bot Practice"};
  size_t last = 0;
  int in_order = 0;
  for (const char* name : expect) {
    const size_t at = servers.find(name);
    if (at == std::string::npos || at < last) break;
    last = at;
    in_order++;
  }
  lab.metric("cards_in_order", in_order, "of 6");
  if (servers.find("Night Arena <EU>") != std::string::npos) {
    lab.fail("a server's name reached the page unescaped",
             "next: escape it with html_escape (pages.cpp)");
  }
  if (servers.find("<script") != std::string::npos) {
    lab.fail("the Servers page carries script", "next: the pages are plain HTML; drop the script");
  }

  const std::string browser = lab.arg("browser") == "0" ? "" : ge::lab::hub_browser::find_browser();
  if (browser.empty()) {
    lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium): pages not drawn");
    return;
  }
  std::error_code ec;
  const fs::path dir = fs::temp_directory_path(ec) / "ge-hub-pages";
  fs::create_directories(dir, ec);
  struct View {
    const char* id;
    const char* title;
    const std::string* page;
    int w, h;
    bool dark;
    const char* what;
    const char* look;
  };
  const View views[] = {
      {"servers-desktop", "THE SERVERS PAGE, 1280 WIDE", &servers, 1280, 1400, false,
       "GET / at a desktop's width: a card per server with anyone on it, in the interest rule's "
       "order, the mode filters above, the empty servers listed after.",
       "the full team match first with its teams, a Watch link and 23 watching; the bots-only "
       "server last of the cards; the score screen marked 'scores'."},
      {"servers-phone", "THE SERVERS PAGE ON A PHONE, 390 WIDE", &servers, 390, 2400, false,
       "The same page at a phone's width: one column of cards, nothing wider than the screen.",
       "one card a row, the filters wrapping, no sideways scroll."},
      {"servers-dark", "THE SERVERS PAGE IN THE DARK SCHEME", &servers, 1280, 760, true,
       "The same page for a reader whose system asks for dark: the stylesheet's dark colours.",
       "the same cards on a dark ground, the text and the links as legible as in light."},
      {"players-desktop", "THE PLAYERS PAGE", &players, 1280, 1200, false,
       "GET /players: everyone on every server, people before bots, by the server's interest.",
       "the bots at the bottom marked in amber; spectators dimmed."},
      {"overlay", "ONE SERVER'S SCOREBOARD OVERLAY", &overlay, 460, 520, true,
       "GET /scoreboard/{address}: one server's scoreboard alone, sized for a streamer's overlay.",
       "the teams' totals above the players, highest frags first."},
      {"watch", "THE WATCH PAGE OF A BROADCAST", &watch_html, 1280, 820, true,
       "GET /watch/{node}/{stream}: one broadcast; the game's viewer goes in the black frame (a "
       "hub started with --viewer serves it), here the reason it is not there; the match's "
       "scoreboard and the other broadcasts beside it.",
       "the frame at 16:9 with its words centred, the scoreboard to the right, Open in the game "
       "below with 23 watching and 30 s behind."},
  };
  int drawn = 0;
  for (const View& v : views) {
    const std::optional<gl::Picture> pic =
        ge::lab::hub_browser::hub_draw(browser, dir, v.id, *v.page, v.w, v.h, v.dark);
    if (!pic) {
      lab.note(std::string("Chromium did not draw ") + v.id);
      continue;
    }
    drawn++;
    lab.shot({v.id, v.title, v.what, v.look, {{"width", std::to_string(v.w)}}}, *pic,
             gl::Det::measured);
  }
  lab.metric("pages_drawn", drawn, "of 6", gl::Det::measured);
  fs::remove_all(dir, ec);
}

const gl::LabSpec kSpec{
    "hub-pages",
    gl::LabKind::picture,
    "engine",
    "engine/hub (the pages)",
    "net",
    "Do the hub's pages say what its index holds, in the interest rule's order, and read at a "
    "desktop's and a phone's width?",
    {
        {"browser", "1", "draw the pages with a headless Chromium when one is found (0: never)"},
    },
    "--browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
