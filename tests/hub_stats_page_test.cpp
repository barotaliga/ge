// engine/hub/stats_page.hpp: the Player page sums an account's counted matches and filters them by
// mode, the modes left out, map and ping; its weapons come from the report's `<w>_attacks` and
// `<w>_hits` columns, by kills; the Match page shows a kept report's scoreboard and accuracy and
// says when a report counts in no totals; the Leaders page ranks by a count, a column or an
// accuracy, a ratio only over enough matches, all time or over recent days; the JSON says the
// same; a hub without accounts, an unknown name and an unknown match answer 404 in words.
//
// ge:summary Test: the hub's Player, Match and Leaders pages and their JSON over sample reports
// ge:keywords test hub player-page match-page leaders filters weapon-accuracy v1-players
// ge:skill net testing
#include "hub/stats_page.hpp"

#include <string>

#include "account/matches.hpp"
#include "account/store.hpp"
#include "check.hpp"
#include "hub/http.hpp"
#include "hub/page_parts.hpp"

namespace ga = ge::account;
namespace gn = ge::net;
using namespace ge::hub;

namespace {

constexpr int64_t kNow_s = 1794500000;

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  s.score = "frags";
  s.stats = true;
  return s;
}

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

gn::ReportPlayer seat(const char* name, uint64_t account, int64_t frags, int64_t deaths,
                      int64_t ping, int64_t rl_attacks, int64_t rl_hits, int64_t rl_kills) {
  gn::ReportPlayer p;
  p.name = name;
  p.account = account;
  p.score = p.frags = frags;
  p.deaths = deaths;
  p.seconds = 600;
  p.ping_min = p.ping_avg = p.ping_max = ping;
  p.columns = {rl_attacks, rl_hits, rl_kills, 0, 10, 4, 1, 0, frags / 2};
  return p;
}

gn::MatchReport report(uint64_t id, const char* mode, const char* map, int64_t start_s) {
  gn::MatchReport r;
  r.id = id;
  r.game = "test_game";
  r.server = "Night Arena";
  r.mode = mode;
  r.map = map;
  r.start_s = start_s;
  r.duration_ms = 600000;
  r.columns = {"rl_attacks", "rl_hits",  "rl_kills",  "rl_deaths", "lg_attacks",
               "lg_hits",    "lg_kills", "lg_deaths", "streak_max"};
  return r;
}

struct Hub {
  ge::net::Transport port;  // never opened: the pages read the service's records directly
  ga::Service svc;
  uint64_t n = 1, ann = 0, bob = 0, cid = 0;
  gn::Key trusted{}, other{};
  Hub() {
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed.fill(3);
    ga::service_init(svc, c, std::move(store), port, counter_fresh, &n);
    ga::Fresh f{};
    f.fill(9);
    ann = ga::create(svc.store, "ann", "password one", kNow_s * 1000000, f).id;
    f.fill(10);
    bob = ga::create(svc.store, "bob", "password two", kNow_s * 1000000, f).id;
    f.fill(11);
    cid = ga::create(svc.store, "cid", "password six", kNow_s * 1000000, f).id;
    trusted.fill(1);
    other.fill(2);
    ga::matches_trust(svc.matches, trusted);
    // Four duels on arena and towers, an ffa, and a match from a server nobody listed.
    for (uint64_t i = 0; i < 4; i++) {
      gn::MatchReport r = report(0x100 + i, "1on1", i % 2 ? "towers" : "arena",
                                 kNow_s - static_cast<int64_t>(i) * 3 * 86400);
      r.players = {seat("ann", ann, 20, 10, i == 3 ? 150 : 30, 40, 16, 12),
                   seat("bob", bob, 10, 20, 60, 50, 10, 5)};
      ga::matches_add(svc.matches, trusted, r);
    }
    gn::MatchReport ffa = report(0x200, "ffa", "arena", kNow_s);
    ffa.players = {seat("ann", ann, 5, 30, 30, 20, 2, 1), seat("cid", cid, 30, 5, 30, 30, 12, 9)};
    ffa.players.push_back(seat("frogbot", 0, 8, 8, 0, 0, 0, 0));
    ffa.players.back().bot = true;
    ga::matches_add(svc.matches, trusted, ffa);
    gn::MatchReport odd = report(0x300, "1on1", "arena", kNow_s);
    odd.players = {seat("ann", ann, 99, 0, 30, 1, 1, 1)};
    ga::matches_add(svc.matches, other, odd);
  }
  Answer get(const std::string& target) {
    Answer a;
    CHECK_MSG(stats_answer(spec(), &svc, target, kNow_s, a), target);
    return a;
  }
};

bool has(const Answer& a, const std::string& text) {
  return a.body.find(text) != std::string::npos;
}

}  // namespace

TEST(stats_page_paths) {
  CHECK(stats_path("/player/ann") && stats_path("/match/0000000000000100"));
  CHECK(stats_path("/leaders") && stats_path("/v1/players/ann") && stats_path("/v1/matches"));
  CHECK(stats_path("/v1/matches/0000000000000100") && stats_path("/v1/leaders"));
  CHECK(!stats_path("/players") && !stats_path("/matches") && !stats_path("/v1/servers"));
  Answer a;
  CHECK(!stats_answer(spec(), nullptr, "/players", kNow_s, a));
  CHECK(url_text("a b/c") == "a%20b%2Fc");
}

TEST(stats_page_a_hub_without_accounts_says_so) {
  Answer a;
  CHECK(stats_answer(spec(), nullptr, "/player/ann", kNow_s, a));
  CHECK(a.code == 404 && has(a, "keeps no player stats"));
  CHECK(stats_answer(spec(), nullptr, "/v1/leaders", kNow_s, a));
  CHECK(a.code == 404 && a.type == "application/json" && has(a, "keeps no player stats"));
}

TEST(stats_page_player_sums_counted_matches_under_filters) {
  Hub h;
  const Answer all = h.get("/player/ann");
  CHECK(all.code == 200 && has(all, "<h2>ann</h2>"));
  CHECK_MSG(has(all, "<b>5</b><small>matches"), "four duels and the ffa; the unlisted one not");
  CHECK(has(all, "<b>85</b><small>frags"));
  CHECK(has(all, "Weapons") && has(all, "<td>rl</td><td>36.7%</td>"));  // 66 of 180
  CHECK(has(all, "streak (best)") && has(all, "Recent matches"));
  CHECK(all.body.find("<td>rl</td>") < all.body.find("<td>lg</td>"));  // by kills
  // The unlisted server's match is among the recent ones, but in no totals.
  CHECK(has(all, "/match/0000000000000300"));
  const Answer duels = h.get("/player/ann?mode=1on1");
  CHECK(has(duels, "<b>4</b><small>matches"));
  const Answer no_ffa = h.get("/player/ann?without=ffa");
  CHECK(has(no_ffa, "<b>4</b><small>matches") && has(no_ffa, "value=\"ffa\" checked"));
  const Answer arena = h.get("/player/ann?map=arena&without=ffa");
  CHECK(has(arena, "<b>2</b><small>matches"));
  const Answer fast = h.get("/player/ann?ping=40");
  CHECK_MSG(has(fast, "<b>4</b><small>matches"), "one duel was played at 150 ms");
  CHECK(!has(fast, "/match/0000000000000103"));
  const Answer none = h.get("/player/ann?map=e1m1");
  CHECK(has(none, "No matches counted"));
  const Answer json = h.get("/v1/players/ANN");
  CHECK(json.code == 200 && json.type == "application/json");
  CHECK(has(json, "\"name\":\"ann\"") && has(json, "\"matches\":5") &&
        has(json, "\"rl_hits\":66") && has(json, "\"counted\":false"));
}

TEST(stats_page_match_shows_scoreboard_and_accuracy) {
  Hub h;
  const Answer m = h.get("/match/0000000000000200");
  CHECK(m.code == 200 && has(m, "ffa on arena") && has(m, "Night Arena"));
  CHECK(has(m, "<a href=\"/player/cid\">cid</a>") && has(m, "frogbot <small class=\"bot\">bot"));
  CHECK(m.body.find("/player/cid") < m.body.find("/player/ann"));  // by score
  CHECK(has(m, "Accuracy") && has(m, "40.0% <small>9 kills</small>"));
  CHECK(!has(m, "counted in no player's stats"));
  CHECK(has(h.get("/match/0000000000000300"), "counted in no player's stats"));
  const Answer gone = h.get("/match/00000000000000ff");
  CHECK(gone.code == 404 && has(gone, "holds no match"));
  const Answer json = h.get("/v1/matches/0000000000000200");
  CHECK(has(json, "\"mode\":\"ffa\"") && has(json, "\"bot\":true") && has(json, "\"rl_kills\":9"));
  const Answer list = h.get("/v1/matches?player=bob");
  CHECK(has(list, "\"pages\":1") && has(list, "0000000000000100") &&
        !has(list, "0000000000000200"));
  CHECK(has(h.get("/v1/matches?mode=ffa"), "0000000000000200"));
}

TEST(stats_page_leaders_rank_by_stat_and_period) {
  Hub h;
  const Answer frags = h.get("/leaders");
  CHECK(frags.code == 200 && has(frags, "Leaders: frags"));
  CHECK(frags.body.find(">ann<") < frags.body.find(">bob<"));  // 85 before 40
  CHECK(frags.body.find(">bob<") < frags.body.find(">cid<"));  // 40 before 30
  const Answer acc = h.get("/leaders?stat=rl_acc");
  CHECK_MSG(!has(acc, ">cid<"), "cid played one match: too few for a ratio");
  CHECK(has(acc, "36.7%") && has(acc, "at least 3 matches"));
  const Answer duels = h.get("/leaders?stat=frags&mode=1on1");
  CHECK(!has(duels, ">cid<"));
  const Answer week = h.get("/v1/leaders?stat=matches&days=7");
  CHECK_MSG(has(week, "{\"rank\":1,\"name\":\"ann\",\"value\":4,\"matches\":4}"), week.body);
  const Answer col = h.get("/v1/leaders?stat=streak_max");
  CHECK(has(col, "\"name\":\"ann\""));
  CHECK(has(h.get("/leaders?stat=nothing_like_it"), "Nobody yet"));
}

TEST(stats_page_nav_links_leaders_only_with_stats) {
  HubSpec s = spec();
  CHECK(nav(s, "").find("/leaders") != std::string::npos);
  s.stats = false;
  CHECK(nav(s, "").find("/leaders") == std::string::npos);
}
