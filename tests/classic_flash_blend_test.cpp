// Tests: glow discs for the moment's lights (plugins/classic/flash_blend.hpp): the disc's size and
// its middle pulled toward the eye, Quake's one colour and QuakeWorld's nearest of four, the eye
// inside a light tinting the screen instead, QuakeWorld's own lights making none.
//
// ge:summary Tests: glow discs for the moment's lights: size, place, colours, the eye inside
// ge:keywords test gl_flashblend flash_glow flash_tint glow-disc
// ge:skill classic render testing
// ge:test tests/classic_flash_blend_test.cpp
#include <cmath>

#include "check.hpp"
#include "classic/flash_blend.hpp"

namespace {

namespace cl = ge::classic;

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

cl::FlashIn blast(cl::FlashGame game) {
  cl::FlashIn in;
  in.radius_units = 350;
  in.eye[0] = 1000;
  in.game = game;
  return in;
}

TEST(a_disc_is_a_third_of_the_light_across_pulled_toward_the_eye) {
  const cl::Glow g = cl::flash_glow(blast(cl::FlashGame::quake));
  CHECK(g.disc && g.tint == 0);
  CHECK(near(g.radius_units, 122.5));
  CHECK(near(g.centre[0], 122.5) && near(g.centre[1], 0) && near(g.centre[2], 0));
}

TEST(quake_glows_dim_orange_and_quakeworld_takes_the_nearest_of_four) {
  cl::FlashIn in = blast(cl::FlashGame::quake);
  in.colour[0] = 0.1, in.colour[1] = 0.2, in.colour[2] = 1;  // blue: Quake does not care
  cl::Glow g = cl::flash_glow(in);
  CHECK(near(g.colour[0], 0.2) && near(g.colour[1], 0.1) && near(g.colour[2], 0));
  in.game = cl::FlashGame::quakeworld;
  g = cl::flash_glow(in);
  CHECK(near(g.colour[0], 0.05) && near(g.colour[1], 0.05) && near(g.colour[2], 0.3));
  in.colour[0] = 1, in.colour[1] = 0.1, in.colour[2] = 0.8;  // the pentagram's purple
  g = cl::flash_glow(in);
  CHECK(near(g.colour[0], 0.5) && near(g.colour[2], 0.4));
  in.colour[0] = 1, in.colour[1] = 0.5, in.colour[2] = 0.2;  // a rocket's orange
  g = cl::flash_glow(in);
  CHECK(near(g.colour[0], 0.2) && near(g.colour[1], 0.1) && near(g.colour[2], 0.05));
}

TEST(the_eye_inside_a_light_tints_the_screen_and_draws_no_disc) {
  cl::FlashIn in = blast(cl::FlashGame::quake);
  in.eye[0] = 100;  // under 122.5
  const cl::Glow g = cl::flash_glow(in);
  CHECK(!g.disc);
  CHECK(near(g.tint, 0.105));
  double rgba[4] = {0, 0, 0, 0};
  cl::flash_tint(g.tint, rgba);
  CHECK(near(rgba[0], 1) && near(rgba[1], 0.5) && near(rgba[2], 0) && near(rgba[3], 0.105));
}

TEST(quakeworld_draws_no_disc_for_the_players_own_light_and_none_without_a_radius) {
  cl::FlashIn in = blast(cl::FlashGame::quakeworld);
  in.own = true;
  CHECK(!cl::flash_glow(in).disc);
  in.game = cl::FlashGame::quake;
  CHECK(cl::flash_glow(in).disc);
  in.radius_units = 0;
  CHECK(!cl::flash_glow(in).disc && cl::flash_glow(in).tint == 0);
}

}  // namespace
