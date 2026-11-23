// Glow discs for the moment's lights: the rule (flash_blend.hpp).
//
// ge:summary Glow discs for the moment's lights (gl_flashblend): size, place, colour, eye inside
// ge:keywords gl_flashblend flash_glow flash_tint
// ge:skill classic render
// ge:test tests/classic_flash_blend_test.cpp
#include "classic/flash_blend.hpp"

#include <algorithm>
#include <cmath>

namespace ge::classic {

namespace {

constexpr double kQuakeGlow[3] = {0.2, 0.1, 0};
constexpr double kWorldGlows[4][3] = {
    {0.2, 0.1, 0.05}, {0.05, 0.05, 0.3}, {0.5, 0.05, 0.05}, {0.5, 0.05, 0.4}};

// The nearest of QuakeWorld's four by direction, so a light's strength does not change its pick.
const double* world_glow(const double c[3]) {
  const double len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
  if (len <= 0) return kWorldGlows[0];
  int best = 0;
  double most = -2;
  for (int i = 0; i < 4; i++) {
    const double* g = kWorldGlows[i];
    const double gl = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    const double cosine = (c[0] * g[0] + c[1] * g[1] + c[2] * g[2]) / (len * gl);
    if (cosine > most) most = cosine, best = i;
  }
  return kWorldGlows[best];
}

}  // namespace

Glow flash_glow(const FlashIn& in) {
  Glow g;
  if (in.radius_units <= 0) return g;
  if (in.game == FlashGame::quakeworld && in.own) return g;
  const double r = kFlashDiscShare * in.radius_units;
  double to_eye[3];
  for (int a = 0; a < 3; a++) to_eye[a] = in.eye[a] - in.light[a];
  const double dist =
      std::sqrt(to_eye[0] * to_eye[0] + to_eye[1] * to_eye[1] + to_eye[2] * to_eye[2]);
  if (dist < r) {
    g.tint = std::min(1.0, in.radius_units * kFlashTintPerUnit);
    return g;
  }
  g.disc = true;
  g.radius_units = r;
  for (int a = 0; a < 3; a++) g.centre[a] = in.light[a] + to_eye[a] / dist * r;
  const double* c = in.game == FlashGame::quake ? kQuakeGlow : world_glow(in.colour);
  for (int a = 0; a < 3; a++) g.colour[a] = c[a];
  return g;
}

void flash_tint(double tint, double rgba[4]) {
  if (tint <= 0) return;
  const double was = rgba[3];
  const double now = was + (1 - was) * tint;
  for (int a = 0; a < 3; a++)
    rgba[a] = now > 0 ? (rgba[a] * was * (1 - tint) + kFlashTint[a] * tint) / now : kFlashTint[a];
  rgba[3] = now;
}

}  // namespace ge::classic
