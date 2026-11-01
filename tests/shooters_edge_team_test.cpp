// A TEAM GAME OF BOTS (games/shooters_edge/bots/team.hpp, net/classic_team.hpp): two teams of
// four fighters, each at the controls of a classic client of our server, play the baseline mod
// with `teamplay 1` on the fixture halls furnished for it (kTeamHallsEntities) for a minute. Both
// teams score; no player is ever fragged by a teammate; each team has a holder and rushes the quad;
// the players hold shots for teammates in the line; the mod never faults; the multiview recording
// opens and its frames carry all eight players; and a match replays to the bit and matches its
// golden (tests/golden/shooters_edge_team.txt, GE_GOLDEN_WRITE=1 rewrites it; the PR says why).
//
// ge:summary Tests: a 4on4 of bots on teamplay: both score, no team kills, roles, the MVD, bits
// ge:keywords test team-game 4on4 teamplay roles hold rush mvd recording golden replay
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>

#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_team.txt";

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

struct Halls {
  std::vector<uint8_t> bsp;
  ge::progs::Progs progs;
  std::string error;
  bool ok = false;
  Halls() {
    ok = ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error) &&
         cl::baseline_progs(progs, error);
  }
};

const Halls& halls() {
  static const Halls h;
  return h;
}

cn::TeamMatchReport match(int frames, uint32_t seed, bool record) {
  cn::TeamMatchOptions o;
  o.map = halls().bsp;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &halls().progs;
  o.frames = frames;
  o.seed = seed;
  o.record = record;
  return cn::run_team_match(o);
}

// The frag log's line: \frag\<killer>\<killed>\<killer's team>\<killed's team>\...
bool team_kill(const std::string& line) {
  std::vector<std::string> f;
  size_t at = 0;
  while (at <= line.size()) {
    const size_t next = line.find('\\', at);
    f.push_back(line.substr(at, next == std::string::npos ? std::string::npos : next - at));
    if (next == std::string::npos) break;
    at = next + 1;
  }
  return f.size() > 5 && f[2] != f[3] && !f[4].empty() && f[4] == f[5];
}

}  // namespace

TEST(a_team_game_of_bots_plays_as_teams_and_records) {
  CHECK_MSG(halls().ok, halls().error);
  if (!halls().ok) return;
  const cn::TeamMatchReport r = match(4615, 1, true);  // a minute
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  CHECK_MSG(r.faults == 0, "the baseline mod faulted under the team game");
  CHECK_MSG(r.players.size() == 8, "eight players");
  for (const cn::TeamPlayer& p : r.players) CHECK_MSG(p.slot >= 0, p.name + " never got a slot");
  for (int t = 0; t < 2; t++) {
    CHECK_MSG(r.teams[t].frags >= 3,
              "team " + std::to_string(t) + " scored " + std::to_string(r.teams[t].frags));
    CHECK_MSG(r.teams[t].plans > 50, "team " + std::to_string(t) + " never planned");
    int64_t hold = 0;
    for (const cn::TeamPlayer& p : r.players)
      if (p.team == t) hold += p.role_commands[1];
    CHECK_MSG(hold > 0, "team " + std::to_string(t) + " never held its area");
  }
  CHECK_MSG(r.teams[0].rushes + r.teams[1].rushes > 0, "nobody rushed the quad");
  CHECK_MSG(r.hold_item >= 0 && r.quad_item >= 0, "the key areas were not found");
  for (const std::string& line : r.frag_log)
    CHECK_MSG(!team_kill(line), "a teammate fragged a teammate under teamplay: " + line);

  // The recording opens and its frames carry every player.
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(r.mvd, d, error), error);
  int frames = 0;
  std::set<int> seen;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    frames++;
    for (size_t k = 0; k < d.state.players.size(); k++)
      if (d.state.players[k].present) seen.insert(static_cast<int>(k));
  }
  CHECK_MSG(frames > 4000, "the recording has " + std::to_string(frames) + " frames");
  CHECK_MSG(seen.size() == 8, "the recording shows " + std::to_string(seen.size()) + " players");
}

TEST(a_team_game_replays_to_the_bit_and_matches_its_golden) {
  CHECK_MSG(halls().ok, halls().error);
  if (!halls().ok) return;
  uint64_t a = 0, b = 0;
  for (int run = 0; run < 2; run++) {
    const cn::TeamMatchReport r = match(1500, 3, run == 0);
    CHECK_MSG(r.ok, r.error);
    (run == 0 ? a : b) = r.digest;
  }
  CHECK_MSG(a == b, "two team games on one seed differ (one recorded, one not)");
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/net/classic_team.hpp: 4on4 of fighters on the fixture halls, "
           "teamplay 1, seed 3, 1500 frames: the match's digest "
           "(tests/shooters_edge_team_test.cpp)\n";
    char line[32];
    std::snprintf(line, sizeof line, "%016llx\n", static_cast<unsigned long long>(a));
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 1,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() == 1 && rows[0].size() == 1)
    CHECK_MSG(rows[0][0] == a,
              "the team game moved off its golden bits: if the fighter, the team, the mod or the "
              "server changed on purpose, rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
}
