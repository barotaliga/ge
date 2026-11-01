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
// THE MESSAGES (`DemoNotices`): what the recorded player read, as the classic client showed it.
// Every print the demo carries for them (the pickups, the deaths, the server's words, and the chat:
// `say` and `say_team` alike, a team's lines only in its own players' recordings) goes under the
// words above as a notify line, the newest last, `kNoticeLines` at most, each for
// `kNoticeSeconds` (a chat line `kChatSeconds`, long enough to read), the chat in the gold half; a
// centre print (and the episode's closing text) is drawn in the middle of the picture for
// `kCentreSeconds`, longer for a long text. The text keeps the font's own bytes (its gold half,
// its brackets and rules), which the charset, lacking them, draws as plain ASCII (`plain_text`).
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
// ge:keywords notify chat say say_team teamsay print centerprint centre-print finale messages
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

inline constexpr size_t kNoticeLines = 4;
inline constexpr double kNoticeSeconds = 4;  // a notify line's life, s
inline constexpr double kChatSeconds = 6;    // a chat line's
inline constexpr double kCentreSeconds = 2;  // a centre print's, at the least

struct Notice {
  std::string text;  // one line, the font's bytes
  bool chat = false;
  double born = 0;  // s on the demo's timeline
};

// Prints joined into lines: the game prints a line in pieces ("You got the ", "Nailgun", "\n"),
// so a line is shown once its `\n` comes, at the level of its first piece.
struct PrintJoin {
  std::string pending;
  int level = 0;
  // The whole lines `p` finishes (each with nothing to read left out); the rest kept.
  std::vector<DemoPrint> feed(const DemoPrint& p);
};

// The messages on screen, fed each demo frame and read at each picture's time.
struct DemoNotices {
  PrintJoin join;
  std::vector<Notice> lines;  // oldest first
  std::string centre;         // "" none
  double centre_until = 0;
  // This frame's prints and centre print from `s`, at `t` s.
  void take(const DemoState& s, double t);
  // The lines still on screen at `t` (the old ones dropped), as caption lines.
  std::vector<CaptionLine> at(double t);
};

// The font's bytes as plain ASCII: the gold half folded onto the white, the brackets and rules
// as `[`, `]` and `-`, the dots as `.`, every other control byte dropped.
std::string plain_text(std::string_view text);

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

// `lines` in the upper left of a `width` x `height` picture in `glyphs` (empty: the charset), then
// `notices` under them after a gap.
void draw_caption(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                  std::span<const CaptionLine> lines, std::span<const CaptionLine> notices = {});
// A centre print's lines, each centred, the block a third of the way down.
void draw_centre(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                 std::string_view text);

}  // namespace shooters_edge::client
