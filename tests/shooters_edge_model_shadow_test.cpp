// Tests: the models' ground shadows (games/shooters_edge/client/model_shadow.hpp): none at mode 0,
// half dark under an animated model at 1 however high, fading with height at 2 and gone at four
// radii; none under a sprite, a see-through model, a model that casts none, no floor or a floor
// steeper than sixty degrees; the lean along the way it faces.
//
// ge:summary Tests: the models' ground shadows: modes, fade, the floor, the exclusions, the lean
// ge:keywords test shadow gl_shadows ground-shadows shadow_of fade lean no-shadow
// ge:skill render shooter testing
// ge:test tests/shooters_edge_model_shadow_test.cpp
#include <cmath>

#include "check.hpp"
#include "shooters_edge/client/model_shadow.hpp"

namespace {

namespace cl = shooters_edge::client;

cl::ShadowIn standing(int mode) {
  return {.mode = mode,
          .animated = true,
          .floor_found = true,
          .floor_up = 1,
          .height_units = 24,
          .radius_units = 30,
          .yaw_deg = 0};
}

TEST(mode_zero_casts_none_and_one_is_half_dark_at_any_height) {
  CHECK(cl::shadow_of(standing(0)).alpha == 0);
  CHECK(cl::shadow_of(standing(1)).alpha == cl::kShadowAlpha);
  cl::ShadowIn high = standing(1);
  high.height_units = 1000;
  CHECK(cl::shadow_of(high).alpha == cl::kShadowAlpha);
}

TEST(mode_two_fades_with_height_and_is_gone_at_four_radii) {
  cl::ShadowIn in = standing(2);
  in.height_units = 0;
  CHECK(cl::shadow_of(in).alpha == cl::kShadowAlpha);
  in.height_units = 60;  // half of four radii
  CHECK(std::abs(cl::shadow_of(in).alpha - cl::kShadowAlpha * 0.5f) < 1e-6f);
  in.height_units = 120;
  CHECK(cl::shadow_of(in).alpha == 0);
}

TEST(sprites_see_through_models_marked_ones_and_steep_or_missing_floors_cast_none) {
  cl::ShadowIn in = standing(1);
  in.animated = false;
  CHECK(cl::shadow_of(in).alpha == 0);
  in = standing(1);
  in.see_through = true;
  CHECK(cl::shadow_of(in).alpha == 0);
  in = standing(1);
  in.no_shadow = true;
  CHECK(cl::shadow_of(in).alpha == 0);
  in = standing(1);
  in.floor_found = false;
  CHECK(cl::shadow_of(in).alpha == 0);
  in = standing(1);
  in.floor_up = 0.4f;  // steeper than sixty degrees
  CHECK(cl::shadow_of(in).alpha == 0);
  in.floor_up = 0.6f;
  CHECK(cl::shadow_of(in).alpha > 0);
}

TEST(the_shadow_leans_along_the_way_the_model_faces) {
  cl::ShadowIn in = standing(1);
  const cl::Shadow east = cl::shadow_of(in);
  CHECK(std::abs(east.lean_x - cl::kShadowLean) < 1e-6f && std::abs(east.lean_y) < 1e-6f);
  in.yaw_deg = 90;
  const cl::Shadow north = cl::shadow_of(in);
  CHECK(std::abs(north.lean_x) < 1e-6f && std::abs(north.lean_y - cl::kShadowLean) < 1e-6f);
}

}  // namespace
