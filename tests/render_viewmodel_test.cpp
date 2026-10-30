// The view-model pass (engine/render/viewmodel.hpp), its CPU half: the gun's depth floor is what
// the world at the guard has and never reaches 1; a held point light handed to the frame as a lamp
// puts the same light on white as the held unit says; a packed draw carries the placement, the
// lens, the flags and the light, and the view's basis the frame's camera has.
//
// ge:summary Tests for the view-model pass's CPU half: depth floor, lamp units, the packed draw
// ge:keywords test viewmodel held_depth_floor held_candela lamp_of pack_held_draw basis lens
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/lights.hpp"
#include "render/viewmodel.hpp"
#include "viewmodel/light.hpp"

namespace {

using namespace ge::render;
namespace vm = ge::viewmodel;

bool near(double a, double b, double eps = 1e-5) { return std::fabs(a - b) <= eps; }

}  // namespace

TEST(the_depth_floor_is_the_world_at_the_guard) {
  CHECK(near(held_depth_floor(0.1, 0.25), 0.4));
  CHECK(near(held_depth_floor(0.1, 1.0), 0.1));
  CHECK(near(held_depth_floor(0.1, 0.05), 0.9));  // a guard inside the near plane: capped
  CHECK(near(held_depth_floor(0.1, 0.0), 0.9));
}

TEST(a_held_light_as_a_lamp_lights_white_the_same) {
  vm::PointLight p;
  p.position[0] = 0;
  p.position[1] = 0;
  p.position[2] = 2;
  p.intensity = 3;
  p.range_m = 6;
  p.radius_m = 0.1f;
  const double white_lux = 800;
  const LocalLight l = lamp_of(p, white_lux);
  CHECK(near(l.candela, 2400, 1e-3));
  CHECK(l.range_m == p.range_m && l.radius_m == p.radius_m && l.outer_deg == 0);
  const double at[3] = {0, 0, 0};
  const double held = p.intensity * vm::light_falloff(2.0f, p.range_m, p.radius_m);
  CHECK(near(lamp_lux_at(std::span<const LocalLight>(&l, 1), at, 0) / white_lux, held, 1e-5));
}

TEST(a_packed_draw_carries_its_placement_lens_flags_and_light) {
  FrameView view;
  view.heading = 0.7;
  view.pitch = -0.2;
  view.fov_y = 1.1;
  view.near_m = 0.1;
  HeldDraw d;
  d.model = 2;
  d.frame_a = 3;
  d.frame_b = 4;
  d.blend = 0.25f;
  d.now.m[3] = 0.13;
  d.last.m[3] = 0.11;
  d.fov_y = 0.9;
  d.outdoor = 0.5f;
  d.lamps = false;
  d.light.count = 1;
  d.light.lights[0] = {{0.1f, 0.2f, 0.3f}, {1, 2, 3}, 5, 0.05f};
  d.light.floor = 0.07f;
  const GpuHeldDraw g = pack_held_draw(d, view, 1600, 900, 0.25);
  CHECK(g.now[3] == 0.13f && g.last[3] == 0.11f && g.now[0] == 1 && g.now[5] == 1);
  CHECK(near(g.lens[1], 1 / std::tan(0.45)) && near(g.lens[0], g.lens[1] * 900 / 1600));
  CHECK(near(g.lens[2], kHeldNearM) && near(g.lens[3], 0.4));
  CHECK(g.info[0] == 3 && g.info[1] == 4 && g.info[2] == 2 && g.info[3] == 2u);
  CHECK(g.mix[0] == 0.25f && g.mix[1] == 0.5f && g.mix[3] == 0.07f);
  CHECK(g.toward[3] == 1 && g.lights[0][2] == 0.3f && g.lights[0][3] == 5 && g.lights[0][6] == 3 &&
        g.lights[0][7] == 0.05f);
  // The basis: unit, square, forward along the look, and the lens ratio of the two lenses.
  double f[3];
  look_dir(view.heading, view.pitch, f);
  CHECK(near(g.forward[0], f[0]) && near(g.forward[1], f[1]) && near(g.forward[2], f[2]));
  const double ru = g.right[0] * g.up[0] + g.right[1] * g.up[1] + g.right[2] * g.up[2];
  const double rr = g.right[0] * g.right[0] + g.right[1] * g.right[1] + g.right[2] * g.right[2];
  CHECK(near(ru, 0) && near(rr, 1));
  CHECK(near(g.right[1], 0));  // no roll: the right stays level
  CHECK(near(g.right[3], std::tan(0.55) / std::tan(0.45)));
  // No lens of its own: the view's, and the ratio 1.
  d.fov_y = 0;
  CHECK(near(pack_held_draw(d, view, 1600, 900, 0.25).right[3], 1));
}
