// Tests: the marks a Quake II entity's effect bits leave in the world as the window draws them
// (games/shooters_edge/client/world_fx_q2.cpp's q2_mark_fx, fed by client/demo_fx.hpp's
// q2_missile): the mission packs' ion ripper's specks and light, the blue hyperblaster's blue
// light, the phalanx's plasma, the trap's glow over it, the tracker's dark strands, and spinning
// lights' red light turning ahead; the flags' columns beside them.
//
// ge:summary Tests: Quake II's effect-bit marks (ion ripper, trap, tracker) drawn in the world
// ge:keywords test q2 ef_ionripper ef_trap ef_tracker ef_plasma ef_spinninglights mission-pack
// ge:skill shooter testing
#include <cmath>
#include <vector>

#include "brush/map.hpp"
#include "check.hpp"
#include "shooters_edge/client/demo_fx.hpp"
#include "shooters_edge/client/q2_look.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace {

namespace cl = shooters_edge::client;
constexpr double kU = ge::brush::kUnitM;

struct Marked {
  cl::FxFrame out;
  bool known = false;
};

// An entity with `bits` that moved 64 units along x this frame, at time `t`.
Marked mark(uint32_t bits, double t = 0) {
  cl::FxState s;
  s.palette.assign(768, 128);
  cl::Missile m;
  Marked r;
  r.known = cl::q2_missile(static_cast<int>(bits), m);
  const double from[3] = {0, 0, 0}, to[3] = {64 * kU, 0, 0};
  if (r.known) cl::q2_mark_fx(s, m, true, from, to, t, r.out);
  return r;
}

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

bool lit(const Marked& m, double radius_u, double red, double green, double blue) {
  if (m.out.lights.size() != 1) return false;
  const auto& l = m.out.lights[0];
  const double level = radius_u / 128;
  return near(l.radius, radius_u * kU) && near(l.colour[0], red * level) &&
         near(l.colour[1], green * level) && near(l.colour[2], blue * level);
}

}  // namespace

TEST(the_reckonings_ion_ripper_and_blue_hyperblaster_and_phalanx_light_their_shots) {
  const Marked ion = mark(cl::kSqIonRipper);
  CHECK(ion.known && !ion.out.births.empty());  // its specks along the way it flew
  CHECK(lit(ion, 100, 1, 0.5, 0.5));
  const Marked blue = mark(cl::kSqBlueHyperblaster);
  CHECK(blue.known && blue.out.births.empty() && lit(blue, 200, 0, 0, 1));
  const Marked plasma = mark(cl::kSqPlasma);
  CHECK(plasma.known && lit(plasma, 130, 1, 0.5, 0.5));
}

TEST(the_reckonings_trap_glows_over_it_with_a_flickering_light) {
  const Marked trap = mark(cl::kSqTrap);
  CHECK(trap.known && !trap.out.births.empty());
  CHECK(trap.out.lights.size() == 1);
  if (trap.out.lights.empty()) return;
  const auto& l = trap.out.lights[0];
  CHECK(near(l.pos[0], 64 * kU) && near(l.pos[1], 32 * kU));  // 32 units over it
  CHECK(l.radius >= 100 * kU && l.radius < 200 * kU);
}

TEST(ground_zeros_tracker_leaves_dark_strands_and_darkens_the_room) {
  const Marked tracker = mark(cl::kSqTracker);
  CHECK(tracker.known && !tracker.out.births.empty() && lit(tracker, 200, -1, -1, -1));
  const Marked shell = mark(cl::kSqTrackerTrail);
  CHECK(shell.known && !shell.out.births.empty() && lit(shell, 155, -1, -1, -1));
  const Marked dark = mark(cl::kSqTrackerTrail | cl::kSqTracker);
  CHECK(dark.known && dark.out.births.empty() && dark.out.lights.size() == 1);
  CHECK(!dark.out.lights.empty() && dark.out.lights[0].colour[0] < 0);
}

TEST(spinning_lights_put_a_red_light_64_units_ahead_turning) {
  const Marked a = mark(cl::kSqSpinningLights, 0);
  const Marked b = mark(cl::kSqSpinningLights, 0.09);  // 45 degrees on at 500 a second
  CHECK(a.known && a.out.lights.size() == 1 && b.out.lights.size() == 1);
  if (a.out.lights.empty() || b.out.lights.empty()) return;
  const auto& la = a.out.lights[0];
  const auto& lb = b.out.lights[0];
  CHECK(near(std::hypot(la.pos[0] - 64 * kU, la.pos[2]), 64 * kU));
  CHECK(near(std::hypot(lb.pos[0] - 64 * kU, lb.pos[2]), 64 * kU));
  CHECK(!near(la.pos[2], lb.pos[2]));  // it turned
  CHECK(la.colour[0] > 0 && near(la.colour[1], 0) && near(la.colour[2], 0));
}

TEST(the_flags_and_the_tag_leave_their_columns_in_their_colours) {
  const Marked red = mark(cl::kSqFlag1);
  CHECK(red.known && !red.out.births.empty() && lit(red, 225, 1, 0.1, 0.1));
  const Marked blue = mark(cl::kSqFlag2);
  CHECK(blue.known && !blue.out.births.empty() && lit(blue, 225, 0.1, 0.1, 1));
}
