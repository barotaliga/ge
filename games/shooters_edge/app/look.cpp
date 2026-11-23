// The plugin's picture settings joined to the game's effects: lights by kind, the screen's tint.
//
// ge:summary Shooter's Edge's look: kept lights by kind and the toned screen tint
// ge:keywords look kept_lights toned_blend toned_tints gl_damageblend_frac r_rocketlight r_explosionlight r_powerupglow
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/look.hpp"

#include <algorithm>

namespace shooters_edge::look {

std::span<const effects::Light> kept_lights(const Picture& picture,
                                            std::span<const effects::Light> lights,
                                            std::vector<effects::Light>& out) {
  out.clear();
  if (picture.rocket_light && picture.explosion_light && picture.power_glow) return lights;
  for (const effects::Light& l : lights) {
    const bool off = (l.group == effects::kLightRocket && !picture.rocket_light) ||
                     (l.group == effects::kLightExplosion && !picture.explosion_light) ||
                     (l.group == effects::kLightPower && !picture.power_glow);
    if (!off) out.push_back(l);
  }
  return out;
}

namespace {

// The player's share of each layer (the v_ cvars, cl_bonusflash).
effects::BlendShares shares(const Picture& picture) {
  return {.content = picture.content_blend,
          .damage = picture.damage_blend,
          .quad = picture.quad_blend,
          .pent = picture.pent_blend,
          .suit = picture.suit_blend,
          .ring = picture.ring_blend,
          .bonus = picture.bonus_flash ? 1.0 : 0.0};
}

// gl_cshiftpercent's share of every tint, none with gl_polyblend 0.
double all_tints(const Picture& picture) {
  return picture.poly_blend ? std::clamp(picture.cshift_percent, 0.0, 100.0) / 100.0 : 0;
}

}  // namespace

void toned_blend(const Picture& picture, const effects::Screen& s, double rgba[4]) {
  effects::screen_blend(s, rgba, shares(picture));
  rgba[3] *= all_tints(picture);
}

Tints toned_tints(const Picture& picture, const effects::Screen& s, bool rules,
                  const double whole[4], const double apart[4], const double damage[4]) {
  Tints t;
  const double all = all_tints(picture);
  const bool framed = picture.damage_frac > 0;
  if (rules) {
    const bool split = framed && damage[3] > 0;
    for (int k = 0; k < 4; k++) {
      t.flat[k] = split ? apart[k] : whole[k];
      t.edge[k] = split ? damage[k] : 0;
    }
    t.flat[3] *= all;
    t.edge[3] *= all;
    return t;
  }
  if (!framed) {
    toned_blend(picture, s, t.flat);
    return t;
  }
  effects::BlendShares rest = shares(picture);  // the flash off the whole view, alone round it
  rest.damage = 0;
  effects::screen_blend(s, t.flat, rest);
  t.flat[3] *= all;
  effects::Screen hit;
  hit.hurt = s.hurt;
  hit.armour = s.armour;
  hit.classic = s.classic;
  for (int k = 0; k < 3; k++) hit.hurt_rgb[k] = s.hurt_rgb[k];
  hit.air = 0;
  toned_blend(picture, hit, t.edge);
  return t;
}

}  // namespace shooters_edge::look
