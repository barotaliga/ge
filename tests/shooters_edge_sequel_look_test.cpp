// Tests: the sequel's effect and render bits read into the picture's look
// (games/shooters_edge/client/sequel_look.hpp): the shells' colours in their order (quad blue,
// invulnerability red, both magenta, a power shield green, god mode white), the plain copy under a
// shell, see-through (translucent alone, a beam, the BFG), an item's spin and glow, the glow's
// pulse and its floor, the shots' trails by their bits, and a body's piece bleeding by its own.
// The shells and the pulse lifted to the first game (`power_look`): off by its switches, its quad
// and pentagram bits blue and red, its turning items pulsing; the sequel's own held to its switch.
//
// ge:summary Tests: the sequel's look bits: shells, see-through, spin, glow pulse, trails
// ge:keywords test sequel render-fx shell quad invulnerability glow pulse translucent trails
// ge:keywords power_look r_powershells r_itempulse first-game-shells
// ge:skill render shooter testing
// ge:test tests/shooters_edge_sequel_look_test.cpp
#include <cmath>

#include "check.hpp"
#include "shooters_edge/client/demo_fx.hpp"
#include "shooters_edge/client/sequel_look.hpp"

namespace {

namespace cl = shooters_edge::client;

bool rgb_is(const cl::SequelLook& l, float r, float g, float b) {
  return l.shell && std::abs(l.shell_rgb[0] - r) < 1e-6f && std::abs(l.shell_rgb[1] - g) < 1e-6f &&
         std::abs(l.shell_rgb[2] - b) < 1e-6f;
}

TEST(the_powers_wear_their_shells_in_their_order) {
  CHECK(rgb_is(cl::sequel_look(cl::kSqQuad, 0), 0, 0, 1));
  CHECK(rgb_is(cl::sequel_look(cl::kSqInvulnerable, 0), 1, 0, 0));
  CHECK(rgb_is(cl::sequel_look(cl::kSqQuad | cl::kSqInvulnerable, 0), 1, 0, 1));
  // A power shield's hit: green, with the colour-shell effect bit the server sends with it.
  CHECK(rgb_is(cl::sequel_look(cl::kSqColourShell, cl::kRfShellGreen), 0, 1, 0));
  // Quad with a shield's hit: blue (green yields to red, blue and double damage).
  CHECK(rgb_is(cl::sequel_look(cl::kSqQuad | cl::kSqColourShell, cl::kRfShellGreen), 0, 0, 1));
  // God mode: all three, white.
  const uint32_t god = cl::kRfShellRed | cl::kRfShellGreen | cl::kRfShellBlue;
  CHECK(rgb_is(cl::sequel_look(cl::kSqColourShell, god), 1, 1, 1));
  CHECK(rgb_is(cl::sequel_look(cl::kSqDoubleDamage, 0), 0.9f, 0.7f, 0));
  CHECK(rgb_is(cl::sequel_look(cl::kSqDoubleDamage | cl::kSqQuad, 0), 0, 1, 1));
  CHECK(rgb_is(cl::sequel_look(cl::kSqHalfDamage, 0), 0.56f, 0.59f, 0.45f));
  // Shell bits without the colour-shell effect bit draw none.
  CHECK(!cl::sequel_look(0, cl::kRfShellRed).shell);
}

TEST(the_copy_under_a_shell_is_plain) {
  const cl::SequelLook l =
      cl::sequel_look(cl::kSqColourShell | cl::kSqRotate, cl::kRfShellGreen | cl::kRfTranslucent);
  CHECK(l.shell);
  CHECK(l.alpha == 1);  // not see-through: its render bits are dropped
  CHECK(l.turns);       // an effect bit still turns it
}

TEST(see_through_by_its_bits) {
  CHECK(cl::sequel_look(0, cl::kRfTranslucent).alpha == 0.7f);
  CHECK(cl::sequel_look(0, cl::kRfBeam | cl::kRfTranslucent).alpha == 0.3f);
  CHECK(cl::sequel_look(cl::kSqBfg, 0).alpha == 0.3f);
  // Translucent with another bit and no beam: the client sets no alpha of its own; drawn solid.
  CHECK(cl::sequel_look(0, cl::kRfTranslucent | cl::kRfGlow).alpha == 1);
  CHECK(cl::sequel_look(0, 0).alpha == 1);
}

TEST(an_item_turns_and_its_light_pulses) {
  const cl::SequelLook item = cl::sequel_look(cl::kSqRotate, cl::kRfGlow);
  CHECK(item.turns && item.pulse && !item.shell && item.alpha == 1);
  // The pulse adds 0.1 sin(7 t) a channel, never below 80 % of the light before it.
  float lit[3] = {0.5f, 0.5f, 0.5f};
  cl::sequel_pulse(lit, 3.14159265358979 / 14);  // sin = 1
  CHECK(std::abs(lit[0] - 0.6f) < 1e-5f);
  float dark[3] = {0.2f, 0.2f, 0.2f};
  cl::sequel_pulse(dark, 3 * 3.14159265358979 / 14);  // sin = -1: 0.1, held at 0.16
  CHECK(std::abs(dark[1] - 0.16f) < 1e-5f);
}

TEST(the_shots_trail_by_their_bits) {
  cl::Missile m;
  CHECK(cl::sequel_missile(int(cl::kSqRocket), m) && m.rocket && !m.grenade && !m.bolt);
  CHECK(cl::sequel_missile(int(cl::kSqGrenade), m) && m.rocket && m.grenade);
  CHECK(cl::sequel_missile(int(cl::kSqBlaster), m) && m.bolt && !m.rocket);
  CHECK(cl::sequel_missile(int(cl::kSqHyperblaster), m) && m.bolt);
  CHECK(cl::sequel_missile(int(cl::kSqBfg), m) && m.bfg && !m.bolt);
  CHECK(!cl::sequel_missile(int(cl::kSqGib), m));
  CHECK(!cl::sequel_missile(int(cl::kSqRotate), m));
  // A rocket's trail first when it carries more than one.
  CHECK(cl::sequel_missile(int(cl::kSqRocket | cl::kSqBlaster), m) && m.rocket && !m.bolt);
}

TEST(a_body_piece_bleeds_by_its_gib_bit) {
  CHECK(cl::sequel_look(cl::kSqGib, 0).bleeds);
  CHECK(cl::sequel_look(cl::kSqGib | cl::kSqRotate, 0).bleeds);
  CHECK(!cl::sequel_look(cl::kSqRotate, 0).bleeds);
  CHECK(!cl::sequel_look(cl::kSqRocket, 0).bleeds);
}

TEST(the_first_game_takes_the_shells_and_the_pulse_only_when_asked) {
  cl::PowerIn quad;
  quad.player = true;
  quad.effects = cl::kFirstQuad;
  CHECK(!cl::power_look(quad, false, false).shell);  // its classic default: the glow alone
  cl::PowerLook l = cl::power_look(quad, true, true);
  CHECK(l.shell && l.shell_rgb[0] == 0 && l.shell_rgb[2] == 1);
  quad.effects = cl::kFirstQuad | cl::kFirstPentagram;  // quad and the pentagram: magenta
  l = cl::power_look(quad, true, true);
  CHECK(l.shell && l.shell_rgb[0] == 1 && l.shell_rgb[1] == 0 && l.shell_rgb[2] == 1);
  cl::PowerIn pickup;  // the quad's pickup glows by the same bit: no shell, its pulse instead
  pickup.effects = cl::kFirstQuad;
  pickup.model_flags = cl::kFirstRotates;
  CHECK(!cl::power_look(pickup, true, true).shell && cl::power_look(pickup, true, true).pulse);
  cl::PowerIn item;
  item.model_flags = cl::kFirstRotates;
  CHECK(cl::power_look(item, true, true).pulse);
  CHECK(!cl::power_look(item, true, false).pulse);
  item.model_flags = 0;  // a model that does not turn: no item
  CHECK(!cl::power_look(item, true, true).pulse);
  // The sequel's own shell and glow, by its bits, held to the same switches.
  cl::PowerIn sq;
  sq.sequel = sq.shell = true;
  sq.shell_rgb[2] = 1;
  CHECK(cl::power_look(sq, true, true).shell);
  CHECK(!cl::power_look(sq, false, true).shell);
  cl::PowerIn glowing;
  glowing.sequel = glowing.pulse = true;
  CHECK(cl::power_look(glowing, false, true).pulse);
  CHECK(!cl::power_look(glowing, false, false).pulse);
}

}  // namespace
