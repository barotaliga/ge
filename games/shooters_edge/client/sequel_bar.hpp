// THE SEQUEL'S STATUS BAR IN THE WINDOW: the program the sequel's rules send (rules_world.hpp's
// `status_bar`) run over the player's stats (hud/sequel_layout.hpp) and drawn in the game's own
// pictures, read from its packs as the program names them (`pics/<name>.pcx`, the last colour
// see-through) and kept for the session, its text in the console's letters (`conchars`).
//
// The language places in screen pixels and draws pictures at their own size; on a large window
// that is a small bar, so the window runs it on a smaller screen, a whole number of times smaller
// (one step for every 400 px of height, at least one), and draws every pixel that many times over,
// sharp. The crosshair (under the bar) and the centre print (over it) are drawn the same way.
//
// ge:summary The sequel's status bar drawn in the window from its program and the game's pictures
// ge:keywords sequel status-bar layout pictures conchars pcx draw scale sequel-hud crosshair
// ge:keywords centre-print ch1 layout-message scoreboard help-computer
// ge:skill ui shooter
// ge:test tests/shooters_edge_sequel_screen_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "shooters_edge/client/files.hpp"
#include "shooters_edge/hud/sequel_layout.hpp"
#include "shooters_edge/server/rules_world.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"

namespace shooters_edge::client {

struct SequelBar {
  const SearchPath* files = nullptr;  // where the pictures are read (outlives the bar)
  std::unordered_map<std::string, ge::ui::Image> pictures;  // by path; empty: not in the packs
  std::vector<hud::LayoutOp> ops;                           // the last frame's
};

// The whole number the bar is drawn larger by on a window `height` px high.
int sequel_bar_scale(int height);

// Runs the world's bar for client `slot` (its stats, the world's strings) on a `width` x `height`
// window into `bar.ops`, in screen pixels of the smaller screen (`sequel_bar_scale`); then, while
// its stat 13 has bit 1, the layout program the rules last sent it (a scoreboard, the help
// computer) over the bar.
void run_sequel_bar(SequelBar& bar, const server::RulesWorld& world, int slot, int64_t frame,
                    int width, int height);

// Adds what the window draws round the bar to `bar.ops`, in the bar's pixels (`run_sequel_bar`'s
// screen): the crosshair `ch<crosshair>` (1..3; 0 none) centred on the screen, under the bar, and
// the centre print `centre` over it (its lines in the plain letters, each centred, at most 40
// letters; the first at 35% of the screen's height, or at 48 for more than 4 lines, 8 apart).
void add_sequel_screen(SequelBar& bar, int crosshair, std::string_view centre, int width,
                       int height);

// Draws `bar.ops` on `dl`, `scale` px a bar pixel; true when the bar drew anything.
bool draw_sequel_bar(ge::ui::DrawList& dl, SequelBar& bar, int scale);

// The world's string `index` as a sequel client holds it: image names at 544 + n, item names at
// 1056 + n, each player's `name\male/grunt` at 1312 + slot; `scratch` holds a built one.
std::string_view sequel_world_string(const server::RulesWorld& world, int index,
                                     std::string& scratch);

}  // namespace shooters_edge::client
