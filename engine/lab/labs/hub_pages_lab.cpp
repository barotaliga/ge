// LAB `hub-pages`: the hub's pages (docs/hub.md § What a game's community gets) as a browser draws
// them. An index of a busy evening (a full team match with its teams and a broadcast, a duel, a
// capture match, a public deathmatch half bots, a server of bots alone, a match on its score
// screen, empty servers and a TV node) is written as the Servers page, the Players page, a filtered
// page, one scoreboard overlay and the broadcast's Watch page, then the Account page (its forms
// over HTTPS and over plain HTTP, and a login code made), the Demos page over an archive's rows,
// and a player's, a match's and the Leaders page over a master's reports; their HTML is attached
// and hashed. The pages wear the game's look (`HubSpec::look`, here `arena`).
// When a headless Chromium is on the machine (GE_CHROMIUM, or the container's
// /opt/pw-browsers/chromium), each page is drawn at a desktop's and a phone's width and shown:
// those pictures are the browser's, so they are measured, never hashed. It answers whether the
// pages say what the index holds, in the interest rule's order, and whether they read at both
// widths.
//
// ge:summary Lab: the hub's HTML pages written from a sample index and drawn by headless Chromium
// ge:keywords lab hub pages html browser chromium screenshot phone desktop cards overlay
// ge:keywords account-page demos-page player-page match-page leaders-page look
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

#include "account/matches.hpp"
#include "account/service.hpp"
#include "account/store.hpp"
#include "hub/account_page.hpp"
#include "hub/archive.hpp"
#include "hub/pages.hpp"
#include "hub/poller.hpp"
#include "hub/spec.hpp"
#include "hub/stats_page.hpp"
#include "hub/watch.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "lab/png.hpp"
#include "net/packet.hpp"
#include "net/report.hpp"
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
  s.look = "arena";
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

constexpr int64_t kNowS = 1794500000;  // the sample evening's wall clock (2026-11-12)

// The archive's sample rows (no files: the Demos page reads only the rows): the evening's matches,
// newest first, one with its scores and a tag.
gh::Archive sample_archive() {
  struct Sample {
    const char* map;
    const char* mode;
    const char* server;
    int64_t ago_s, duration_s;
    std::vector<std::pair<const char*, int>> players;
    std::vector<std::pair<const char*, int>> teams;
    const char* tag;
  };
  const Sample samples[] = {
      {"tower",
       "4on4",
       "Night Arena <EU>",
       1800,
       1200,
       {{"vale", 41},
        {"sable", 39},
        {"orrin", 37},
        {"wren", 33},
        {"ash", 30},
        {"kit", 25},
        {"moss", 19},
        {"fen", 12}},
       {{"red", 122}, {"blue", 114}},
       "final"},
      {"crypt", "1on1", "Duel Hall", 5400, 600, {{"quill", 27}, {"brand", 19}}, {}, ""},
      {"aqueduct",
       "2on2",
       "Team Two",
       9000,
       900,
       {{"nyx", 25}, {"oak", 22}, {"ivy", 18}, {"elm", 15}},
       {{"red", 47}, {"blue", 33}},
       ""},
      {"foundry", "ffa", "Open Pit", 30000, 900, {{"tamsin", 23}, {"cole", 14}}, {}, ""},
      {"bridges",
       "ctf",
       "Flag Yard",
       90000,
       1200,
       {{"pike", 6}, {"rook", 4}, {"lark", 5}, {"dune", 3}},
       {{"red", 3}, {"blue", 2}},
       ""},
  };
  gh::Archive a;
  a.dir = "sample";
  int n = 0;
  for (const Sample& m : samples) {
    gh::DemoRow r;
    char sha[65];
    std::snprintf(sha, sizeof sha, "%064x", 0x5eed0 + n++);
    r.sha = sha;
    r.game = "shooters_edge";
    r.map = m.map;
    r.mode = m.mode;
    r.server = m.server;
    r.start_s = kNowS - m.ago_s;
    r.duration_ms = m.duration_s * 1000;
    r.size = 180000 + n * 37000;
    r.added_s = r.start_s + m.duration_s;
    const char* sides[] = {"red", "blue"};
    int i = 0;
    for (const auto& [name, score] : m.players)
      r.players.push_back({name, m.teams.empty() ? "" : sides[i++ % 2], score, true});
    for (const auto& [name, score] : m.teams) r.teams.push_back({name, score, true});
    if (*m.tag) r.tags.push_back(m.tag);
    a.rows.push_back(r);
  }
  return a;
}

// A downhill game's hub (Fall Line's spec, `alpine`): runs found by their seed, times as m:ss.cc.
gh::HubSpec runs_spec() {
  gh::HubSpec s;
  s.game = "Fall Line";
  s.id = "fall_line";
  s.modes = {{"race", 3}, {"free ride", 2}};
  s.score = "time";
  s.score_low_first = true;
  s.score_ms = true;
  s.look = "alpine";
  s.map_word = "seed";
  return s;
}

gh::Archive sample_runs() {
  struct Sample {
    const char* seed;
    const char* mode;
    int64_t ago_s;
    std::vector<std::pair<const char*, int>> racers;  // their times in ms
  };
  const Sample samples[] = {
      {"38", "race", 900, {{"Player", 214370}, {"Rival 1", 218050}, {"Rival 2", 223910}}},
      {"38", "race", 4200, {{"Player", 216880}, {"Rival 1", 217420}}},
      {"7",
       "race",
       9600,
       {{"Player", 187240}, {"Rival 1", 190010}, {"Rival 2", 191550}, {"Rival 3", 199800}}},
      {"112", "free ride", 30000, {{"Player", 0}}},
  };
  gh::Archive a;
  a.dir = "sample";
  int n = 0;
  for (const Sample& m : samples) {
    gh::DemoRow r;
    char sha[65];
    std::snprintf(sha, sizeof sha, "%064x", 0xf1a0 + n++);
    r.sha = sha;
    r.game = "fall_line";
    r.run = true;
    r.map = m.seed;
    r.mode = m.mode;
    r.title = std::string(std::string(m.mode) == "race" ? "A race" : "A free ride") + " on seed " +
              m.seed;
    r.start_s = kNowS - m.ago_s;
    r.duration_ms = m.racers.front().second ? m.racers.front().second + 4000 : 600000;
    r.size = 2400 + n * 300;
    r.added_s = r.start_s;
    for (const auto& [name, ms] : m.racers) r.players.push_back({name, "", ms, ms > 0});
    a.rows.push_back(r);
  }
  return a;
}

// The match reports a master keeps (account/matches.hpp): an evening of duels and a team match,
// for the Player, Match and Leaders pages.
void sample_reports(ge::account::Service& svc) {
  namespace ga = ge::account;
  const char* names[] = {"vale", "sable", "orrin", "wren", "quill", "brand"};
  uint64_t ids[6] = {};
  for (int i = 0; i < 6; i++) {
    ga::Fresh f{};
    f.fill(static_cast<uint8_t>(20 + i));
    ids[i] = ga::create(svc.store, names[i], "a long password", kNowS * 1000000, f).id;
  }
  gn::Key trusted{};
  trusted.fill(7);
  ga::matches_trust(svc.matches, trusted);
  auto seat = [&](int who, const char* team, int64_t frags, int64_t deaths, int64_t ping,
                  int64_t rl_shots, int64_t rl_hits, int64_t lg_shots, int64_t lg_hits) {
    gn::ReportPlayer p;
    p.name = names[who];
    p.account = ids[who];
    p.team = team;
    p.score = p.frags = frags;
    p.deaths = deaths;
    p.seconds = 1200;
    p.ping_min = ping - 4;
    p.ping_avg = ping;
    p.ping_max = ping + 9;
    p.columns = {rl_shots, rl_hits,   frags * 2 / 3, deaths / 2, lg_shots,
                 lg_hits,  frags / 3, deaths / 3,    frags / 6};
    return p;
  };
  auto report = [](uint64_t id, const char* mode, const char* map, const char* server,
                   int64_t ago_s) {
    gn::MatchReport r;
    r.id = id;
    r.game = "shooters_edge";
    r.server = server;
    r.mode = mode;
    r.map = map;
    r.start_s = kNowS - ago_s;
    r.duration_ms = 1200000;
    r.columns = {"rl_attacks", "rl_hits",  "rl_kills",  "rl_deaths", "lg_attacks",
                 "lg_hits",    "lg_kills", "lg_deaths", "streak_max"};
    return r;
  };
  gn::MatchReport team = report(0x4a11, "4on4", "tower", "Night Arena <EU>", 1800);
  team.teams = {{"red", 78, 0xd94a4a}, {"blue", 72, 0x4a7ad9}};
  team.players = {seat(0, "red", 41, 22, 22, 120, 52, 900, 310),
                  seat(2, "red", 37, 25, 31, 110, 41, 760, 220),
                  seat(1, "blue", 39, 24, 27, 130, 49, 820, 290),
                  seat(3, "blue", 33, 29, 25, 95, 33, 640, 180)};
  ga::matches_add(svc.matches, trusted, team);
  for (int i = 0; i < 5; i++) {
    gn::MatchReport duel = report(0x4b00 + static_cast<uint64_t>(i), "1on1",
                                  i % 2 ? "crypt" : "tower", "Duel Hall", 5400 + i * 86400);
    duel.players = {seat(i % 2 ? 4 : 0, "", 27 - i, 19 + i, 14, 80, 36 - i, 500, 170),
                    seat(i % 2 ? 5 : 1, "", 19 + i, 27 - i, 18, 85, 30, 520, 150)};
    ga::matches_add(svc.matches, trusted, duel);
  }
}

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
  // The Account page (M59b3): its forms over HTTPS and over plain HTTP, and a code made.
  ge::net::Transport unused;
  ge::account::Service accounts;
  uint64_t fresh_n = 1;  // the fresh bytes' counter: each run starts it again, so it digests alike
  {
    ge::account::Store store;
    ge::account::store_init(store, ge::account::HashCost{8, 1});
    ge::account::ServiceConfig c;
    c.seed.fill(5);
    ge::account::service_init(
        accounts, c, std::move(store), unused,
        [](void* ctx, uint8_t* out, size_t size) {
          auto* n = static_cast<uint64_t*>(ctx);
          for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>(*n * 37 + i * 11);
          (*n)++;
        },
        &fresh_n);
  }
  const int64_t kUnix = 1794500000ll * 1000000;
  gh::Answer account_forms, account_plain, account_code;
  gh::account_answer(spec, &accounts, {"GET", "/account", "", true, "a", kUnix}, account_forms);
  gh::account_answer(spec, &accounts, {"GET", "/account", "", false, "a", kUnix}, account_plain);
  gh::Answer made;
  gh::account_answer(spec, &accounts,
                     {"POST", "/account/signup",
                      "name=Ranger&password=long+enough&again=long+enough", true, "a", kUnix},
                     made);
  gh::account_answer(
      spec, &accounts,
      {"POST", "/account/code", "name=ranger&password=long+enough", true, "a", kUnix},
      account_code);
  // The archive's Demos page, and the Player, Match and Leaders pages over a master's reports.
  const gh::Archive archive = sample_archive();
  gh::Site site;
  site.archive = &archive;
  site.unix_s = kNowS;
  site.host = "hub.example";
  const std::string demos = gh::demos_page(site, spec, "/demos");
  gh::HubSpec stats_spec = spec;
  stats_spec.stats = true;
  sample_reports(accounts);
  gh::Answer player_page, match_page, leaders_page;
  gh::stats_answer(stats_spec, &accounts, "/player/vale", kNowS, player_page);
  gh::stats_answer(stats_spec, &accounts, "/match/0000000000004a11", kNowS, match_page);
  gh::stats_answer(stats_spec, &accounts, "/leaders", kNowS, leaders_page);
  const gh::HubSpec fl_spec = runs_spec();
  const gh::Archive runs = sample_runs();
  gh::Site fl_site = site;
  fl_site.archive = &runs;
  const std::string runs_page = gh::demos_page(fl_site, fl_spec, "/demos");
  const std::string seed_page = gh::demos_page(fl_site, fl_spec, "/demos?map=38");
  lab.metric(
      "runs_on_seed_38",
      static_cast<double>(gh::archive_search(runs, gh::demo_query("/demos?map=38"), kNowS).total),
      "runs");
  lab.attach("runs.html", bytes(runs_page));
  lab.attach("demos.html", bytes(demos));
  lab.attach("player.html", bytes(player_page.body));
  lab.attach("match.html", bytes(match_page.body));
  lab.attach("leaders.html", bytes(leaders_page.body));
  if (player_page.code != 200 || match_page.code != 200 || leaders_page.code != 200 ||
      demos.find("Night Arena &lt;EU&gt;") == std::string::npos) {
    lab.fail("the Demos, Player, Match or Leaders page did not show the sample evening",
             "next: hub/pages.cpp (demos), hub/stats_page.cpp; skill: net");
  }
  lab.attach("account.html", bytes(account_forms.body));
  lab.attach("account-code.html", bytes(account_code.body));
  if (made.code != 200 || account_code.code != 200 ||
      account_code.body.find("<code>login Ranger ") == std::string::npos) {
    lab.fail("the Account page did not sign up and make a code",
             "next: hub/account_page.cpp; skill: net");
  }
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
      {"account", "THE ACCOUNT PAGE OVER HTTPS", &account_forms.body, 1280, 1100, true,
       "GET /account on a hub run with --accounts, reached over HTTPS: sign up, a login code for "
       "a classic client, and a new password by the recovery code.",
       "three short forms, one under another, each with a line on what it is for."},
      {"account-plain", "THE ACCOUNT PAGE OVER PLAIN HTTP, ON A PHONE", &account_plain.body, 390,
       1500, false,
       "The same page reached over plain HTTP: it never asks for a password in the clear.",
       "the warning in amber above, every field and button dimmed and disabled."},
      {"account-code", "A LOGIN CODE MADE", &account_code.body, 1280, 520, true,
       "POST /account/code: the code a classic client's console takes, and the line to type.",
       "the code large, 'login Ranger <code>' below it, good once within 15 minutes."},
      {"demos", "THE DEMOS PAGE", &demos, 1280, 900, false,
       "GET /demos: the archive searched by player, map, mode and tag, newest first; each row's "
       "scores folded away until asked.",
       "the search form above, five matches below with their maps, modes, players and the 'final' "
       "tag, Watch and Download on each."},
      {"demos-phone", "THE DEMOS PAGE ON A PHONE", &demos, 390, 1400, true,
       "The same page at a phone's width in the dark scheme.",
       "the less needed columns gone, the buttons stacked, no sideways scroll."},
      {"player", "A PLAYER'S PAGE", &player_page.body, 1280, 1300, true,
       "GET /player/vale: an account's totals as tiles, by mode, the weapons' accuracy and the "
       "recent matches.",
       "the totals large; rocket and lightning accuracy; six matches, newest first."},
      {"match", "ONE MATCH'S PAGE", &match_page.body, 1280, 1000, false,
       "GET /match/{id}: a kept report: the teams, the scoreboard, each player's accuracy.",
       "red 78 to blue 72 on tower; four players with their weapons."},
      {"runs", "A DOWNHILL GAME'S RUNS, ITS OWN LOOK", &runs_page, 1280, 700, false,
       "GET /demos on Fall Line's hub (look `alpine`): the archive's runs, each found by its seed, "
       "the racers' times at the line.",
       "the glacier blue look; the column and the search field say 'seed'; times as m:ss.cc."},
      {"runs-seed-dark", "THE RUNS DOWN ONE SEED, IN THE DARK", &seed_page, 1280, 600, true,
       "GET /demos?map=38: only the runs down seed 38, in the alpine look's night scheme.",
       "two races, both on seed 38, the newest first."},
      {"leaders", "THE LEADERS PAGE", &leaders_page.body, 1280, 900, true,
       "GET /leaders: the best accounts by frags (a stat chosen above), all time.",
       "vale first; the top three ranks in the accent colour."},
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
  lab.metric("pages_drawn", drawn, "of 16", gl::Det::measured);
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
