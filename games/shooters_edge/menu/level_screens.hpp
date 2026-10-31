// THE CAMPAIGN'S SCREENS over the game (porting/shooters-edge/campaign.md § The level's end, § The
// campaign's statistics), drawn on the classic 320 x 200 screen scaled by a whole number and
// centred, in the pack's own pictures (classic_art.hpp) when the packs carry them, else in the
// engine's type:
//
// - THE INTERMISSION: the level done. The screen darkened; the `complete` title centred at the
//   top (y 24); the `inter` labels (time, secrets, kills, 40 px apart) at (0, 56) with their
//   figures beside them at x 160, in the big figures of the glyph sheet's wad: the completed time
//   as minutes, a colon and two digits of seconds; secrets and kills as found, a slash, the total.
// - THE TEXT (a finale or a cutscene): the screen darkened and the text typed out: the first
//   `typed` characters shown, each line centred on the screen in the glyph sheet, the block's
//   middle at the screen's 40%.
// - THE SOLO PANEL (the scores key held in a campaign): two rows of the glyph sheet in a dark
//   strip above the status bar (y 152): the map's title, then the kills, the secrets and the time
//   on the map.
//
// ge:summary The campaign's screens: the intermission overlay, the typed finale, the solo panel
// ge:keywords campaign intermission complete inter big-figures num-colon num-slash finale
// ge:keywords cutscene typed-text solo-panel scores kills secrets time classic 320x200
// ge:skill ui
// ge:test tests/shooters_edge_level_screens_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// What the intermission and the solo panel show.
struct LevelStats {
  int minutes = 0, seconds = 0;
  int secrets = 0, total_secrets = 0;
  int kills = 0, total_kills = 0;
};

// The intermission over a `width` x `height` frame.
void draw_intermission(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                       const LevelStats& s);
// A finale's or a cutscene's text with its first `typed` characters shown ('\n' breaks a line).
void draw_level_text(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view text, size_t typed);
// The solo panel: `title` the map's message (or its name).
void draw_solo_panel(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view title, const LevelStats& s);

// The big figures' picture names a number is drawn in, left to right (a minus first for a
// negative one); `digits` pads with leading zeros to that many.
std::vector<std::string> figure_names(int n, int digits = 1);

}  // namespace shooters_edge::menu
