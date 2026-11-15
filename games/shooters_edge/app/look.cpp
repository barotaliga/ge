// The plugin's picture settings joined to the game's effects: lights by kind, the screen's tint.
//
// ge:summary Shooter's Edge's look: kept lights by kind and the toned screen tint
// ge:keywords look kept_lights toned_blend r_rocketlight r_explosionlight r_powerupglow
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

void toned_blend(const Picture& picture, const effects::Screen& s, double rgba[4]) {
  const effects::BlendShares k{.content = picture.content_blend,
                               .damage = picture.damage_blend,
                               .quad = picture.quad_blend,
                               .pent = picture.pent_blend,
                               .suit = picture.suit_blend,
                               .ring = picture.ring_blend};
  effects::screen_blend(s, rgba, k);
  rgba[3] *= picture.poly_blend ? std::clamp(picture.cshift_percent, 0.0, 100.0) / 100.0 : 0;
}

}  // namespace shooters_edge::look
