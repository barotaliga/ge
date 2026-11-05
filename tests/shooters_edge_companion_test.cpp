// THE CO-OP COMPANION (games/shooters_edge/bots/companion.hpp, coop_run.hpp; plugins/shooter/
// idle.hpp): our campaign in coop on the coop hall, a scripted leader walking his tour and an
// expert companion beside him. The companion keeps to him (near him most of the run, never left
// far behind), presses the button that opens the shut door he waits by (he never presses it), kills
// the monsters that come for him once he walks into their room and none before they wake, and
// while he stands still with nothing about it fidgets (several kinds of idle act) and stops when he
// moves on. Co-op spares one player another's blows under teamplay, never his own. The run replays
// to the bit and matches its golden (tests/golden/shooters_edge_companion.txt, GE_GOLDEN_WRITE=1
// rewrites it; the PR says why).
//
// ge:summary Tests: the co-op companion follows, presses the button, defends, idles; replays
// ge:keywords test companion co-op coop follow defend button idle bored replay teamplay golden
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/idle.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge_campaign_fixture.hpp"

namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_companion.txt";

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

fb::CoopOptions options(uint32_t seed) {
  fb::CoopOptions o;
  o.root = GE_SOURCE_DIR;
  o.seed = seed;
  o.seconds = 60;
  return o;
}

const fb::CoopReport& run_once() {
  static const fb::CoopReport r = fb::run_coop(options(1));
  return r;
}

int64_t in_mode(const fb::CoopMate& m, fb::EscortMode mode) {
  return m.escort.commands[static_cast<int>(mode)];
}

// The first time from `from` the leader's leg is `leg`, s (-1: never).
double leg_starts(const fb::CoopReport& r, int leg) {
  for (size_t i = 0; i < r.leg.size(); i++)
    if (r.leg[i] == leg) return r.t[i];
  return -1;
}

}  // namespace

TEST(the_run_plays_with_no_fault) {
  const fb::CoopReport& r = run_once();
  CHECK_MSG(r.ok, r.error);
  CHECK(r.faults == 0);
  CHECK(r.mates.size() == 1);
  CHECK(r.monsters_total == 3);
}

TEST(the_companion_keeps_to_the_leader) {
  const fb::CoopReport& r = run_once();
  if (!r.ok) return;
  const fb::CoopMate& m = r.mates[0];
  int near = 0;
  float farthest = 0;
  for (float d : m.dist) {
    near += d < fb::kEscortFar ? 1 : 0;
    farthest = d > farthest ? d : farthest;
  }
  const double share = static_cast<double>(near) / static_cast<double>(m.dist.size());
  std::printf("near share %.3f, farthest %.0f\n", share, farthest);
  CHECK_MSG(share > 0.8, "the companion was near the leader less than four fifths of the run");
  CHECK_MSG(farthest < fb::kEscortCatchUp, "the companion fell further behind than its catch-up");
  CHECK(m.deaths == 0);
}

TEST(the_companion_presses_the_button_the_leader_waits_by) {
  const fb::CoopReport& r = run_once();
  if (!r.ok) return;
  const double waits = leg_starts(r, 2);  // "waits by the door"
  std::printf("waits by the door at %.2f s, button %.2f s, door %.2f s\n", waits,
              r.button_pressed_at, r.door_open_at);
  CHECK_MSG(r.button_pressed_at > 0, "nobody pressed the button");
  CHECK_MSG(r.button_pressed_at >= waits, "the button went before the leader stopped by it");
  CHECK_MSG(r.button_pressed_at < waits + 5, "the leader waited 5 s and more on the button");
  CHECK(r.door_open_at >= r.button_pressed_at);
  CHECK(r.mates[0].escort.buttons == 1);
}

TEST(the_companion_kills_what_comes_for_the_leader_and_nothing_before) {
  const fb::CoopReport& r = run_once();
  if (!r.ok) return;
  const fb::CoopMate& m = r.mates[0];
  CHECK_MSG(static_cast<int>(r.kills.size()) == r.monsters_total, "a monster outlived the run");
  CHECK_MSG(m.kills == r.monsters_total, "a monster fell outside the companion's sight");
  for (const fb::CoopKill& k : r.kills) {
    std::printf("killed %d at %.2f s\n", k.ent, k.t);
    CHECK_MSG(k.t > r.door_open_at, "a monster fell before the door to its room opened");
  }
  CHECK(in_mode(m, fb::EscortMode::defend) > 0);
  CHECK(r.leader_deaths == 0);
}

TEST(the_companion_idles_like_a_person_while_he_waits) {
  const fb::CoopReport& r = run_once();
  if (!r.ok) return;
  const fb::CoopMate& m = r.mates[0];
  int kinds = 0;
  for (int k = 1; k < sh::kIdleActs; k++) {
    std::printf("  %s %lld\n", sh::idle_act_name(static_cast<sh::IdleAct>(k)),
                static_cast<long long>(m.idle.acts[k]));
    kinds += m.idle.acts[k] > 0 ? 1 : 0;
  }
  CHECK_MSG(kinds >= 3, "fewer than three kinds of idle act");
  CHECK(m.escort.idle_breaks >= 1);
  // Bored only while he stands: never a second after his tour has him walking on.
  size_t walking_since = 0;
  for (size_t i = 0; i < r.t.size(); i++) {
    const bool walking = fb::kCoopTour[r.leg[i]].walk;
    if (!walking) walking_since = i + 1;
    if (walking && i >= walking_since + 72 &&
        m.mode[i] == static_cast<uint8_t>(fb::EscortMode::bored)) {
      CHECK_MSG(false, "still bored a second after the leader walked on, at " +
                           std::to_string(r.t[i]) + " s");
      break;
    }
  }
}

TEST(coop_under_teamplay_spares_the_other_player_but_not_oneself) {
  campfix::Game g(R"({
"classname" "info_player_coop"
"origin" "-416 96 24"
}
)",
                  {"deathmatch 0", "coop 1", "teamplay 1"});
  shooters_edge::server::campaign_join(g.c, 1, "mate");
  const float before = g.field(2, g.f().health);
  CHECK(g.hit(2, 1, 30));
  CHECK_MSG(g.field(2, g.f().health) == before, "a coop player's blow harmed the other");
  const float own = g.field(1, g.f().health);
  CHECK(g.hit(1, 1, 30));
  CHECK_MSG(g.field(1, g.f().health) < own, "a coop player's own blow spared him");
}

TEST(coop_without_teamplay_lets_players_harm_each_other) {
  campfix::Game g(R"({
"classname" "info_player_coop"
"origin" "-416 96 24"
}
)",
                  {"deathmatch 0", "coop 1", "teamplay 0"});
  shooters_edge::server::campaign_join(g.c, 1, "mate");
  const float before = g.field(2, g.f().health);
  CHECK(g.hit(2, 1, 30));
  CHECK(g.field(2, g.f().health) < before);
}

TEST(a_coop_run_replays_to_the_bit_and_matches_its_golden) {
  const fb::CoopReport& a = run_once();
  const fb::CoopReport b = fb::run_coop(options(1));
  CHECK_MSG(a.ok && b.ok, a.error + b.error);
  CHECK_MSG(a.final_digest == b.final_digest, "two coop runs on one seed differ");
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/bots/coop_run.hpp: the leader's tour of the coop hall with an "
           "expert companion, seed 1, 60 s: the last frame's digest "
           "(tests/shooters_edge_companion_test.cpp)\n";
    char line[32];
    std::snprintf(line, sizeof line, "%016llx\n", static_cast<unsigned long long>(a.final_digest));
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 1,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() == 1 && rows[0].size() == 1)
    CHECK_MSG(rows[0][0] == a.final_digest,
              "the coop run moved off its golden bits: if the companion, the fighter, the bot, the "
              "campaign mod or the move changed on purpose, rewrite with GE_GOLDEN_WRITE=1 and say "
              "why in the PR");
}
