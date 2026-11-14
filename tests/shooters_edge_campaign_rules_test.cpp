// Tests: the campaign in C++ (games/shooters_edge/rules/campaign*.cpp) plays the co-op runs the
// compiled QC campaign played before it was deleted: the coop hall (its door, button, items and
// monsters) with companions fighting beside a scripted leader. The numbers were the QC's, taken
// while the two ran side by side and agreed frame by frame (every frame's digest of the loop's
// state, every player's origin, view and health; the kills and who made them, the door, the
// button, the exit); a change that moves one is a change to the rules.
//
// ge:summary Tests: the campaign's rules in C++ play the QC campaign's co-op runs to the bit,
// ge:summary pinned
// ge:keywords test campaign native-campaign parity coop-run campaign_mod loop-digest pinned
// ge:skill progs shooter testing
#include <cmath>
#include <cstdint>
#include <string>

#include "check.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/server/baseline_mod.hpp"

namespace sb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

const pg::Progs& native() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    if (!sv::campaign_mod(out, error)) out = {};
    return out;
  }();
  return p;
}

struct Pin {
  size_t frames, kills;
  int monsters, button_ms, door_ms, leader_deaths;
  bool exited;
  uint64_t hash;  // FNV-1a over every frame's digest, then each kill's monster and companion
};

uint64_t fold(const sb::CoopReport& r) {
  uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](uint64_t w) {
    for (int i = 0; i < 8; i++) {
      h ^= (w >> (8 * i)) & 0xffu;
      h *= 1099511628211ull;
    }
  };
  for (uint64_t d : r.digests) mix(d);
  for (const sb::CoopKill& k : r.kills) {
    mix(static_cast<uint64_t>(static_cast<uint32_t>(k.ent)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(k.by)));
  }
  return h;
}

int ms(double s) { return s < 0 ? -1 : static_cast<int>(std::lround(s * 1000.0)); }

void same_run(sb::CoopOptions o, const Pin& qc) {
  o.root = GE_SOURCE_DIR;
  o.progs = &native();
  const sb::CoopReport n = sb::run_coop(o);
  CHECK_MSG(n.ok, n.error);
  CHECK(n.faults == 0);
  const Pin got = {n.digests.size(),   n.kills.size(),  n.monsters_total, ms(n.button_pressed_at),
                   ms(n.door_open_at), n.leader_deaths, n.exited,         fold(n)};
  const std::string seen =
      "{" + std::to_string(got.frames) + ", " + std::to_string(got.kills) + ", " +
      std::to_string(got.monsters) + ", " + std::to_string(got.button_ms) + ", " +
      std::to_string(got.door_ms) + ", " + std::to_string(got.leader_deaths) + ", " +
      (got.exited ? "true" : "false") + ", " + std::to_string(got.hash) + "ull}";
  CHECK_MSG(got.frames == qc.frames && got.kills == qc.kills && got.monsters == qc.monsters &&
                got.button_ms == qc.button_ms && got.door_ms == qc.door_ms &&
                got.leader_deaths == qc.leader_deaths && got.exited == qc.exited &&
                got.hash == qc.hash,
            "the campaign's run moved from the QC's: now " + seen);
}

}  // namespace

TEST(the_campaign_builds_as_a_native_program) {
  CHECK(native().natives != nullptr);
  CHECK(native().statements.size() <= 1);  // no bytecode: every function is C++
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall) {
  sb::CoopOptions o;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13111, 13417, 0, false, 10830296068118722354ull});
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall_with_two_companions_on_hard) {
  sb::CoopOptions o;
  o.skills = {sh::AimSkill::expert, sh::AimSkill::novice};
  o.skill = 3;
  o.seed = 7;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13097, 13403, 0, false, 5208136328405202676ull});
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall_to_its_exit) {
  sb::CoopOptions o;
  o.exit = true;
  o.seed = 3;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13125, 13431, 0, false, 14654904817129053890ull});
}
