// Shooter's Edge's match tally and report (games/shooters_edge/net/classic_report.hpp): a death, a
// suicide and a team kill told apart off the rules' world, a player who left kept in the report,
// the teams' scores summed, how the match ended; and a bot match on the server reporting once at
// its time limit, the same report on every run.
//
// ge:summary Tests the classic match tally and the report a bot match leaves on the server
// ge:keywords match-report match-tally classic-report suicides team-kills
// ge:skill net shooter
#include <array>
#include <string>

#include "check.hpp"
#include "net/report.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_report.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace gn = ge::net;
namespace se = shooters_edge::net;
namespace sv = shooters_edge::server;

namespace {

void seat(sv::RulesWorld& w, int slot, const char* name, const char* team, int userid) {
  sv::WorldClient& c = w.clients[slot];
  c.active = true;
  c.userid = userid;
  c.name = name;
  c.userinfo = {{"name", name}, {"team", team}, {"bottomcolor", slot < 2 ? "4" : "13"}};
  c.numbers.health = 100;
}

const gn::ReportPlayer* find(const gn::MatchReport& r, const std::string& name) {
  for (const gn::ReportPlayer& p : r.players)
    if (p.name == name) return &p;
  return nullptr;
}

}  // namespace

TEST(classic_report_tells_a_death_a_suicide_and_a_team_kill_apart) {
  sv::RulesWorld w;
  w.map_name = "arena";
  seat(w, 0, "ann", "red", 1);
  seat(w, 1, "bo", "red", 2);
  seat(w, 2, "cy", "blue", 3);
  seat(w, 3, "di", "blue", 4);
  std::array<uint64_t, ge::shooter::kMaxClients> accounts{};
  accounts[0] = 77;
  se::MatchTally t;
  w.time = 1;
  se::tally_frame(t, w, true, accounts);
  auto step = [&](double at) {
    w.time = at;
    se::tally_frame(t, w, true, accounts);
  };
  // ann kills cy; cy dies.
  w.clients[0].numbers.frags = 1;
  w.clients[2].numbers.health = -10;
  step(2);
  w.clients[2].numbers.health = 100;  // cy respawns
  step(3);
  // bo kills themself: dead, a frag down.
  w.clients[1].numbers.health = 0;
  w.clients[1].numbers.frags = -1;
  step(4);
  w.clients[1].numbers.health = 100;
  step(5);
  // di shoots cy: cy dies, di loses a frag while alive.
  w.clients[3].numbers.frags = -1;
  w.clients[2].numbers.health = -5;
  step(6);
  w.clients[2].numbers.health = 100;
  step(7);
  // di leaves; the slot is empty.
  w.clients[3] = {};
  step(8);
  // ann reaches the limit.
  w.clients[0].numbers.frags = 3;
  step(61);
  se::TallyFacts f;
  f.mode = se::classic_mode(true, false, 4);
  f.teams = true;
  f.fraglimit = 3;
  gn::MatchReport r;
  CHECK(se::tally_end(t, w, f, r));
  CHECK(!se::tally_end(t, w, f, r));  // once
  CHECK(r.mode == "2on2");
  CHECK(r.map == "arena");
  CHECK(r.duration_ms == 60000);
  CHECK(r.ended == gn::MatchEnd::limit);
  CHECK(r.players.size() == 4);
  CHECK(r.players[0].name == "ann");  // the best first
  const gn::ReportPlayer* ann = find(r, "ann");
  const gn::ReportPlayer* bo = find(r, "bo");
  const gn::ReportPlayer* cy = find(r, "cy");
  const gn::ReportPlayer* di = find(r, "di");
  CHECK(ann && bo && cy && di);
  if (!(ann && bo && cy && di)) return;
  CHECK(ann->frags == 3 && ann->deaths == 0 && ann->account == 77);
  CHECK(bo->frags == -1 && bo->deaths == 1 && bo->suicides == 1 && bo->team_kills == 0);
  CHECK(cy->deaths == 2 && cy->suicides == 0);
  CHECK(di->team_kills == 1 && di->deaths == 0 && di->seconds == 6);  // left at 7 s
  CHECK(ann->team == "red" && ann->seconds == 60);
  CHECK(r.teams.size() == 2);
  CHECK(r.teams[0].name == "red" && r.teams[0].score == 2 && r.teams[0].colour == 4);
  CHECK(r.teams[1].name == "blue" && r.teams[1].score == -1 && r.teams[1].colour == 13);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(gn::report_write(r, bytes, error), error);
}

TEST(classic_report_a_side_gone_is_a_forfeit_and_a_warmup_counts_nothing) {
  sv::RulesWorld w;
  w.map_name = "arena";
  seat(w, 0, "ann", "", 1);
  seat(w, 1, "bo", "", 2);
  std::array<uint64_t, ge::shooter::kMaxClients> accounts{};
  se::MatchTally t;
  se::tally_frame(t, w, false, accounts);  // the warmup
  w.clients[0].numbers.frags = 5;
  w.time = 5;
  se::tally_frame(t, w, false, accounts);
  se::TallyFacts f;
  f.mode = se::classic_mode(false, false, 2);
  gn::MatchReport r;
  CHECK(!se::tally_end(t, w, f, r));  // nothing counted
  w.time = 0;                         // the match restarts the map
  w.clients[0].numbers.frags = 0;
  se::tally_frame(t, w, true, accounts);
  w.clients[0].numbers.frags = 2;
  w.time = 30;
  se::tally_frame(t, w, true, accounts);
  w.clients[1] = {};  // bo leaves
  w.time = 31;
  se::tally_frame(t, w, true, accounts);
  CHECK(se::tally_end(t, w, f, r));
  CHECK(r.mode == "1on1");
  CHECK(r.ended == gn::MatchEnd::forfeit);
  CHECK(r.players.size() == 2 && r.players[0].name == "ann" && r.players[0].frags == 2);
  CHECK(r.teams.empty());
}

// The server's own: three bots play out a 20 s match offline; the core keeps its report when
// the map changes after the intermission.
gn::MatchReport bot_match(int& reports) {
  ge::server::ServerConfig c;
  c.hostname = "Report Night";
  c.map = "arena";
  c.maxclients = 8;
  c.bots = 3;
  c.seed = 5;
  c.rules["timelimit"] = "0.34";
  ge::server::Server s;
  std::string error;
  CHECK_MSG(ge::server::server_open(s, se::classic_game(), c, nullptr, nullptr, error), error);
  for (int f = 0; f < 2600; f++) ge::server::server_frame(s);  // 34 s
  reports = static_cast<int>(s.reports.size());
  return s.reports.empty() ? gn::MatchReport{} : s.reports.back();
}

TEST(classic_report_a_bot_match_reports_once_at_its_time_limit) {
  int count = 0;
  const gn::MatchReport r = bot_match(count);
  CHECK_MSG(count == 1, std::to_string(count) + " reports");
  CHECK(r.server == "Report Night");
  CHECK(r.map == "arena");
  CHECK(r.mode == "ffa");
  CHECK_MSG(r.ended == gn::MatchEnd::limit, gn::match_end_name(r.ended));
  CHECK_MSG(r.duration_ms > 19000 && r.duration_ms < 21000, std::to_string(r.duration_ms));
  CHECK(r.players.size() == 3);
  int64_t frags = 0, deaths = 0;
  for (const gn::ReportPlayer& p : r.players) {
    CHECK(p.bot);
    CHECK(p.account == 0);
    frags += p.frags;
    deaths += p.deaths;
  }
  CHECK_MSG(deaths > 0, "the bots fought: " + std::to_string(frags) + " frags, " +
                            std::to_string(deaths) + " deaths");
  int again_count = 0;
  const gn::MatchReport again = bot_match(again_count);
  std::vector<uint8_t> a, b;
  std::string error;
  CHECK(gn::report_write(r, a, error) && gn::report_write(again, b, error));
  CHECK(a == b);  // the same match, the same report (the clock is the server's own)
}
