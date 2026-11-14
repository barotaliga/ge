// A TEAM GAME OF BOTS (games/shooters_edge/bots/team.hpp, net/classic_team.hpp): two teams of
// four fighters, each at the controls of a classic client of our server, play the baseline mod
// with `teamplay 1` on the fixture halls furnished for it (kTeamHallsEntities) for a minute. Both
// teams score; no player is ever fragged by a teammate; each team has a holder and rushes the quad;
// the players hold shots for teammates in the line; both teams talk (team messages, the quad's
// call among them, carried in the recording); a novice starts knowing none of the map's items and
// an expert all of them; the mod never faults; the multiview recording opens and its frames carry
// all eight players; and a match replays to the bit and matches its golden
// (tests/golden/shooters_edge_team.txt, GE_GOLDEN_WRITE=1 rewrites it; the PR says why).
//
// ge:summary Tests: a 4on4 of bots on teamplay: both score, no team kills, roles, the MVD, bits
// ge:keywords test team-game 4on4 teamplay roles hold rush mvd recording golden replay
// ge:keywords say_team team-messages map-knowledge novice expert
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>

#include "ai/why.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/bumps_read.hpp"
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

cn::TeamMatchReport match(int frames, uint32_t seed, bool record, int32_t drill = 0,
                          ge::ai::Why* why = nullptr) {
  cn::TeamMatchOptions o;
  o.why = why;
  o.drill = drill;
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
  // Shots and hits by weapon (net/classic_hits.hpp): a frag takes hits, and no weapon hits more
  // than it fired.
  int64_t hits = 0;
  for (const cn::TeamPlayer& p : r.players)
    for (int w = 0; w < cn::kHitWeapons; w++) {
      const cn::HitCount& c = p.hits[static_cast<size_t>(w)];
      CHECK_MSG(c.hit >= 0 && c.hit <= c.fired,
                p.name + "'s " + cn::hit_weapon_name(static_cast<cn::HitWeapon>(w)) + " hit " +
                    std::to_string(c.hit) + " of " + std::to_string(c.fired));
      hits += c.hit;
    }
  CHECK_MSG(hits > 0, "the match's frags were made with no hit counted");
  for (int t = 0; t < 2; t++) {
    CHECK_MSG(r.teams[t].frags >= 3,
              "team " + std::to_string(t) + " scored " + std::to_string(r.teams[t].frags));
    int64_t hold = 0;
    for (const cn::TeamPlayer& p : r.players)
      if (p.team == t) hold += p.role_commands[1];
    CHECK_MSG(hold > 0, "team " + std::to_string(t) + " never held its area");
  }
  CHECK_MSG(r.teams[0].rushes + r.teams[1].rushes > 0, "nobody rushed the quad");
  CHECK_MSG(r.hold_item >= 0 && r.quad_item >= 0, "the key areas were not found");
  for (const std::string& line : r.frag_log)
    CHECK_MSG(!team_kill(line), "a teammate fragged a teammate under teamplay: " + line);

  // Both teams talk, the quad among it, and the words are in the recording.
  for (int t = 0; t < 2; t++)
    CHECK_MSG(r.teams[t].messages > 0, "team " + std::to_string(t) + " said nothing");
  bool quad_call = false;
  for (const std::string& l : r.chat) quad_call |= l.find("quad") != std::string::npos;
  CHECK_MSG(quad_call, "nobody called the quad");
  if (!r.chat.empty()) {
    const std::string& first = r.chat.front();
    const std::string words = first.substr(first.find(": ") + 2);
    const std::string bytes(r.mvd.begin(), r.mvd.end());
    CHECK_MSG(bytes.find(words) != std::string::npos,
              "the team message '" + words + "' is not in the recording");
  }
  // Each reads his mates' lines by his own skill: an expert all of them, a novice few.
  for (const cn::TeamPlayer& p : r.players) {
    if (p.skill == ge::shooter::AimSkill::expert)
      CHECK_MSG(p.read > 0 && p.missed == 0, p.name + " (an expert) missed a team line");
    if (p.skill == ge::shooter::AimSkill::novice)
      CHECK_MSG(p.missed > p.read, p.name + " (a novice) read most of the team's lines");
  }
  // A novice starts the match knowing none of the items, an expert all of them.
  for (const cn::TeamPlayer& p : r.players) {
    if (p.known.empty()) continue;
    if (p.skill == ge::shooter::AimSkill::novice)
      CHECK_MSG(p.known.front() == 0, p.name + " (a novice) knew the map at the start");
    if (p.skill == ge::shooter::AimSkill::expert)
      CHECK_MSG(p.known.front() == static_cast<int16_t>(r.item_at.size()),
                p.name + " (an expert) did not know every item at the start");
  }

  // The recording opens and its frames carry every player.
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(r.mvd, d, error), error);
  int frames = 0;
  std::set<int> seen;
  std::set<std::string> teams;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    frames++;
    for (size_t k = 0; k < d.state.players.size(); k++)
      if (d.state.players[k].present) {
        seen.insert(static_cast<int>(k));
        CHECK_MSG(!d.state.names[k].empty(), "player " + std::to_string(k) + " has no name");
        teams.insert(d.state.teams[k]);
      }
  }
  // Its players' names and teams, so a viewer names each and knows his side.
  CHECK_MSG(teams.size() == 2, "the recording shows " + std::to_string(teams.size()) + " teams");
  CHECK_MSG(frames > 4000, "the recording has " + std::to_string(frames) + " frames");
  CHECK_MSG(seen.size() == 8, "the recording shows " + std::to_string(seen.size()) + " players");
}

TEST(a_weapon_drill_fires_only_its_weapon) {
  if (!halls().ok) return;
  // The double scattergun alone (the mod's bit 2), its shells topped up: no other weapon fires,
  // and the double does, in pellets of 14.
  const cn::TeamMatchReport r = match(2300, 2, false, 2);
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  int64_t doubles = 0;
  for (const cn::TeamPlayer& p : r.players)
    for (int w = 0; w < cn::kHitWeapons; w++) {
      const cn::HitCount& c = p.hits[static_cast<size_t>(w)];
      if (w == static_cast<int>(cn::HitWeapon::dbl)) {
        doubles += c.fired;
        CHECK_MSG(c.fired % 14 == 0, p.name + "'s double fired " + std::to_string(c.fired) +
                                         " pellets, not a whole number of shots");
      } else {
        CHECK_MSG(c.fired == 0, p.name + "'s " +
                                    cn::hit_weapon_name(static_cast<cn::HitWeapon>(w)) + " fired " +
                                    std::to_string(c.fired) + " in a drill of the double");
      }
    }
  CHECK_MSG(doubles > 0, "no double was fired in its drill");
}

// The decision record decides nothing (engine/ai/why.hpp): a match with every fighter's decisions
// recorded (se-team --why's decision log) plays to the same bits as one without.
TEST(a_team_game_recorded_for_its_decisions_plays_the_same) {
  CHECK_MSG(halls().ok, halls().error);
  if (!halls().ok) return;
  ge::ai::Why why;
  const cn::TeamMatchReport off = match(600, 5, false), on = match(600, 5, false, 0, &why);
  CHECK_MSG(off.ok && on.ok, off.error + on.error);
  CHECK_MSG(off.digest == on.digest,
            "a team game with its decisions recorded played differently: a fighter branches on "
            "its Why; compute the choice first, record it after (the bots skill, the rule)");
  CHECK_MSG(!why.decisions().empty(), "no fighter recorded a decision on the Why it was handed");
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

// The bumps read off a recording (labs/bumps_read.hpp): a run stopped dead against a wall is a wall
// bump; the same stop in the open, or with health lost, or touching another player, is not one.
TEST(the_bumps_read_off_motion_tell_a_wall_from_a_stop) {
  namespace la = shooters_edge::labs;
  namespace sh = ge::shooter;
  const sh::BoxHull box = sh::make_box_hull({100, -500, -100}, {200, 500, 100});
  const sh::Hull wall = box.hull();
  const auto run = [&](float stop_x, float y, float hit, bool other) {
    la::BumpState b;
    la::BumpsRead r;
    la::Bump last = la::Bump::none;
    double t = 0;
    float x = stop_x - 60;
    for (int i = 0; i < 9; i++) {  // 300 u/s at 30 updates a second, then stopped dead
      const bool stopped = x >= stop_x;
      const float health = stopped ? 100 - hit : 100;
      const la::Bump k = la::bumps_update(b, r, wall, t, {x, y, 0}, true, health, other);
      if (k != la::Bump::none) last = k;
      t += 1.0 / 30;
      x = x + 10 < stop_x ? x + 10 : stop_x;
    }
    return std::make_pair(last, r);
  };
  const auto [at_wall, r1] = run(99, 0, 0, false);
  CHECK_MSG(at_wall == la::Bump::wall && r1.wall == 1, "a run stopped against the wall: one bump");
  CHECK_MSG(r1.fast_s > 0 && r1.live_s > r1.fast_s, "the time at speed is part of the time read");
  CHECK_MSG(run(99, 900, 0, false).first == la::Bump::none, "the same stop in the open: none");
  CHECK_MSG(run(99, 0, 20, false).first == la::Bump::none, "a hit's knock: no bump");
  CHECK_MSG(run(99, 0, 0, true).first == la::Bump::player, "another player there: his bump");
}
