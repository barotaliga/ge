// games/shooters_edge/client/touch_play.hpp: the game played by touch on the page.
//
// ge:summary Test: the touch layout walks, looks, fires, jumps and takes the next weapon
// ge:keywords test touch touch_play touch_moves stick look-drag fire-button
// ge:skill input testing
#include "shooters_edge/client/touch_play.hpp"

#include <cmath>

#include "check.hpp"
#include "input/sim.hpp"

namespace cl = shooters_edge::client;
namespace in = ge::input;

namespace {

struct Screen {
  in::SimInput sim;
  in::InputState s;
  cl::TouchPlay t = cl::touch_play(16.0f / 9.0f);
  in::DeviceId glass = in::sim_plug(sim, in::sim_touch());
  double now = 0;
  cl::Moves m;
  float yaw = 0, pitch = 0;

  void touch(uint16_t id, in::TouchPhase p, float x, float y) {
    in::sim_touch(sim, glass, id, p, x, y);
  }
  void frame() {
    in::sim_frame(sim, s);
    now += 1.0 / 60;
    m = {};
    cl::touch_moves(t, s, now, m, yaw, pitch);
  }
};

}  // namespace

TEST(the_left_thumb_walks_and_the_right_one_looks) {
  Screen sc;
  CHECK(!sc.t.seen);
  sc.touch(1, in::TouchPhase::down, 0.2f, 0.7f);
  sc.touch(2, in::TouchPhase::down, 0.6f, 0.5f);
  sc.frame();
  CHECK(sc.t.seen && !sc.m.forward && sc.yaw == 0);
  // The left thumb pushes up and right; the right one drags a tenth of the height right.
  sc.touch(1, in::TouchPhase::move, 0.2f + 0.1f * 9 / 16, 0.6f);
  sc.touch(2, in::TouchPhase::move, 0.6f + 0.1f * 9 / 16, 0.5f);
  sc.frame();
  CHECK(sc.m.forward && sc.m.moveright && !sc.m.back && !sc.m.moveleft);
  CHECK(std::fabs(sc.yaw - 0.1f * cl::kTouchLookDegrees) < 0.5f && std::fabs(sc.pitch) < 0.01f);
  CHECK(!sc.m.attack);
  // Let go: nothing held, nothing turned.
  sc.touch(1, in::TouchPhase::up, 0.25f, 0.6f);
  sc.touch(2, in::TouchPhase::up, 0.65f, 0.5f);
  sc.frame();
  sc.frame();
  CHECK(!sc.m.forward && !sc.m.moveright && sc.yaw == 0);
}

TEST(the_buttons_fire_jump_and_take_the_next_weapon) {
  Screen sc;
  sc.touch(1, in::TouchPhase::down, 0.9f, 0.8f);  // fire, held
  sc.frame();
  CHECK(sc.m.attack && !sc.m.jump && sc.yaw == 0);  // the button's finger never looks
  sc.touch(1, in::TouchPhase::move, 0.7f, 0.8f);    // slid off, still the button's
  sc.touch(2, in::TouchPhase::down, 0.9f, 0.45f);   // jump
  sc.frame();
  CHECK(sc.m.attack && sc.m.jump && sc.yaw == 0);
  sc.touch(3, in::TouchPhase::down, 0.9f, 0.1f);  // the weapon: once, as it goes down
  sc.frame();
  CHECK(sc.m.impulse == cl::kNextWeaponImpulse);
  sc.frame();
  CHECK(sc.m.impulse == 0);
}
