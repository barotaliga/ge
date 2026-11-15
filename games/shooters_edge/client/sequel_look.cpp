// The sequel's effect and render bits read into what the picture draws (sequel_look.hpp).
//
// ge:summary The sequel's look bits: see-through, spin, the glow pulse, the shells, the trails
// ge:keywords sequel render-fx shell glow pulse translucent rotate
// ge:skill render shooter
// ge:test tests/shooters_edge_sequel_look_test.cpp
#include "shooters_edge/client/sequel_look.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::client {

namespace {

// The shell's colour by its bits, in this order: all three of red, green and blue white; then
// red, blue and double damage (green left out): red, magenta (red with blue or double), blue,
// cyan (blue with double), gold (double alone); then half damage (0.56, 0.59, 0.45) with green
// lifting its green channel to 1 (green alone: pure green).
void shell_colour(uint32_t rf, float rgb[3]) {
  const bool r = (rf & kRfShellRed) != 0, g = (rf & kRfShellGreen) != 0;
  const bool b = (rf & kRfShellBlue) != 0, dd = (rf & kRfShellDouble) != 0;
  const auto set = [&](float x, float y, float z) { rgb[0] = x, rgb[1] = y, rgb[2] = z; };
  if (r && g && b) return set(1, 1, 1);
  if (r || b || dd) {
    if (r) return (b || dd) ? set(1, 0, 1) : set(1, 0, 0);
    if (b) return dd ? set(0, 1, 1) : set(0, 0, 1);
    return set(0.9f, 0.7f, 0);
  }
  const bool half = (rf & kRfShellHalf) != 0;
  if (half)
    set(0.56f, 0.59f, 0.45f);
  else
    set(0, 0, 0);
  if (g) rgb[1] = 1;
}

}  // namespace

SequelLook sequel_look(uint32_t effects, uint32_t render_fx) {
  SequelLook look;
  // The powers the client turns into shells.
  uint32_t rf = render_fx;
  if ((effects & kSqQuad) != 0) rf |= kRfShellBlue;
  if ((effects & kSqInvulnerable) != 0) rf |= kRfShellRed;
  if ((effects & kSqDoubleDamage) != 0) rf |= kRfShellDouble;
  if ((effects & kSqHalfDamage) != 0) rf |= kRfShellHalf;
  const uint32_t shells =
      kRfShellRed | kRfShellGreen | kRfShellBlue | kRfShellDouble | kRfShellHalf;
  const bool powered =
      (effects & (kSqQuad | kSqInvulnerable | kSqDoubleDamage | kSqHalfDamage)) != 0;
  look.turns = (effects & kSqRotate) != 0;
  if (((effects & kSqColourShell) != 0 || powered) && (rf & shells) != 0) {
    look.shell = true;
    shell_colour(rf, look.shell_rgb);
    look.pulse = (rf & kRfGlow) != 0;  // on the shell's colour; the plain copy keeps none
    return look;                       // the copy under it drawn plain
  }
  if ((rf & kRfBeam) != 0)
    look.alpha = 0.3f;
  else if (rf == kRfTranslucent)
    look.alpha = 0.7f;
  if ((effects & kSqBfg) != 0) look.alpha = 0.3f;
  look.pulse = (rf & kRfGlow) != 0;
  // Its trail, the first of these it carries.
  if ((effects & kSqRocket) != 0)
    look.missile = SequelLook::Missile::rocket;
  else if ((effects & (kSqBlaster | kSqHyperblaster)) != 0)
    look.missile = SequelLook::Missile::bolt;
  else if ((effects & kSqGrenade) != 0)
    look.missile = SequelLook::Missile::grenade;
  else if ((effects & kSqBfg) != 0)
    look.missile = SequelLook::Missile::bfg;
  return look;
}

void sequel_pulse(float rgb[3], double t) {
  const float gain = static_cast<float>(0.1 * std::sin(7 * t));
  for (int c = 0; c < 3; c++) rgb[c] = std::max(rgb[c] * 0.8f, rgb[c] + gain);
}

}  // namespace shooters_edge::client
