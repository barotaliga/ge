// THE DEMO WATCHER'S TABLE OF EVERY PLAYER (`mvd_info`) and the player picked by name when a
// multiview demo starts (`demo_playlist_track_name`), both for a multiview recording watched in
// the window (demo_playback.hpp draws the table).
//
// - THE ROWS: one per slot with a name that is not a spectator, in slot order, both teams mixed;
//   with `mvd_info_show_header` a header row above them in the warning colour.
// - THE ROW'S TEXT is `mvd_info_setup` (default `%p%n [%l] %h/%a %w`, the brackets the font's gold
//   ones) with its codes filled in: `%n` the name, `%h` health, `%a` armour, `%f` frags, `%l` the
//   place he is in (`loc`'s names; `tp_name_someplace` with none), `%w` the weapon in hand and its
//   ammunition (`rl:23`, the `tp_name_*` words), `%W` the best weapon owned and its ammunition,
//   `%v` his autotrack rating's kit part (demo_autotrack.hpp), six decimals; `%P` (ping) and `%p`
//   (powerups) print nothing (a recording keeps neither for every player). A width between `%`
//   and the letter pads: right-aligned, or left with a minus (`%-12n`); longer text is not cut. An
//   unknown letter is dropped with its `%`; a `%` with nothing after ends the row. In the header
//   each code is a word (`Nick`, `Health`, `Armor`, `Frags`, `Location`, `Cur.Weap.`, `Best
//   Weap.`, `Value`, `Ping`, `Powerup`) at the same width.
// - THE PLACE: `mvd_info_x`, `mvd_info_y` in the demo clock's cells (demo_playback.hpp): x from the
//   left, or with x negative each row ends |x| cells from the right; y from the top, or with y
//   negative |y| cells above the HUD. The first row is one cell below the point.
// - THE NAME PICKED: `demo_playlist_track_name` (empty: none) at a multiview demo's first frame:
//   each player's name made plain (no colour, the font's red half made white, space, slashes,
//   quotes and semicolons as `_`, outer `_` dropped, lower case) is searched for the wanted text,
//   which must be written in lower case; the earliest match wins, a tie to the lower slot;
//   spectators never. Missed if he is not there yet.
//
// ge:summary The demo watcher's table of every player (mvd_info) and the player picked by name
// ge:keywords mvd_info mvd_info_setup mvd_info_show_header mvd_info_x mvd_info_y player-table
// ge:keywords demo_playlist_track_name track-by-name mvd_info_lines name_track_slot
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_mvd_info_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/place_names.hpp"

namespace shooters_edge::client {

inline const char* const kMvdInfoSetup = "%p%n \x10%l\x11 %h/%a %w";

struct MvdInfoLine {
  std::string text;
  bool header = false;
};

// The table's rows for `d` (a multiview demo: none for another kind or the intermission): the
// header first when `header`. `con` gives the `tp_name_*` words, `places` the place names (null:
// someplace).
std::vector<MvdInfoLine> mvd_info_lines(const Demo& d, const std::string& setup, bool header,
                                        const ge::console::Console& con, const PlaceNames* places);

// A name made plain for `demo_playlist_track_name`'s search.
std::string plain_track_name(std::string_view name);

// The slot `wanted` picks in `s` (-1: none; `wanted` empty: none).
int name_track_slot(const DemoState& s, std::string_view wanted);

}  // namespace shooters_edge::client
