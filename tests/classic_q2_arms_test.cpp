// Quake II's weapons as any rules fire them (plugins/classic/q2_arms.hpp): a firing hand's
// period is its fire run plus the step back to ready (the shotgun 12 steps, 1.2 s; the rail 16),
// with one shot a once-fired weapon's run; the chaingun's spin fires 1, then 2, then 3 bullets a
// step; a blow's push is 500 x knockback / mass and 1600 x on oneself; every weapon has its fire
// word and each weapon but the BFG its obituary words, for a hit or its blast.
//
// ge:summary Tests Quake II's weapons' shared hand step, shot counts, push and words
// ge:keywords test q2-arms hand-step q2_firing_step weapon-period chaingun-spin q2_push
// ge:skill classic testing
#include <string_view>

#include "check.hpp"
#include "classic/q2_arms.hpp"

namespace gc = ge::classic;

namespace {

struct Run {
  int steps = 0, shots = 0;
};

// Attack held from ready: the steps until the hand is ready again, and the shots on the way.
Run fire_once(gc::Q2ArmId id) {
  const gc::Q2Arm& a = gc::q2_arm(id);
  int frame = a.frames.last_activate + 1, held_shots = 0;
  Run r;
  for (int i = 0; i < 200; i++) {
    r.steps++;
    const gc::Q2Firing f =
        gc::q2_firing_step(a.frames, a.held, frame, held_shots, false, 1000, false);
    r.shots += f.shots;
    if (f.done) break;
  }
  return r;
}

}  // namespace

TEST(a_once_fired_weapons_period_is_its_fire_run_and_the_step_back) {
  const Run shotgun = fire_once(gc::Q2ArmId::shotgun);
  CHECK_MSG(shotgun.steps == 12 && shotgun.shots == 1, std::to_string(shotgun.steps));
  const Run rail = fire_once(gc::Q2ArmId::railgun);
  CHECK_MSG(rail.steps == 16 && rail.shots == 1, std::to_string(rail.steps));
  const Run rocket = fire_once(gc::Q2ArmId::rocket_launcher);
  CHECK_MSG(rocket.steps == 9 && rocket.shots == 1, std::to_string(rocket.steps));
}

TEST(the_chaingun_spins_up_to_three_bullets_a_step) {
  const gc::Q2Arm& a = gc::q2_arm(gc::Q2ArmId::chaingun);
  int frame = a.frames.last_activate + 1, held_shots = 0, most = 0, first = -1;
  for (int i = 0; i < 40; i++) {
    const gc::Q2Firing f =
        gc::q2_firing_step(a.frames, a.held, frame, held_shots, true, 1000, true);
    if (first < 0 && f.shots > 0) first = f.shots;
    most = f.shots > most ? f.shots : most;
  }
  CHECK(first == 1 && most == 3);
}

TEST(a_blows_push_is_quake_iis) {
  CHECK(gc::q2_push(100, gc::kQ2PlayerMass, false) == 250.0f);
  CHECK(gc::q2_push(100, gc::kQ2PlayerMass, true) == 800.0f);
  CHECK(gc::q2_push(10, 10, false) == 100.0f);  // a mass under 50 counts as 50
}

TEST(every_weapon_has_its_fire_word_and_obituary) {
  for (int a = 0; a < gc::kQ2ArmCount; a++) {
    CHECK(std::string_view(gc::kQ2ArmFireWords[a]).starts_with("q2_"));
    const gc::Q2ArmWords hit = gc::q2_arm_words(static_cast<gc::Q2ArmId>(a), false);
    const gc::Q2ArmWords blast = gc::q2_arm_words(static_cast<gc::Q2ArmId>(a), true);
    const bool own = a == static_cast<int>(gc::Q2ArmId::bfg);  // its rules' own words, by its kind
    CHECK_MSG(own || hit.words != nullptr || blast.words != nullptr, gc::kQ2ArmFireWords[a]);
  }
}
