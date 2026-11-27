// Quake II's weapons as any rules fire them (plugins/classic/q2_arms.hpp): a firing hand's
// period is its fire run plus the step back to ready (the shotgun 12 steps, 1.2 s; the rail 16),
// with one shot a once-fired weapon's run; the chaingun's spin fires 1, then 2, then 3 bullets a
// step; a blow's push is 500 x knockback / mass and 1600 x on oneself; every weapon has its fire
// word and each weapon but the BFG its obituary words, for a hit or its blast.
//
// ge:summary Tests Quake II's weapons' shared hand step, shot counts, push and words
// ge:keywords test q2-arms hand-step q2_firing_step weapon-period chaingun-spin q2_push
// ge:skill classic testing
#include <string>
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

// The idle run rests on its pause frames: a draw that rests stays put, one whose low four bits are
// 0 moves on; the last idle frame goes back to the first without asking its pause (the super
// shotgun's 57), but the hand grenade's 48 rests before it goes back to 16; the hyperblaster never
// rests.
TEST(the_idle_run_rests_on_its_pause_frames) {
  const auto step = [](gc::Q2ArmId id, int frame, uint32_t draw) {
    gc::q2_idle_step(gc::q2_arm(id).frames, gc::kQ2ArmPauses[static_cast<int>(id)], frame, draw);
    return frame;
  };
  CHECK(step(gc::Q2ArmId::blaster, 19, 1) == 19);
  CHECK(step(gc::Q2ArmId::blaster, 19, 16) == 20);  // low bits 0: on
  CHECK(step(gc::Q2ArmId::blaster, 20, 1) == 21);   // not a pause frame
  CHECK(step(gc::Q2ArmId::super_shotgun, 57, 1) == 18);
  CHECK(step(gc::Q2ArmId::hand_grenade, 48, 1) == 48);
  CHECK(step(gc::Q2ArmId::hand_grenade, 48, 0) == 16);
  for (int f = 21; f <= 49; f++) CHECK(step(gc::Q2ArmId::hyperblaster, f, 1) != f);
  // A long idle by the server's draws: about 15 in 16 of the steps on a pause frame stay.
  int frame = gc::q2_arm(gc::Q2ArmId::rocket_launcher).frames.last_fire + 1, stayed = 0, on = 0;
  for (uint32_t t = 0; t < 4000; t++) {
    const int was = frame;
    gc::q2_idle_step(gc::q2_arm(gc::Q2ArmId::rocket_launcher).frames,
                     gc::kQ2ArmPauses[static_cast<int>(gc::Q2ArmId::rocket_launcher)], frame,
                     gc::q2_pause_draw(t, 3));
    if (gc::q2_pauses_on(gc::kQ2ArmPauses[6], was) && was != 50) {
      on++;
      stayed += frame == was ? 1 : 0;
    }
  }
  CHECK_MSG(on > 1000 && stayed * 16 > on * 14 && stayed * 16 < on * 16, std::to_string(on));
}

// The kick: straight up and back along the aim for the once-fired weapons (the rail 3 and 3), the
// chaingun's shake within 0.7 degrees and 0.35 units on the wire's quarter grain, the machinegun's
// climb its pitch (none in deathmatch), the hand grenade none.
TEST(each_weapons_kick_is_quake_iis) {
  const float ahead[3] = {1, 0, 0};
  const gc::Q2Kick rail = gc::q2_shot_kick(gc::Q2ArmId::railgun, ahead, 0, 7);
  CHECK(rail.angles[0] == -3 && rail.angles[1] == 0 && rail.origin[0] == -3);
  const gc::Q2Kick shot = gc::q2_shot_kick(gc::Q2ArmId::shotgun, ahead, 0, 7);
  CHECK(shot.angles[0] == -2 && shot.origin[0] == -2);
  const gc::Q2Kick blast = gc::q2_shot_kick(gc::Q2ArmId::blaster, ahead, 0, 7);
  CHECK(blast.angles[0] == -1 && blast.origin[0] == -2);
  const gc::Q2Kick none = gc::q2_shot_kick(gc::Q2ArmId::hand_grenade, ahead, 0, 7);
  CHECK(none.angles[0] == 0 && none.origin[0] == 0);
  for (uint32_t d = 0; d < 200; d++) {
    const gc::Q2Kick c = gc::q2_shot_kick(gc::Q2ArmId::chaingun, ahead, 0, d);
    for (int i = 0; i < 3; i++) {
      CHECK(c.angles[i] >= -0.5f && c.angles[i] <= 0.5f);  // 0.7 on the quarter grain
      CHECK(c.angles[i] * 4 == static_cast<float>(static_cast<int>(c.angles[i] * 4)));
      CHECK(c.origin[i] >= -0.25f && c.origin[i] <= 0.25f);
    }
  }
  CHECK(gc::q2_machine_climb(0, false) == 0);
  CHECK(gc::q2_machine_climb(3, false) == 4.5f);
  CHECK(gc::q2_machine_climb(20, false) == 13.5f);
  CHECK(gc::q2_machine_climb(5, true) == 0);
  CHECK(gc::q2_shot_kick(gc::Q2ArmId::machinegun, ahead, 4.5f, 1).angles[0] == -4.5f);
  CHECK(gc::q2_kick_wire(-40) == -32);  // the BFG's hurt kick as the wire carries it
}

// A kick rises over one tenth and falls over the next.
TEST(a_kick_eases_in_and_out_over_two_tenths) {
  gc::Q2KickBlend b;
  gc::Q2Kick k;
  k.angles[0] = -2;
  gc::q2_kick_shot(b, 1.0, k);
  const auto near = [](float x, float y) { return x - y < 1e-3f && y - x < 1e-3f; };
  CHECK(near(gc::q2_kick_at(b, 1.0).angles[0], 0));
  CHECK(near(gc::q2_kick_at(b, 1.05).angles[0], -1));
  gc::q2_kick_advance(b, 1.1);
  CHECK(near(gc::q2_kick_at(b, 1.1).angles[0], -2));
  gc::q2_kick_advance(b, 1.15);
  CHECK(near(gc::q2_kick_at(b, 1.15).angles[0], -1));
  gc::q2_kick_advance(b, 1.25);
  CHECK(near(gc::q2_kick_at(b, 1.25).angles[0], 0));
}

// The shots read off the frames: each once-fired weapon on its step off its fire frame, the
// machinegun on each change between 4 and 5, the chaingun round its loop, never on an idle step or
// a let go.
TEST(shots_are_read_off_the_hands_frames) {
  const auto fired = [](gc::Q2ArmId id, int was, int now) {
    const gc::Q2Arm& a = gc::q2_arm(id);
    return gc::q2_frame_fired(a.frames, a.held, was, now);
  };
  CHECK(fired(gc::Q2ArmId::rocket_launcher, 13, 6));  // from idle: frame 5 fired, 6 shown
  CHECK(!fired(gc::Q2ArmId::rocket_launcher, 6, 7));
  CHECK(!fired(gc::Q2ArmId::rocket_launcher, 20, 21));  // idle
  CHECK(!fired(gc::Q2ArmId::bfg, 16, 18));              // a frame missed between: no shot read
  CHECK(fired(gc::Q2ArmId::bfg, 17, 18));
  CHECK(fired(gc::Q2ArmId::machinegun, 6, 5));
  CHECK(fired(gc::Q2ArmId::machinegun, 5, 4));
  CHECK(!fired(gc::Q2ArmId::machinegun, 4, 6));  // let go
  CHECK(fired(gc::Q2ArmId::chaingun, 20, 21));
  CHECK(fired(gc::Q2ArmId::chaingun, 21, 15));   // round the loop
  CHECK(!fired(gc::Q2ArmId::chaingun, 14, 32));  // let go before the spin
  CHECK(fired(gc::Q2ArmId::hyperblaster, 12, 7));
  CHECK(!fired(gc::Q2ArmId::hyperblaster, 8, 12));  // let go
}
