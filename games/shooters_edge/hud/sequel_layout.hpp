// THE SEQUEL'S LAYOUT LANGUAGE: the little program a sequel server sends for its status bar (and,
// on top of it, for its scoreboard and help screen), run into a list of pictures and lines of text
// at screen positions. A pure function of the program, the player's 32 stats, the server's strings
// and the screen's size: no pictures are read here (the window looks the names up in the pack,
// client/sequel_bar.hpp), so a test or a lab runs it with no game.
//
// The language, word by word (porting notes: sequel-hud.md):
//
// - **The cursor**: `xl n` n from the left, `xr n` the width plus n, `xv n` half the width less
//   160 plus n (a 320-wide field centred); `yt n`, `yb n` and `yv n` (240 high) the same downwards.
//   It starts at (0, 0) on every run.
// - **Numbers** in the digit pictures (`num_0`.. `num_9`, `num_minus`; the coloured set `anum_*`),
//   16 px a digit, right-aligned in a field of 1..5 digits with a 2 px inset, a number too wide cut
//   to its first digits: `num w s` stat s; `hnum` the health (stat 1, 3 digits; at 25 or less it
//   flashes between the sets every 4 server frames, at 0 or less coloured); `anum` the ammo (stat
//   3; flashes at 5 or less, nothing when negative); `rnum` the armour (stat 5; nothing below 1).
//   Stat 15's bits 1, 4 and 2 put `field_3` behind each of them (the blow just taken).
// - **Pictures**: `pic s` the picture whose index stat s holds (the server's image strings; 0 or
//   out of range draws nothing); `picn name` the named one.
// - **Text** in the console's letters, 8 px each: `string "t"` and `string2` (coloured) one line at
//   the cursor; `cstring` and `cstring2` each line centred in 320 px from the cursor's x, 8 px
//   apart; `stat_string s` the server string stat s names.
// - **Conditions**: `if s` .. `endif`: when stat s is 0 every word up to the first `endif` is
//   skipped (no nesting).
// - **Score blocks**: `client x y c score ping time` (the icon, the name, score, ping and minutes,
//   160 x 32), `ctf x y c score ping` (one row of 3, 3 and 12 columns).
//
// An unknown word is skipped. Where a client would be dropped or read past its stats (a stat number
// out of 0..31, a picture index of 256 or more, a string index outside the server's 2080), nothing
// is drawn: a player sees the rest of the bar.
//
// ge:summary The sequel's status bar language run into pictures and text at screen positions
// ge:keywords sequel layout status-bar statusbar layout-language hnum anum rnum stat_string cstring
// ge:keywords scoreboard client-block stats field_3 num_minus conchars picn sequel-hud
// ge:skill ui shooter
// ge:test tests/shooters_edge_sequel_layout_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::hud {

inline constexpr int kLayoutStats = 32;
inline constexpr int kLayoutStrings = 2080;  // the server's strings a program may name
inline constexpr int kLayoutImages = 544;    // the first image string (image 0 is none)
inline constexpr int kLayoutSkins = 1312;    // the first player's `name\model/skin`

// One thing the program draws, its top left in screen pixels.
struct LayoutOp {
  enum class Kind : uint8_t { picture, text };
  Kind kind = Kind::picture;
  int x = 0, y = 0;
  // A picture by the program's name for it (`num_3`, `i_health`, `/players/male/grunt_i.pcx`; see
  // `layout_picture_path`), and the one shown when it is missing (a player's icon), else empty.
  std::string name, fallback;
  std::string text;  // a line of text, each byte a letter of the console's sheet
  bool alt = false;  // the text's coloured set (each letter + 128)
};

struct LayoutInput {
  std::string_view program;
  std::span<const int16_t> stats;  // the player's 32
  int width = 320, height = 240;   // the screen, px
  int64_t frame = 0;               // the server frame last received (the numbers' flashing)
  int self = -1;                   // the viewer's client number (a `ctf` row's colour)
  // The server's string `index` (0..2079), empty when none.
  std::string_view (*string)(const void* ctx, int index) = nullptr;
  const void* ctx = nullptr;
};

// Runs `in.program` into `out` (cleared first), in drawing order.
void run_layout(const LayoutInput& in, std::vector<LayoutOp>& out);

// The file a picture name means: `pics/<name>.pcx`, or the path after a leading slash.
std::string layout_picture_path(std::string_view name);

}  // namespace shooters_edge::hud
