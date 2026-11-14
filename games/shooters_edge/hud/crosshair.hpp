// THE CROSSHAIR ELEMENT: the engine's crosshair settings (hud/crosshair.hpp: the mark, its colour,
// size and opacity) over Shooter's Edge's HUD art: mark 1 the set's own crosshair picture, a
// colour number from the set's palette (the classic set's; the free set's a grey).
//
// ge:summary Shooter's Edge's crosshair element: the engine's settings over the HUD set's art
// ge:keywords crosshair fill_crosshair crosshair-element
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include "hud/crosshair.hpp"
#include "hud/layout.hpp"
#include "shooters_edge/hud/art.hpp"

namespace shooters_edge::hud {

// The crosshair element's content this frame from the settings and the set (`dead`: none).
void fill_crosshair(ge::hud::Content& c, const ge::hud::CrosshairSettings& s, const HudArt& art,
                    bool dead);

}  // namespace shooters_edge::hud
