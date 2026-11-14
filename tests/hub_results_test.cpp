// engine/hub/results.hpp: a classic server's match seen ending in its status records (an
// intermission with the final scores, a return to warmup, another map, a restart) is kept as a
// result; a match without two scoring players is not; the length is known only when the start was
// seen; the oldest leave past kResultsKept; the Results page and its JSON say the same, filter by
// server, and a hub that reads no classic server says so.
//
// ge:summary Test: the hub's classic results: a match seen ending, kept, and its page and JSON
// ge:keywords test hub results classic adapter match-end results_see results_answer
// ge:skill net testing
#include "hub/results.hpp"

#include <string>

#include "check.hpp"

using namespace ge::hub;
namespace gn = ge::net;

namespace {

constexpr int64_t kUnix = 1800000000;

gn::StatusPlayer player(const char* name, int32_t score, const char* team = "", bool spec = false) {
  gn::StatusPlayer p;
  p.name = name;
  p.score = score;
  p.team = team;
  p.ping_ms = 25;
  p.flags = spec ? gn::kPlayerSpectator : 0;
  return p;
}

gn::Status playing(const char* map, int32_t a, int32_t b) {
  gn::Status s;
  s.name = "Old Fort";
  s.map = map;
  s.mode = "duel";
  s.phase = gn::StatusPhase::playing;
  s.players = {player("ann", a), player("bob", b), player("zed", 0, "", true)};
  return s;
}

gn::Address addr(uint16_t port) {
  gn::Address a;
  gn::parse_address("192.0.2.5:" + std::to_string(port), a);
  return a;
}

bool has(const Answer& a, const std::string& t) { return a.body.find(t) != std::string::npos; }

}  // namespace

TEST(results_kept_when_a_match_ends) {
  Results r;
  // To an intermission: the intermission's (final) scores are kept.
  gn::Status end = playing("dm4", 21, 17);
  end.phase = gn::StatusPhase::intermission;
  CHECK(results_see(r, addr(27500), playing("dm4", 20, 17), 0, end, 600000000, kUnix));
  CHECK(r.kept.size() == 1 && r.kept[0].players.size() == 2);  // the spectator left out
  CHECK(r.kept[0].players[0].name == "ann" && r.kept[0].players[0].score == 21);
  CHECK(r.kept[0].length_s == 600 && r.kept[0].end_s == kUnix && r.kept[0].id == 1);
  // Another map while playing: the last playing scores, the start unknown.
  CHECK(results_see(r, addr(27500), playing("dm4", 3, 9), -1, playing("dm6", 0, 0), 1, kUnix));
  CHECK(r.kept[1].map == "dm4" && r.kept[1].players[0].name == "bob" && r.kept[1].length_s == -1);
  // A restart on the same map: every score lower.
  CHECK(results_see(r, addr(27500), playing("dm6", 10, 8), 0, playing("dm6", 0, 1), 1, kUnix));
  // Back to warmup.
  gn::Status warm = playing("dm6", 0, 0);
  warm.phase = gn::StatusPhase::lobby;
  CHECK(results_see(r, addr(27500), playing("dm6", 4, 2), 0, warm, 1, kUnix));
  CHECK(r.seen == 4);
}

TEST(results_not_kept_without_an_end_or_a_match) {
  Results r;
  // Scores going up: still the same match.
  CHECK(!results_see(r, addr(1), playing("dm4", 3, 2), 0, playing("dm4", 5, 2), 1, kUnix));
  // One player scoring is no match.
  gn::Status end = playing("dm4", 0, 0);
  end.phase = gn::StatusPhase::intermission;
  CHECK(!results_see(r, addr(1), playing("dm4", 7, 0), 0, end, 1, kUnix));
  // A record that was not playing ends nothing.
  gn::Status warm = playing("dm4", 5, 5);
  warm.phase = gn::StatusPhase::lobby;
  CHECK(!results_see(r, addr(1), warm, 0, playing("dm6", 0, 0), 1, kUnix));
  CHECK(r.kept.empty());
}

TEST(results_oldest_leave_first) {
  Results r;
  for (size_t i = 0; i < kResultsKept + 5; i++)
    results_see(r, addr(1), playing("dm4", 2, 1), 0, playing("dm6", 0, 0), 1, kUnix);
  CHECK(r.kept.size() == kResultsKept && r.kept.front().id == 6 && r.seen == kResultsKept + 5);
}

TEST(results_page_and_json) {
  HubSpec spec;
  spec.game = "Test Game";
  spec.results = true;
  Results r;
  gn::Status end = playing("dm4", 21, 17);
  end.phase = gn::StatusPhase::intermission;
  results_see(r, addr(27500), playing("dm4", 20, 17), 0, end, 754000000, kUnix);
  gn::Status team = playing("e1m2", 0, 0);
  team.players = {player("cid", 30, "red"), player("dee", 12, "blue")};
  team.teams = {{"red", 30, 0}, {"blue", 12, 0}};
  results_see(r, addr(27501), team, -1, playing("e1m3", 0, 0), 1, kUnix + 60);
  Answer a;
  CHECK(results_answer(spec, &r, "/results", kUnix + 3600, a));
  CHECK(a.code == 200 && has(a, "Results") && has(a, "dm4") && has(a, "12:34"));
  CHECK(has(a, "1 h ago") && has(a, "/results?server=192.0.2.5:27500"));
  CHECK_MSG(a.body.find("e1m2") < a.body.find("dm4"), "newest first");
  CHECK(results_answer(spec, &r, "/results?server=192.0.2.5:27501", kUnix, a));
  CHECK(has(a, "e1m2") && !has(a, "dm4"));
  CHECK(results_answer(spec, &r, "/v1/results", kUnix, a));
  CHECK(has(a, "\"map\":\"e1m2\"") && has(a, "\"teams\":[{\"name\":\"red\",\"score\":30}"));
  CHECK(has(a, "\"length\":754") && has(a, "\"name\":\"ann\",\"team\":\"\",\"score\":21"));
  CHECK(!results_answer(spec, &r, "/leaders", kUnix, a));
  CHECK(results_answer(spec, nullptr, "/results", kUnix, a) && a.code == 404);
  CHECK(has(a, "reads no classic servers"));
  CHECK(results_answer(spec, nullptr, "/v1/results", kUnix, a) && a.code == 404);
}
