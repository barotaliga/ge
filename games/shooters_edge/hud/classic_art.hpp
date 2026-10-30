// THE CLASSIC PICTURES: the player's own copy of the classic content's status bar pictures, as
// classic-enhance leaves them in its local cache (`pictures/<name>.png`, each one picture of the
// pack's graphics archive decoded through its palette), read into the HUD's set (art.hpp) by
// what each shows. They are drawn at one picture pixel per art pixel, sampled nearest, so the
// classic status bar looks as it did; the counts on the inventory bar are drawn from the pack's
// own glyph sheet, its gold figures included.
//
// Nothing here ships or is committed: the pictures are read at run time from the player's
// cache. The crosshair is the glyph sheet's '+', as the classic game drew it; the pack has no
// health icon, so the looks that show one show the portrait.
//
// ge:summary Shooter's Edge's HUD drawn from the player's classic pictures in the local cache
// ge:keywords hud classic-art classic-enhance cache pictures png status-bar portrait digits
// ge:keywords glyph-sheet conchars crosshair
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "shooters_edge/hud/art.hpp"

namespace shooters_edge::hud {

// The classic set from the PNGs in `dir`; false with the first missing or unreadable picture
// named in `*error`.
bool classic_art(const std::filesystem::path& dir, HudArt* out, std::string* error);

}  // namespace shooters_edge::hud
