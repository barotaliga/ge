// LAB `hub-players`: the hub's Player, Match and Leaders pages (engine/hub/stats_page.hpp) as a
// browser draws them. A master's evening of match reports (duels, two-a-side and four-a-side
// matches and free-for-alls on three maps, eight accounts and a bot, each player's weapons, damage
// and items as the report's columns, one match from a server the owner never listed) is written
// as a Player page whole and filtered (duels only, no ffa at a good ping), a Match page, the
// Leaders by frags and by rocket accuracy, and their JSON; the HTML is attached and hashed. When
// a headless Chromium is on the machine each page is drawn at a desktop's width and one at a
// phone's; those pictures are measured, never hashed. It answers whether the pages say what the
// reports hold, whether the filters change the sums as they should, and whether they read.
//
// ge:summary Lab: the hub's Player, Match and Leaders pages from sample reports, drawn by Chromium
// ge:keywords lab hub player-page match-page leaders weapon-accuracy filters browser screenshot
// ge:skill net labs
// ge:test tests/hub_stats_page_test.cpp
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "account/ladder.hpp"
#include "account/matches.hpp"
#include "account/service.hpp"
#include "account/store.hpp"
#include "core/prng.hpp"
#include "hub/archive.hpp"
#include "hub/pages.hpp"
#include "hub/spec.hpp"
#include "hub/stats_page.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "mod/sha256.hpp"
#include "net/report.hpp"
#include "replay/stream.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gh = ge::hub;
namespace ga = ge::account;
namespace fs = std::filesystem;

constexpr int64_t kNow_s = 1794500000;
const char* const kWeapons[] = {"axe", "sg", "ssg", "ng", "sng", "gl", "rl", "lg"};
const char* const kMore[] = {"rl_splash", "midair", "dmg_given", "dmg_taken",   "ra",
                             "ya",        "mh",     "quad",      "spawn_frags", "streak_max"};

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

struct Who {
  const char* name;
  double skill;  // 0..1: aim and frags
  int ping;
};

const Who kWho[] = {{"Ranger", 0.85, 22}, {"Vex", 0.75, 35},  {"Mira", 0.6, 48},
                    {"Tork", 0.55, 95},   {"Ozzy", 0.45, 28}, {"Lumen", 0.4, 140},
                    {"Pike", 0.35, 60},   {"Quill", 0.3, 33}};

std::vector<std::string> columns() {
  std::vector<std::string> c;
  for (const char* w : kWeapons)
    for (const char* part : {"_attacks", "_hits", "_kills", "_deaths"})
      c.push_back(std::string(w) + part);
  for (const char* m : kMore) c.emplace_back(m);
  return c;
}

gn::ReportPlayer seat(ge::core::Rng& rng, const Who& w, uint64_t account, const char* team) {
  gn::ReportPlayer p;
  p.name = w.name;
  p.account = account;
  p.team = team;
  p.seconds = 600;
  p.ping_min = w.ping - 4;
  p.ping_avg = w.ping + ge::core::int_in(rng, -3, 3);
  p.ping_max = w.ping + 30;
  const auto n = [&](double lo, double hi) {
    return static_cast<int64_t>(ge::core::range(rng, lo, hi));
  };
  int64_t kills = 0;
  // Per weapon: how much a player uses it, and how well a hit lands for them.
  const double use[] = {0.02, 0.3, 0.4, 0.2, 0.3, 0.4, 1.0, 0.8};
  const double aim[] = {0.6, 0.45, 0.5, 0.35, 0.35, 0.15, 0.38, 0.3};
  for (int i = 0; i < 8; i++) {
    const int64_t a = static_cast<int64_t>(use[i] * ge::core::range(rng, 60, 200) *
                                           (i == 1 || i == 2 ? 2.0 : 1.0));
    const int64_t h = static_cast<int64_t>(static_cast<double>(a) * aim[i] *
                                           ge::core::range(rng, 0.6, 1.0) * (0.5 + w.skill));
    const int64_t k = static_cast<int64_t>(static_cast<double>(h) * (i == 6 ? 0.25 : 0.06) *
                                           ge::core::range(rng, 0.7, 1.2));
    kills += k;
    p.columns.insert(p.columns.end(), {a, h, k, n(0, 6)});
  }
  p.frags = p.score = kills;
  p.deaths = n(8, 30) - static_cast<int64_t>(w.skill * 8);
  p.columns.insert(p.columns.end(),
                   {n(10, 40), n(0, 4), kills * 95 + n(0, 400), p.deaths * 110 + n(0, 300), n(1, 6),
                    n(1, 6), n(0, 3), n(0, 2), n(0, 3), n(2, 9)});
  return p;
}

struct Night {
  ge::net::Transport unused;
  ga::Service svc;
  uint64_t fresh_n = 1;
  std::vector<uint64_t> ids;
  uint64_t first_team_match = 0;
  gn::MatchReport recorded;  // the match whose server recorded it
  std::string demo_sha;      // its demo, joined to it by the closing summary ("" not joined)
};

void build(Night& night) {
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::ServiceConfig c;
  c.seed.fill(5);
  ga::service_init(
      night.svc, c, std::move(store), night.unused,
      [](void* ctx, uint8_t* out, size_t size) {
        auto* n = static_cast<uint64_t*>(ctx);
        for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>(*n * 37 + i * 11);
        (*n)++;
      },
      &night.fresh_n);
  for (size_t i = 0; i < std::size(kWho); i++) {
    ga::Fresh f{};
    f.fill(static_cast<uint8_t>(i + 1));
    night.ids.push_back(ga::create(night.svc.store, kWho[i].name, "a long password",
                                   (kNow_s - int64_t{90} * 86400) * 1000000, f)
                            .id);
  }
  gn::Key trusted{}, unlisted{};
  trusted.fill(1);
  unlisted.fill(2);
  ga::matches_trust(night.svc.matches, trusted);
  ge::core::Rng rng = ge::core::make_rng(59);
  const char* maps[] = {"arena", "towers", "yard"};
  const std::vector<std::string> cols = columns();
  uint64_t id = 0x5e00;
  for (int m = 0; m < 24; m++) {
    gn::MatchReport r;
    r.id = id++;
    r.game = "shooters_edge";
    r.server = m % 3 == 0 ? "Night Arena <EU>" : "Duel Hall";
    r.mode = m % 4 == 0 ? "4on4" : m % 4 == 1 ? "1on1" : m % 4 == 2 ? "ffa" : "2on2";
    r.map = maps[m % 3];
    r.start_s = kNow_s - int64_t{24 - m} * 4 * 3600;
    r.duration_ms = 600000;
    r.columns = cols;
    const size_t count = r.mode == "4on4" ? 8 : r.mode == "2on2" ? 4 : r.mode == "1on1" ? 2 : 6;
    const bool teams = r.mode == "4on4" || r.mode == "2on2";
    for (size_t k = 0; k < count; k++) {
      const size_t who = (static_cast<size_t>(m) * 3 + k) % std::size(kWho);
      r.players.push_back(
          seat(rng, kWho[who], night.ids[who], teams ? (k % 2 ? "blue" : "red") : ""));
    }
    if (r.mode == "ffa") {
      gn::ReportPlayer bot = seat(rng, {"Grunt", 0.3, 0}, 0, "");
      bot.bot = true;
      r.players.push_back(bot);
    }
    if (teams) {
      int64_t red = 0, blue = 0;
      for (const gn::ReportPlayer& p : r.players) (p.team == "red" ? red : blue) += p.score;
      r.teams = {{"red", red, 4}, {"blue", blue, 13}};
      if (r.mode == "4on4" && night.first_team_match == 0) night.first_team_match = r.id;
    }
    if (r.id == night.first_team_match) night.recorded = r;  // the match page drawn
    ga::matches_add(night.svc.matches, m == 23 ? unlisted : trusted, r);
  }
  // The recorded match's demo, as a server's recorder closes it (tv/server_source.hpp): the
  // report in the trailer. The hub reads it as the archive would and joins the two.
  ge::replay::StreamHeader h;
  h.game = "shooters_edge";
  h.meta = {{"map", night.recorded.map}, {"server", night.recorded.server}};
  h.tracks = {{1, ge::replay::TrackKind::meta, ge::replay::TrackCodec::stored, "meta"}};
  std::vector<uint8_t> file;
  ge::replay::StreamWriter w;
  ge::replay::stream_begin(w, h, file);
  const uint8_t tick[] = {1};
  ge::replay::stream_add(w, 1, 0, tick);
  ge::replay::stream_end_block(w, true);
  std::string text, error;
  if (!gn::report_text(night.recorded, text, error)) return;
  ge::replay::stream_finish(w, {{"report", text}});
  ge::hub::DemoRow row;
  if (!ge::hub::demo_row(file, row, error)) return;
  row.sha = ge::mod::sha256_hex(file.data(), file.size());  // the archive names a demo so
  if (ga::matches_link_demo(night.svc.matches, row.report_id, row.report_start_s, row.report_server,
                            row.sha))
    night.demo_sha = row.sha;
}

// The ladders' sample: the same eight accounts over three seasons (14 days each) of duels, and of
// two-on-two matches between four duos they registered; each match won by the better player's
// side more often than not.
void build_ladder(Night& night) {
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::ServiceConfig c;
  c.seed.fill(6);
  ga::service_init(
      night.svc, c, std::move(store), night.unused,
      [](void* ctx, uint8_t* out, size_t size) {
        auto* n = static_cast<uint64_t*>(ctx);
        for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>(*n * 41 + i * 13);
        (*n)++;
      },
      &night.fresh_n);
  for (size_t i = 0; i < std::size(kWho); i++) {
    ga::Fresh f{};
    f.fill(static_cast<uint8_t>(i + 1));
    night.ids.push_back(ga::create(night.svc.store, kWho[i].name, "a long password",
                                   (kNow_s - int64_t{90} * 86400) * 1000000, f)
                            .id);
  }
  gn::Key trusted{};
  trusted.fill(1);
  ga::matches_trust(night.svc.matches, trusted);
  const char* duos[] = {"Night Owls", "Iron Pair", "Slow Burn", "Last Stand"};
  for (size_t d = 0; d < 4; d++) {
    ga::matches_duo_ask(night.svc.matches, night.ids[2 * d], night.ids[2 * d + 1], duos[d]);
    ga::matches_duo_ask(night.svc.matches, night.ids[2 * d + 1], night.ids[2 * d], duos[d]);
  }
  ge::core::Rng rng = ge::core::make_rng(1600);
  const int64_t first = ga::season_start(night.svc.matches.ladder,
                                         ga::season_of(night.svc.matches.ladder, kNow_s) - 2);
  uint64_t id = 0x7000;
  for (int64_t t = first; t < kNow_s - 3600; t += int64_t{3} * 3600) {
    gn::MatchReport r;
    r.id = id++;
    r.game = "shooters_edge";
    r.server = "Duel Hall";
    r.map = "arena";
    r.start_s = t;
    r.duration_ms = 600000;
    const bool pair = (id % 3) == 0;
    r.mode = pair ? "2on2" : "1on1";
    const auto score = [&](double skill) {
      return static_cast<int64_t>(ge::core::range(rng, 5, 25) * (0.6 + skill));
    };
    if (pair) {
      const size_t a = ge::core::pick_index(rng, 4), b = (a + 1 + ge::core::pick_index(rng, 3)) % 4;
      const int64_t sa = score((kWho[2 * a].skill + kWho[2 * a + 1].skill) / 2);
      const int64_t sb = score((kWho[2 * b].skill + kWho[2 * b + 1].skill) / 2);
      r.teams = {{"red", sa, 4}, {"blue", sb, 13}};
      for (size_t k = 0; k < 2; k++) {
        gn::ReportPlayer p;
        p.name = kWho[2 * a + k].name;
        p.account = night.ids[2 * a + k];
        p.team = "red";
        p.score = p.frags = sa / 2 + static_cast<int64_t>(k) * (sa % 2);
        r.players.push_back(p);
        p.name = kWho[2 * b + k].name;
        p.account = night.ids[2 * b + k];
        p.team = "blue";
        p.score = p.frags = sb / 2 + static_cast<int64_t>(k) * (sb % 2);
        r.players.push_back(p);
      }
    } else {
      const size_t a = ge::core::pick_index(rng, 8), b = (a + 1 + ge::core::pick_index(rng, 7)) % 8;
      for (size_t w : {a, b}) {
        gn::ReportPlayer p;
        p.name = kWho[w].name;
        p.account = night.ids[w];
        p.score = p.frags = score(kWho[w].skill);
        r.players.push_back(p);
      }
    }
    ga::matches_add(night.svc.matches, trusted, r);
  }
  ga::ladder_close_due(night.svc.matches.ladder, kNow_s);
}

std::string id_hex(uint64_t id) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(id));
  return b;
}

void run(gl::Lab& lab) {
  Night night;
  build(night);
  gh::HubSpec spec;
  spec.game = "Shooter's Edge";
  spec.id = "shooters_edge";
  spec.score = "frags";
  spec.stats = true;
  const auto get = [&](const std::string& target) {
    gh::Answer a;
    if (!gh::stats_answer(spec, &night.svc, target, kNow_s, a) || a.code != 200)
      lab.fail("the hub did not answer " + target + " with a page",
               "next: read stats_answer's routing in engine/hub/stats_page.cpp");
    return a.body;
  };
  const std::string player = get("/player/ranger");
  const std::string duels = get("/player/ranger?mode=1on1");
  const std::string good = get("/player/ranger?without=ffa&ping=40");
  const std::string match = get("/match/" + id_hex(night.first_team_match));
  const std::string leaders = get("/leaders");
  const std::string accuracy = get("/leaders?stat=rl_acc&without=ffa");
  const std::string json = get("/v1/players/ranger");
  Night ladder_night;
  build_ladder(ladder_night);
  const auto ladder_get = [&](const std::string& target) {
    gh::Answer a;
    if (!gh::stats_answer(spec, &ladder_night.svc, target, kNow_s, a) || a.code != 200)
      lab.fail("the hub did not answer " + target + " with a page",
               "next: read ladder_answer in engine/hub/ladder_page.cpp");
    return a.body;
  };
  const std::string ladder_duel = ladder_get("/ladder?mode=1on1");
  const std::string ladder_pair = ladder_get("/ladder?mode=2on2");
  lab.attach("ladder-1on1.html", bytes(ladder_duel));
  lab.attach("ladder-2on2.html", bytes(ladder_pair));
  lab.attach("ladder.json", bytes(ladder_get("/v1/ladder?mode=1on1")));
  const ga::Ladder& lad = ladder_night.svc.matches.ladder;
  lab.metric("ladder_rated", static_cast<double>(lad.rated), "matches");
  lab.metric("ladder_seasons_closed", static_cast<double>(lad.history.size()), "records");
  lab.metric("ladder_duos", static_cast<double>(lad.duos.size()), "duos");
  if (lad.history.empty())
    lab.fail("no past season was closed into the history",
             "next: read ladder_close_due in engine/account/ladder.cpp");
  lab.attach("player.html", bytes(player));
  lab.attach("player-duels.html", bytes(duels));
  lab.attach("player-good-ping.html", bytes(good));
  lab.attach("match.html", bytes(match));
  lab.attach("leaders.html", bytes(leaders));
  lab.attach("leaders-rl-accuracy.html", bytes(accuracy));
  lab.attach("player.json", bytes(json));
  lab.attach("leaders.json", bytes(get("/v1/leaders?stat=frags")));
  const ga::Totals all = ga::totals_sum(night.svc.matches, night.ids[0], {});
  ga::TotalsFilter f;
  f.modes = {"1on1"};
  const ga::Totals one = ga::totals_sum(night.svc.matches, night.ids[0], f);
  lab.metric("demo_joined", night.demo_sha.empty() ? 0 : 1, "demos");
  if (night.demo_sha.empty())
    lab.fail("the recorded match's demo was not joined to it by its closing summary",
             "Run tests/hub_archive_test and tests/account_matches_test (matches_link_demo); "
             "skill: net.");
  lab.metric("reports_kept", static_cast<double>(night.svc.matches.kept.size()), "reports");
  lab.metric("ranger_matches", static_cast<double>(all.matches), "matches");
  lab.metric("ranger_duels", static_cast<double>(one.matches), "matches");
  lab.metric("ranger_frags", static_cast<double>(all.frags), "frags");
  lab.metric("player_page", static_cast<double>(player.size()), "bytes");
  lab.metric("match_page", static_cast<double>(match.size()), "bytes");
  lab.metric("leaders_page", static_cast<double>(leaders.size()), "bytes");
  if (one.matches == 0 || one.matches >= all.matches)
    lab.fail("the mode filter did not narrow Ranger's matches",
             "next: read totals_keeps in engine/account/matches.cpp");
  if (match.find("Night Arena <EU>") != std::string::npos)
    lab.fail("a server's name reached the Match page unescaped",
             "next: escape it with html_escape (stats_page.cpp)");

  const std::string browser = lab.arg("browser") == "0" ? "" : ge::lab::hub_browser::find_browser();
  if (browser.empty()) {
    lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium): pages not drawn");
    return;
  }
  std::error_code ec;
  const fs::path dir = fs::temp_directory_path(ec) / "ge-hub-players";
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
      {"player", "A PLAYER'S PAGE", &player, 1280, 1500, true,
       "GET /player/ranger: every counted match summed, by mode, the weapons by kills with their "
       "accuracy, the other columns, the recent matches.",
       "tiles of matches, wins, frags and efficiency; rl and lg leading the weapons; the match "
       "from the unlisted server among the recent ones."},
      {"player-duels", "THE SAME PLAYER, DUELS ONLY", &duels, 1280, 1200, true,
       "GET /player/ranger?mode=1on1: the same page under the mode filter.",
       "fewer matches in the tiles, one row in By mode, only duels in the recent list."},
      {"player-good-ping", "NO FFA, UNDER 40 MS", &good, 390, 2200, false,
       "GET /player/ranger?without=ffa&ping=40 on a phone: free-for-alls left out, matches at a "
       "good ping only.",
       "the 'no ffa' box ticked, 'under 40 ms' chosen, the tables fitting the width."},
      {"match", "A FOUR-A-SIDE MATCH", &match, 1280, 1500, true,
       "GET /match/{id}: the teams' scores, the scoreboard, each player's accuracy per weapon "
       "and the other columns.",
       "red and blue as tiles, eight players by team and frags, an accuracy cell per weapon."},
      {"leaders", "THE LEADERS BY FRAGS", &leaders, 1280, 900, false,
       "GET /leaders: the accounts by frags, all time, with the filters and the stat to rank by.",
       "Ranger and Vex near the top; each name links to the Player page under the same filter."},
      {"leaders-accuracy", "THE LEADERS BY ROCKET ACCURACY", &accuracy, 1280, 900, true,
       "GET /leaders?stat=rl_acc&without=ffa: a ratio, counted over three matches or more.",
       "percentages, the line on the minimum of matches below the table."},
  };
  const View more[] = {
      {"ladder-1on1", "THE 1ON1 LADDER", &ladder_duel, 1280, 1700, true,
       "GET /ladder?mode=1on1: this season's standings (five matches to be ranked, the rest "
       "dimmed), the all-time ratings, and the champions of the two seasons before.",
       "the ranked players above the dimmed rows that lack matches, each dimmed row saying how "
       "many more; two past seasons with three champions each."},
      {"ladder-2on2", "THE 2ON2 LADDER OF DUOS", &ladder_pair, 390, 2000, false,
       "GET /ladder?mode=2on2 on a phone: the registered duos by name with their two players.",
       "Night Owls (Ranger and Vex) leading; every duo's two players linked to their pages."},
  };
  int drawn = 0;
  std::vector<View> shown(std::begin(views), std::end(views));
  shown.insert(shown.end(), std::begin(more), std::end(more));
  for (const View& v : shown) {
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
  lab.metric("pages_drawn", drawn, "of 8", gl::Det::measured);
  fs::remove_all(dir, ec);
}

}  // namespace

const gl::LabSpec kSpec{
    "hub-players",
    gl::LabKind::picture,
    "engine",
    "engine/hub (the stats pages)",
    "net",
    "Do the hub's Player, Match and Leaders pages say what the match reports hold, do the filters "
    "narrow the sums, and do the pages read at a desktop's and a phone's width?",
    {
        {"browser", "1", "draw the pages with a headless Chromium when one is found (0: never)"},
    },
    "--browser=0",
    run,
};
GE_LAB(kSpec);
