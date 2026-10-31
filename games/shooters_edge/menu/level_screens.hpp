// THE CAMPAIGN'S SCREENS over the game (porting/shooters-edge/campaign.md § The level's end, § The
// campaign's statistics; porting/shooters-edge/scoreboard.md), drawn on the classic 320 x 200
// screen scaled by a whole number and centred, in the pack's own pictures (classic_art.hpp) when
// the packs carry them, else in the engine's type:
//
// - THE INTERMISSION: the level done. The screen darkened; the `complete` title at (64, 24); the
//   `inter` labels (time, secrets, kills) at (0, 56); their figures in the big figures of the
//   glyph sheet's wad: the minutes right-aligned in a field of three from x 160 on y 64, the colon
//   at 234 and the two digits of seconds at 246 and 266; the secrets and the kills found in a
//   field of three from 160, the slash at 232 and the total in a field of three from 240, on y 104
//   and 144.
// - THE LEVEL'S STATISTICS (the scores key held in a campaign, `scr_levelstats 1`): the same
//   labels and figures, live, with the map's title in the glyph sheet where the title picture
//   would be, over the view as it is (not darkened).
// - THE SOLO SCOREBAR (the scores key held in a campaign): the status bar replaced by the wad's
//   `scorebar` strip at (0, 176) and on it, in the glyph sheet: `Monsters:` and `Secrets :` with
//   found and total in fields of three at (8, 180) and (8, 188); `Time :` the minutes in three, a
//   colon and two digits at (184, 180); the map's title centred on x 232 at y 188.
// - THE TEXT (a finale or a cutscene): the screen darkened and the text typed out: the first
//   `typed` characters shown, each line centred on the screen in the glyph sheet, the block's
//   middle at the screen's 40%.
//
// ge:summary The campaign's screens: the intermission, the level's statistics, the solo scorebar
// ge:keywords campaign intermission complete inter big-figures num-colon num-slash finale
// ge:keywords cutscene typed-text solo-scorebar scorebar scores kills secrets time classic 320x200
// ge:keywords level-stats scr_levelstats classic-type
// ge:skill ui
// ge:test tests/shooters_edge_level_screens_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_view.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// What the intermission, the level's statistics and the solo scorebar show.
struct LevelStats {
  int minutes = 0, seconds = 0;
  int secrets = 0, total_secrets = 0;
  int kills = 0, total_kills = 0;
};

// The classic screen's type: the glyph sheet (`glyphs`), or the engine's font in the sheet's cells
// when the packs have none.
struct ClassicType {
  const ClassicPen& pen;
  bool glyphs;

  // `s` from (x, y); `alt` the sheet's second set (the engine's font: the gold ink).
  void text(std::string_view s, float x, float y, bool alt = false) const;
  // `s` centred on the screen's x `cx` at row `y`.
  void centred(std::string_view s, float y, float cx = 160) const;
  // One glyph by its code (16, 17: the brackets; 29 30 31: a rule's pieces).
  void glyph(uint8_t code, float x, float y) const;
};

// `n` right-aligned in `width` characters ("%3i"); `keep` > 0 keeps only its first `keep`.
std::string padded(int n, int width, int keep = 0);

// The intermission over a `width` x `height` frame.
void draw_intermission(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                       const LevelStats& s);
// A finale's or a cutscene's text with its first `typed` characters shown ('\n' breaks a line).
void draw_level_text(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     std::string_view text, size_t typed);
// The level's statistics while the scores key is held: `title` the map's message (or its name).
void draw_level_stats(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                      std::string_view title, const LevelStats& s);
// The solo scorebar in the status bar's place: `title` the map's message (or its name).
void draw_solo_scorebar(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                        std::string_view title, const LevelStats& s);

// The big figures' picture names a number is drawn in, left to right (a minus first for a
// negative one); `digits` pads with leading zeros to that many. (The fields of the intermission
// are right-aligned instead: `field_names`.)
std::vector<std::string> figure_names(int n, int digits = 1);
// A number in a field of `digits` big figures: its names and the cells it is moved right by (the
// field right-aligned); one longer than the field keeps only its last `digits` characters.
std::vector<std::string> field_names(int n, int digits, int* skip);

}  // namespace shooters_edge::menu
