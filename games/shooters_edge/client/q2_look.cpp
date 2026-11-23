// Quake II's effect and render bits read into what the picture draws (q2_look.hpp).
//
// ge:summary Quake II's look bits: see-through, spin, the glow pulse, the shells, the trails
// ge:keywords q2 render-fx shell glow pulse translucent rotate
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_look_test.cpp
#include "shooters_edge/client/q2_look.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::client {

namespace {

// The shell's colour by its bits, in this order: all three of red, green and blue white; then
// red, blue and double damage (green left out): red, magenta (red with blue or double), blue,
// cyan (blue with double), gold (double alone); then half damage (0.56, 0.59, 0.45) with green
// lifting its green channel to 1 (green alone: pure green).
void shell_colour(uint32_t rf, float rgb[3]) {
  // The bits mix in turn from black: the light green, then half damage, then double damage's red
  // and green, then red, green and blue each forced to full.
  const auto set = [&](float x, float y, float z) { rgb[0] = x, rgb[1] = y, rgb[2] = z; };
  set(0, 0, 0);
  if ((rf & kRfShellLiteGreen) != 0) set(0.56f, 0.93f, 0.56f);
  if ((rf & kRfShellHalf) != 0) set(0.56f, 0.59f, 0.45f);
  if ((rf & kRfShellDouble) != 0) rgb[0] = 0.9f, rgb[1] = 0.7f;
  if ((rf & kRfShellRed) != 0) rgb[0] = 1;
  if ((rf & kRfShellGreen) != 0) rgb[1] = 1;
  if ((rf & kRfShellBlue) != 0) rgb[2] = 1;
}

}  // namespace

int q2_texture_frame(uint32_t effects, int frame, double t) {
  const auto ms = static_cast<int64_t>(t * 1000);
  const int half = static_cast<int>(ms * 2 / 1000);
  if ((effects & 0x400u) != 0) return half & 1;
  if ((effects & 0x800u) != 0) return 2 + (half & 1);
  if ((effects & 0x1000u) != 0) return half;
  if ((effects & 0x2000u) != 0) return static_cast<int>(ms / 100);
  return frame;
}

double q2_bob_units(double t) { return 5 * std::sin(t * 1000 / 400); }

Q2Look q2_look(uint32_t effects, uint32_t render_fx) {
  Q2Look look;
  look.bleeds = (effects & kSqGib) != 0;
  // A green gib's trail, when no branch before it (the missiles, the gib, the flies, the trap,
  // the flags, the tag, the tracker's) takes the entity.
  constexpr uint32_t kBefore = kSqRocket | kSqBlaster | kSqHyperblaster | kSqGib | kSqGrenade |
                               kSqFlies | kSqBfg | kSqTrap | kSqFlag1 | kSqFlag2 | kSqTagTrail |
                               kSqTrackerTrail | kSqTracker;
  if ((effects & kSqGreenGib) != 0 && (effects & kBefore) == 0)
    look.bleeds = look.green_blood = true;
  look.flies = (effects & kSqFlies) != 0;
  look.power_screen = (effects & kSqPowerScreen) != 0;
  // The powers the client turns into shells.
  uint32_t rf = render_fx;
  if ((effects & kSqQuad) != 0) rf |= kRfShellBlue;
  if ((effects & kSqInvulnerable) != 0) rf |= kRfShellRed;
  if ((effects & kSqDoubleDamage) != 0) rf |= kRfShellDouble;
  if ((effects & kSqHalfDamage) != 0) rf |= kRfShellHalf;
  const uint32_t shells = kRfShellRed | kRfShellGreen | kRfShellBlue | kRfShellDouble |
                          kRfShellHalf | kRfShellLiteGreen;
  const bool powered =
      (effects & (kSqQuad | kSqInvulnerable | kSqDoubleDamage | kSqHalfDamage)) != 0;
  look.turns = (effects & kSqRotate) != 0;
  look.bobs = (effects & kSqBob) != 0;
  look.full_bright = (render_fx & kRfFullBright) != 0;
  look.min_light = (render_fx & kRfMinLight) != 0;
  look.no_shadow = (render_fx & kRfNoShadow) != 0;
  look.disguise = (render_fx & kRfUseDisguise) != 0;
  look.spinning = !look.turns && (effects & kSqSpinningLights) != 0;
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
  if ((effects & kSqPlasma) != 0) look.alpha = 0.6f;
  if ((effects & kSqSphereTrans) != 0) look.alpha = (effects & kSqTrackerTrail) != 0 ? 0.6f : 0.3f;
  look.pulse = (rf & kRfGlow) != 0;
  // Its trail, the first of these it carries.
  if ((effects & kSqRocket) != 0)
    look.missile = Q2Look::Missile::rocket;
  else if ((effects & (kSqBlaster | kSqHyperblaster)) != 0)
    look.missile = Q2Look::Missile::bolt;
  else if ((effects & kSqGrenade) != 0)
    look.missile = Q2Look::Missile::grenade;
  else if ((effects & kSqBfg) != 0)
    look.missile = Q2Look::Missile::bfg;
  else if ((effects & (kSqGib | kSqFlies)) != 0) {
    // their own branches: no trail of these
  } else if ((effects & kSqTrap) != 0)
    look.missile = Q2Look::Missile::trap;
  else if ((effects & kSqFlag1) != 0)
    look.missile = Q2Look::Missile::flag1;
  else if ((effects & kSqFlag2) != 0)
    look.missile = Q2Look::Missile::flag2;
  else if ((effects & kSqTagTrail) != 0)
    look.missile = Q2Look::Missile::tag;
  else if ((effects & kSqTrackerTrail) != 0)
    look.missile = (effects & kSqTracker) != 0 ? Q2Look::Missile::tracker_dark
                                               : Q2Look::Missile::tracker_shell;
  else if ((effects & kSqTracker) != 0)
    look.missile = Q2Look::Missile::tracker;
  else if ((effects & kSqGreenGib) == 0) {
    if ((effects & kSqIonRipper) != 0)
      look.missile = Q2Look::Missile::ion;
    else if ((effects & kSqBlueHyperblaster) != 0)
      look.missile = Q2Look::Missile::blue;
    else if ((effects & kSqPlasma) != 0)
      look.missile = Q2Look::Missile::plasma;
  }
  return look;
}

bool q2_infra_red(int rdflags, uint32_t render_fx, bool shell, bool full_bright) {
  return (rdflags & kQ2RdIrGoggles) != 0 && (render_fx & kRfIrVisible) != 0 && !shell &&
         !full_bright;
}

void q2_pulse(float rgb[3], double t) {
  const float gain = static_cast<float>(0.1 * std::sin(7 * t));
  for (int c = 0; c < 3; c++) rgb[c] = std::max(rgb[c] * 0.8f, rgb[c] + gain);
}

PowerLook power_look(const PowerIn& in, bool power_shells, bool item_pulse) {
  PowerLook out;
  if (in.q2) {
    out.shell = in.shell && power_shells;
    out.pulse = in.pulse && (in.shell ? out.shell : item_pulse);  // a shell pulses its colour
    for (int c = 0; c < 3; c++) out.shell_rgb[c] = in.shell_rgb[c];
    return out;
  }
  const bool blue = (in.effects & kFirstQuad) != 0, red = (in.effects & kFirstPentagram) != 0;
  if (power_shells && in.player && (blue || red)) {  // a power's pickup glows, never wears one
    out.shell = true;
    out.shell_rgb[0] = red ? 1.0f : 0.0f;
    out.shell_rgb[2] = blue ? 1.0f : 0.0f;
  }
  out.pulse = item_pulse && !out.shell && !in.player && (in.model_flags & kFirstRotates) != 0;
  return out;
}

std::string q2_disguised(std::string_view look) {
  const size_t cut = look.find('/');
  std::string folder(cut == std::string_view::npos || cut == 0 ? std::string_view("male")
                                                               : look.substr(0, cut));
  return folder + "/disguise";
}

}  // namespace shooters_edge::client
