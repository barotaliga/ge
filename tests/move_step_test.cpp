// Character movement (engine/move/step.hpp, spec.hpp, knobs.hpp, strafe.hpp), held to what each
// knob promises: a jump's apex is jump^2 / 2g at any step rate; a run reaches the wish speed and
// never passes it; friction stops a runner; jumping without turning gains nothing and strafe
// jumping gains a lot; jump must be pressed afresh unless held jumps are on; the buffer and the
// coyote time each rescue a jump; a jump let go is cut; a double jump; stairs climbed, a steep
// ramp not; crouching under a ceiling and standing after; a ledge's landing and its impact; the
// slide's boost; air control's turn; flight without gravity. And the knob table: every name once,
// set and clamped by name, scaled between units.
//
// ge:summary Tests: the mover's step against each knob's promise; the knob table; the strafer
// ge:keywords test move movement step jump apex run friction strafe-jump bhop coyote buffer
// ge:keywords jump-cut double-jump stairs ramp crouch ledge slide air-control fly knobs presets
// ge:skill move testing
#include <cmath>
#include <set>
#include <string>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "move/knobs.hpp"
#include "move/scenarios.hpp"
#include "move/strafe.hpp"

namespace mv = ge::move;

namespace {

constexpr double kHz = 120;

mv::MoveRun run(const char* id, const mv::MoveSpec& spec, double hz = kHz) {
  const mv::MoveScenario* sc = mv::find_move_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  if (sc == nullptr) return {};
  return mv::run_move_scenario(*sc, spec, hz);
}

mv::MoveRun run(const char* id) {
  const mv::MoveScenario* sc = mv::find_move_scenario(id);
  return run(id, *mv::find_preset(sc->preset));
}

// The last state a run left, through the step callback.
struct Last {
  mv::MoveState s;
  static void keep(void* ctx, const mv::MoveState& s, const mv::MoveCmd&, const mv::MoveEvents&) {
    static_cast<Last*>(ctx)->s = s;
  }
};

mv::MoveState last_of(const char* id, const mv::MoveSpec& spec) {
  Last last;
  mv::run_move_scenario(*mv::find_move_scenario(id), spec, kHz, Last::keep, &last);
  return last.s;
}

std::string look(const char* id) {
  return std::string("; run `make lab LAB=move-trace ARGS=--scenario=") + id +
         "` and read engine/move/step.cpp (skill: move)";
}

std::string num(double v) { return std::to_string(v); }

}  // namespace

TEST(a_jump_rises_jump_squared_over_two_g_at_any_step_rate) {
  const mv::MoveSpec spec = mv::classic_arena();
  const double want = spec.jump_speed * spec.jump_speed / (2 * spec.gravity);
  for (const double hz : {30.0, 60.0, 120.0, 250.0}) {
    const mv::MoveRun r = run("standing-jump", spec, hz);
    const double dt = 1 / hz;
    CHECK_MSG(r.jumps == 1 && r.landings == 1, "at " + num(hz) + " Hz: " + num(r.jumps) +
                                                   " jumps, " + num(r.landings) + " landings" +
                                                   look("standing-jump"));
    // The apex is sampled at a step, so it may miss the parabola's top by g dt^2 / 8.
    CHECK_MSG(r.apex <= want + 1e-9 && r.apex >= want - spec.gravity * dt * dt / 8 - 1e-9,
              "at " + num(hz) + " Hz the apex was " + num(r.apex) + ", want " + num(want) +
                  look("standing-jump"));
  }
}

TEST(a_run_reaches_the_wish_speed_and_friction_stops_it) {
  const mv::MoveSpec spec = mv::classic_arena();
  const mv::MoveRun r = run("run-stop");
  CHECK_MSG(r.top_speed <= spec.max_speed + 1e-9 && r.top_speed > spec.max_speed - 1,
            "the run topped out at " + num(r.top_speed) + look("run-stop"));
  CHECK_MSG(r.end_speed == 0, "still moving at " + num(r.end_speed) + " after 1.5 s let go");
  // The same run at 60 and 250 Hz covers the same ground within 2%.
  const double d60 = run("run-stop", spec, 60).distance, d250 = run("run-stop", spec, 250).distance;
  CHECK_MSG(std::fabs(d60 - d250) < 0.02 * d250,
            "60 Hz ran " + num(d60) + ", 250 Hz ran " + num(d250));
}

TEST(jumping_straight_on_gains_nothing_and_strafe_jumping_gains_a_lot) {
  const mv::MoveSpec spec = mv::classic_arena();
  const mv::MoveRun straight = run("bhop-straight");
  CHECK_MSG(straight.jumps > 5 && straight.top_speed < spec.max_speed + 5,
            "straight hops: " + num(straight.jumps) + " jumps, top " + num(straight.top_speed) +
                look("bhop-straight"));
  const mv::MoveRun strafe = run("strafe-jump");
  CHECK_MSG(strafe.jumps > 10 && strafe.end_speed > 1.3 * spec.max_speed,
            "strafe jumping: " + num(strafe.jumps) + " jumps, ending at " + num(strafe.end_speed) +
                look("strafe-jump"));
  const mv::MoveRun side = run("strafe-jump-side");
  CHECK_MSG(side.end_speed > 1.5 * spec.max_speed,
            "strafe-only jumping ended at " + num(side.end_speed) + look("strafe-jump-side"));
}

TEST(the_best_wish_angle_gains_more_than_any_other) {
  const mv::MoveSpec spec = mv::classic_arena();
  const double dt = 1 / kHz, speed = 500, wish = spec.max_speed, cap = spec.air_speed_cap;
  const double best = mv::best_wish_angle(speed, wish, cap, spec.air_accelerate, dt);
  auto gain = [&](double theta) {
    mv::Vec3 v{0, 0, speed};
    mv::accelerate(v, {ge::core::libm::sin(theta), 0, ge::core::libm::cos(theta)}, wish, cap,
                   spec.air_accelerate, dt);
    return mv::flat_length(v) - speed;
  };
  CHECK(gain(best) > 0);
  for (const double off : {-0.2, -0.05, 0.05, 0.2}) {
    CHECK_MSG(gain(best + off) <= gain(best) + 1e-12,
              "at best " + num(best) + " + " + num(off) + " gained " + num(gain(best + off)));
  }
  CHECK(mv::best_wish_angle(10, wish, cap, spec.air_accelerate, dt) == 0);  // too slow to turn
}

TEST(jump_must_be_pressed_afresh_unless_held_jumps_are_on) {
  mv::MoveSpec spec = mv::classic_arena();
  mv::BoxWorld bw;
  mv::add_box(bw, {-1000, -10, -1000}, {1000, 0, 1000});
  const mv::World world = mv::as_world(bw);
  auto jumps_holding = [&](const mv::MoveSpec& sp) {
    mv::MoveState s = mv::stand_at(sp, world, {0, 0, 0});
    mv::MoveCmd cmd;
    cmd.jump = true;
    int n = 0;
    for (int i = 0; i < 360; i++) n += mv::step_move(sp, world, s, cmd, 1 / kHz).has(mv::kJumped);
    return n;
  };
  CHECK_MSG(jumps_holding(spec) == 1, "held jump jumped " + num(jumps_holding(spec)) + " times");
  spec.jump_hold = true;
  CHECK_MSG(jumps_holding(spec) >= 3, "auto hop jumped " + num(jumps_holding(spec)) + " times");
}

TEST(the_buffer_keeps_an_early_press_and_coyote_time_a_late_one) {
  // Press jump in the air shortly before landing: with a buffer it jumps on landing.
  mv::BoxWorld bw;
  mv::add_box(bw, {-1000, -10, -1000}, {1000, 0, 1000});
  const mv::World world = mv::as_world(bw);
  auto bounces = [&](double buffer) {
    mv::MoveSpec spec = mv::modern_shooter();
    spec.jump_buffer = buffer;
    mv::MoveState s = mv::stand_at(spec, world, {0, 2, 0});
    s.grounded = false;
    s.pos.y = 0.3;  // falls 0.3 m: about 0.19 s
    int jumps = 0;
    for (int i = 0; i < 120; i++) {
      mv::MoveCmd cmd;
      cmd.jump = i == 14;  // pressed for one step, ~0.08 s before landing
      jumps += mv::step_move(spec, world, s, cmd, 1 / kHz).has(mv::kJumped);
    }
    return jumps;
  };
  CHECK_MSG(bounces(0.12) == 1, "with a buffer: " + num(bounces(0.12)) + " jumps");
  CHECK_MSG(bounces(0) == 0, "with no buffer: " + num(bounces(0)) + " jumps");

  const mv::MoveRun late = run("coyote");
  CHECK_MSG(
      late.jumps == 1 && late.end_height > -0.5,
      "coyote: " + num(late.jumps) + " jumps, ended at " + num(late.end_height) + look("coyote"));
  mv::MoveSpec strict = mv::platformer();
  strict.coyote_time = 0;
  strict.air_jumps = 0;  // or the late press is spent on an air jump instead
  const mv::MoveRun fell = run("coyote", strict);
  CHECK_MSG(fell.jumps == 0 && fell.end_height < -4,
            "no coyote time: " + num(fell.jumps) + " jumps, ended at " + num(fell.end_height));
}

TEST(a_jump_let_go_is_cut_and_a_second_jump_rises_from_the_air) {
  const mv::MoveRun tap = run("jump-tap"), held = run("jump-hold"), twice = run("double-jump");
  CHECK_MSG(tap.apex < 0.6 * held.apex,
            "tap rose " + num(tap.apex) + ", held " + num(held.apex) + look("jump-tap"));
  CHECK_MSG(twice.air_jumps == 1 && twice.apex > held.apex,
            "double jump: " + num(twice.air_jumps) + " air jumps, apex " + num(twice.apex) +
                look("double-jump"));
}

TEST(stairs_are_climbed_and_a_steep_ramp_is_not) {
  const mv::MoveSpec spec = mv::classic_arena();
  const mv::MoveRun up = run("stairs");
  CHECK_MSG(up.jumps == 0 && up.steps_up >= 8 && std::fabs(up.end_height - 2 * spec.unit) < 1,
            "stairs: " + num(up.steps_up) + " steps, ended " + num(up.end_height) + " up" +
                look("stairs"));
  const mv::MoveRun walk = run("ramp-walk");
  CHECK_MSG(walk.end_height > 3.4 * spec.unit,
            "the 30 degree ramp: ended " + num(walk.end_height) + " up" + look("ramp-walk"));
  const mv::MoveRun steep = run("ramp-steep");
  CHECK_MSG(steep.end_height < spec.step_height + 1,
            "the 55 degree ramp: ended " + num(steep.end_height) + " up" + look("ramp-steep"));
  const mv::MoveRun person = run("stroll-stairs");
  CHECK_MSG(person.steps_up >= 8 && std::fabs(person.end_height - 2) < 0.05,
            "the walker on the stairs: " + num(person.steps_up) + " steps, ended " +
                num(person.end_height) + " m up" + look("stroll-stairs"));
}

TEST(crouching_passes_under_a_ceiling_and_stands_after_it) {
  const mv::MoveSpec spec = mv::classic_arena();
  const mv::MoveState s = last_of("crouch-tunnel", spec);
  CHECK_MSG(s.pos.z > 7 * spec.unit && !s.ducked && s.crouch == 0,
            "after the tunnel at z " + num(s.pos.z) + (s.ducked ? ", still ducked" : "") +
                look("crouch-tunnel"));
  // Without crouching the ceiling stops him.
  const mv::MoveScenario* sc = mv::find_move_scenario("crouch-tunnel");
  mv::MoveScenario upright = *sc;
  upright.script = [](const mv::MoveSpec&, const mv::MoveState&, double, mv::ScriptMemory&) {
    mv::MoveCmd c;
    c.forward = 1;
    return c;
  };
  Last last;
  mv::run_move_scenario(upright, spec, kHz, Last::keep, &last);
  CHECK_MSG(last.s.pos.z < 3 * spec.unit, "standing, he got to z " + num(last.s.pos.z));
}

TEST(running_off_a_ledge_lands_hard_once) {
  const mv::MoveSpec spec = mv::classic_arena();
  const mv::MoveRun r = run("ledge");
  const double fall = std::sqrt(2 * spec.gravity * 3 * spec.unit);
  CHECK_MSG(r.landings == 1 && r.max_impact > 0.9 * fall && r.max_impact < 1.05 * fall,
            "ledge: " + num(r.landings) + " landings at " + num(r.max_impact) + ", want about " +
                num(fall) + look("ledge"));
  CHECK(std::fabs(r.end_height + 3 * spec.unit) < 1);
}

TEST(a_slide_starts_from_a_sprint_with_its_boost) {
  const mv::MoveSpec spec = mv::modern_shooter();
  const mv::MoveRun r = run("slide");
  CHECK_MSG(r.slides == 1 && r.top_speed > spec.max_speed * spec.sprint_scale + 1,
            "slide: " + num(r.slides) + " slides, top " + num(r.top_speed) + look("slide"));
}

TEST(air_control_turns_the_path_where_the_classic_air_does_not) {
  const mv::MoveState with = last_of("air-control", mv::strafe_arena());
  const mv::MoveState without = last_of("air-control", mv::classic_arena());
  const double turned = mv::heading_of(with.vel), kept = mv::heading_of(without.vel);
  CHECK_MSG(turned > 0.8 && kept < 0.3, "with air control the path turned " + num(turned) +
                                            " rad, without " + num(kept) + look("air-control"));
}

TEST(flight_rises_along_the_view_without_gravity) {
  const mv::MoveRun r = run("fly");
  CHECK_MSG(r.apex > 100 && r.landings == 0, "flew up " + num(r.apex) + look("fly"));
}

TEST(every_knob_is_named_once_and_set_by_name_within_its_range) {
  std::set<std::string> names;
  for (const mv::Knob& k : mv::knobs()) {
    CHECK_MSG(names.insert(k.name).second, std::string("knob named twice: ") + k.name);
    CHECK(mv::find_knob(k.name) == &k);
    for (const char* c = k.name; *c; c++) CHECK(*c >= 'a' && *c <= 'z');
  }
  CHECK(names.size() == mv::knobs().size() && names.size() > 40);
  mv::MoveSpec spec = mv::classic_arena();
  std::string error;
  CHECK(mv::apply_knobs(spec, "gravity=600, airaccelerate=10 jumphold=1 airjumps=2.6", error));
  CHECK(spec.gravity == 600 && spec.air_accelerate == 10 && spec.jump_hold && spec.air_jumps == 3);
  CHECK(mv::knob_text(spec, mv::classic_arena()) ==
        "gravity=600,airaccelerate=10,jumphold=1,airjumps=3");
  CHECK(mv::apply_knobs(spec, "walknormal=7", error) && spec.walk_normal == 1);  // clamped
  CHECK(!mv::apply_knobs(spec, "gravty=1", error) && error.find("gravty") != std::string::npos);
  CHECK(!mv::apply_knobs(spec, "gravity=fast", error) &&
        error.find("gravity=") != std::string::npos);
  CHECK(mv::number_field(spec, *mv::find_knob("gravity")) == &spec.gravity);
  CHECK(mv::flag_field(spec, *mv::find_knob("gravity")) == nullptr);
}

TEST(a_row_scales_between_units_and_every_preset_is_found) {
  const mv::MoveSpec metres = mv::scale_lengths(mv::classic_arena(), 1.0 / 32);
  CHECK(metres.unit == 1 && metres.gravity == 25 && metres.max_speed == 10);
  CHECK(metres.accelerate == 10 && metres.walk_normal == 0.7 && metres.jump_buffer == 0);
  int n = 0;
  for (const char* const* p = mv::preset_names(); *p != nullptr; p++, n++) {
    CHECK_MSG(mv::find_preset(*p) != nullptr, std::string("no preset ") + *p);
  }
  CHECK(n == 5 && mv::find_preset("nothing") == nullptr);
  for (const mv::MoveScenario& sc : mv::move_scenarios()) CHECK(mv::find_preset(sc.preset));
}
