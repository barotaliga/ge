// Tests: a finished match's report (engine/net/report.hpp): written and read back field for
// field, a report past its limits refused by the writer, damage refused by the reader (every cut
// short, a count past its limit, a byte too many, another version).
//
// ge:summary Tests: the match report's bytes round trip, its limits and damage refused
// ge:keywords test match-report report_write report_read
// ge:skill net testing
#include "net/report.hpp"

#include <string>
#include <vector>

#include "check.hpp"

namespace gn = ge::net;

namespace {

gn::MatchReport sample() {
  gn::MatchReport r;
  r.id = 0x1234567890abcdefull;
  r.game = "shooters_edge";
  r.version = "1";
  r.server = "Community Night";
  r.address = "203.0.113.7:27500";
  r.mode = "2on2";
  r.map = "arena";
  r.demo_sha256 = std::string(64, 'a');
  r.start_s = 1794500000;
  r.duration_ms = 600000;
  r.ended = gn::MatchEnd::limit;
  r.columns = {"rl_hits", "damage_given"};
  r.teams = {{"red", 31, 4}, {"blue", -2, 13}};
  gn::ReportPlayer a;
  a.name = "Niclas";
  a.team = "red";
  a.account = 1;
  a.score = 30;
  a.frags = 31;
  a.deaths = 9;
  a.suicides = 1;
  a.seconds = 600;
  a.columns = {12, 4210};
  gn::ReportPlayer b;
  b.name = "bot1";
  b.team = "blue";
  b.bot = true;
  b.score = -2;
  b.deaths = 31;
  b.team_kills = 2;
  b.seconds = 580;
  b.columns = {0, 15};
  r.players = {a, b};
  return r;
}

}  // namespace

TEST(report_round_trip_field_for_field) {
  const gn::MatchReport r = sample();
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(gn::report_write(r, bytes, error), error);
  CHECK(bytes.size() < 400);
  gn::MatchReport back;
  CHECK_MSG(gn::report_read(bytes.data(), bytes.size(), back, error), error);
  CHECK(back.id == r.id && back.game == r.game && back.server == r.server &&
        back.address == r.address && back.mode == r.mode && back.map == r.map &&
        back.demo_sha256 == r.demo_sha256 && back.start_s == r.start_s &&
        back.duration_ms == r.duration_ms && back.ended == r.ended && back.columns == r.columns);
  CHECK(back.teams.size() == 2 && back.teams[1].name == "blue" && back.teams[1].score == -2 &&
        back.teams[1].colour == 13);
  CHECK(back.players.size() == 2);
  const gn::ReportPlayer& b = back.players[1];
  CHECK(b.name == "bot1" && b.team == "blue" && b.bot && b.account == 0 && b.score == -2 &&
        b.deaths == 31 && b.team_kills == 2 && b.seconds == 580);
  CHECK(back.players[0].columns == std::vector<int64_t>({12, 4210}));
  CHECK(std::string(gn::match_end_name(gn::MatchEnd::forfeit)) == "forfeit");
}

TEST(report_past_its_limits_is_refused_never_cut) {
  gn::MatchReport r = sample();
  r.players.resize(gn::kReportPlayers + 1, r.players[0]);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(!gn::report_write(r, bytes, error));
  CHECK(error.find("at most") != std::string::npos);
  r = sample();
  r.players[0].columns.pop_back();
  error.clear();
  CHECK(!gn::report_write(r, bytes, error));
  CHECK(error.find("one value per column") != std::string::npos);
}

TEST(report_damage_is_refused) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(gn::report_write(sample(), bytes, error));
  gn::MatchReport out;
  for (size_t n = 0; n < bytes.size(); n++) {
    error.clear();
    CHECK(!gn::report_read(bytes.data(), n, out, error));
    CHECK(!error.empty());
  }
  std::vector<uint8_t> more = bytes;
  more.push_back(0);
  CHECK(!gn::report_read(more.data(), more.size(), out, error));
  CHECK(error.find("past its end") != std::string::npos);
  std::vector<uint8_t> other = bytes;
  other[4] = 9;
  CHECK(!gn::report_read(other.data(), other.size(), out, error));
  CHECK(error.find("version 9") != std::string::npos);
}
