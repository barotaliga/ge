// THE WORDS IN A DEMO VIDEO'S CORNER: a few lines in the picture's upper left that say what the
// video shows, drawn over the world and the HUD by `-demovideo` (demo_video.hpp) when
// `demovideo_text` is on (video_cfg.hpp).
//
// THE LINES (`demo_caption`), each from the demo as it stands at the picture's time:
// - the map: its name, and its title (worldspawn's `message`) when it has one, in the gold half of
//   the glyph sheet;
// - the clock: the picture's time on the play bar's timeline (timedemo.hpp's), `m:ss`;
// - whose eyes: a client or multiview recording's tracked player and their frags; a single-player
//   recording's kills and secrets out of the level's totals, when it has any;
// - the match: in a team game the teams' frags summed, best first (`red 23  blue 19`); in a
//   free-for-all of two or more the leader and their frags.
// A line with nothing to say is left out, so a short recording shows only the map and the clock.
//
// THE TYPE is the packs' own console font when they carry one: the 128 x 128 glyph sheet
// (`conchars` in `gfx.wad`, index 0 see-through, in `gfx/palette.lmp`'s colours), or
// classic-enhance's copy of it in the pack's cache (`pictures/enhanced/conchars.png`, upscaled, or
// `pictures/conchars.png`), which wins when it is there, so an enhanced sheet is what a video
// shows. With neither it is the engine's rounded charset (charset/charset.hpp), the gold lines
// tinted. A line is `caption_size` px tall (8 px at 360 lines of picture, 24 at 1080), so the
// original sheet's cells land on a whole number of screen pixels.
//
// ge:summary The demo video's upper-left words: the map, the clock, whose eyes, the match's score
// ge:keywords demo-video caption overlay text upper-left conchars glyph-sheet charset pak-font
// ge:keywords demovideo_text map-title clock team-score frags pov watermark
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_demo_caption_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

struct CaptionLine {
  std::string text;
  bool gold = false;  // drawn from the glyph sheet's second half (the charset: tinted)
};

// The lines for `s` at `t` s on the demo's timeline; `title` the map's title ("": none).
std::vector<CaptionLine> demo_caption(const DemoState& s, std::string_view title, double t);

// `t` s as `m:ss` (`h:mm:ss` from an hour).
std::string caption_clock(double t);

// A line's height in pixels for a picture `height` px tall: 8 px for every 360 lines, at least 8.
int caption_size(int height);

// A file by its path in the game: a view into `store` or into a pack, empty when none has it
// (files.hpp's `find_file`).
using CaptionFind =
    std::function<std::span<const uint8_t>(std::string_view path, std::vector<uint8_t>& store)>;

// The packs' glyph sheet (`gfx.wad`'s `conchars` in `gfx/palette.lmp`'s colours, index 0
// see-through) into `*out`; false with the reason when the packs lack either.
bool caption_glyphs_from_packs(const CaptionFind& find, ge::ui::Image* out, std::string* why);
// A glyph sheet from a PNG (classic-enhance's `pictures/conchars.png`); false when it is missing,
// not a PNG, or not 16 cells a side (its sides a multiple of 16).
bool caption_glyphs_from_png(const std::filesystem::path& path, ge::ui::Image* out);

// `lines` in the upper left of a `width` x `height` picture in `glyphs` (empty: the charset).
void draw_caption(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                  std::span<const CaptionLine> lines);

}  // namespace shooters_edge::client
