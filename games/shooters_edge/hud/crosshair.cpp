// The crosshair element's content each frame, from the engine's settings and the HUD set.
//
// ge:summary Shooter's Edge's crosshair element filled from the engine's settings and the set
// ge:keywords crosshair fill_crosshair
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/crosshair.hpp"

namespace shooters_edge::hud {

void fill_crosshair(ge::hud::Content& c, const ge::hud::CrosshairSettings& s, const HudArt& art,
                    bool dead) {
  bool shown = false;
  const ge::hud::Picture p =
      ge::hud::crosshair_picture(s, art.crosshair, art.density, art.palette, &shown);
  c.image = p.image;
  c.density = p.density;
  c.color = p.tint;
  c.on = c.on && !dead && shown;
}

}  // namespace shooters_edge::hud
