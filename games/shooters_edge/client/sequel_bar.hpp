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
// ENHANCED (`ce_pictures`, `cache` set): a picture is classic-enhance's upscaled copy from the
// packs' cache (`pics/<name>`) where it has one, drawn filtered at the picture's own size from its
// more pixels; the pack's own, sharp, where it has none.
//
// ge:summary The sequel's status bar drawn in the window from its program and the game's pictures
// ge:keywords sequel status-bar layout pictures conchars pcx draw scale sequel-hud crosshair
// ge:keywords centre-print ch1 layout-message scoreboard help-computer inventory hotkey
// ge:skill ui shooter
// ge:test tests/shooters_edge_sequel_screen_test.cpp
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "hud/crosshair.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/hud/sequel_layout.hpp"
#include "shooters_edge/server/rules_world.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"

namespace shooters_edge::client {

// A picture of the bar: `image` at `k` pixels a pixel of the pack's picture (1: the pack's own).
struct BarPicture {
  ge::ui::Image image;
  int k = 1;
  std::vector<uint8_t> palette;  // the picture's own 256 colours (768 bytes)
  float width() const { return static_cast<float>(image.width) / static_cast<float>(k); }
  float height() const { return static_cast<float>(image.height) / static_cast<float>(k); }
};

struct SequelBar {
  const SearchPath* files = nullptr;  // where the pictures are read (outlives the bar)
  std::filesystem::path cache{};      // the packs' enhanced cache (ce_pictures), empty: none
  std::unordered_map<std::string, BarPicture> pictures{};  // by path; empty: not in the packs
  std::vector<hud::LayoutOp> ops{};                        // the last frame's
  // The game's binds (key to command), for the inventory's hotkeys; null: none shown.
  const std::map<std::string, std::string>* binds = nullptr;
  double now = 0;  // the real time, s (the inventory's cursor blinks by it)
  std::shared_ptr<server::RulesWorld> recorded{};  // a recording's world (draw_recorded_sequel_bar)
};

// The world a sequel recording or a joined sequel server gives its bar (the bar's program, the
// pictures and item names, the player's stats, screen and inventory, the players' names), filled
// into `out` for `run_sequel_bar` at slot `ds.player`; false when `ds` is not the sequel's or
// carries no bar program (the window's own HUD is drawn instead).
bool sequel_world_of(const DemoState& ds, server::RulesWorld& out);
// A recording's (or a joined server's) sequel bar run and drawn on `dl` over a `width` x
// `height` window; false when `ds` has none to draw (`sequel_world_of`).
bool draw_recorded_sequel_bar(ge::ui::DrawList& dl, SequelBar& bar, const DemoState& ds, int width,
                              int height);

// The whole number the bar is drawn larger by on a window `height` px high.
int sequel_bar_scale(int height);

// Runs the world's bar for client `slot` (its stats, the world's strings) on a `width` x `height`
// window into `bar.ops`, in screen pixels of the smaller screen (`sequel_bar_scale`); then, while
// its stat 13 has bit 1, the layout program the rules last sent it (a scoreboard, the help
// computer) over the bar, and while bit 2 is set its inventory: a 256 x 240 box centred, the
// `inventory` picture, the header, and a row for each item it holds (the key bound to `use <its
// name>`, the count, the name), the selected one (stat 12) in the plain letters with a blinking
// cursor and the rest in the alternate set, at most 17 rows scrolled to keep the selection near
// the middle.
void run_sequel_bar(SequelBar& bar, const server::RulesWorld& world, int slot, int64_t frame,
                    int width, int height);

// Adds what the window draws round the bar to `bar.ops`, in the bar's pixels (`run_sequel_bar`'s
// screen): the crosshair `ch<crosshair>` (1..3; 0 none) centred on the screen, under the bar, and
// the centre print `centre` over it (its lines in the plain letters, each centred, at most 40
// letters; the first at 35% of the screen's height, or at 48 for more than 4 lines, 8 apart).
void add_sequel_screen(SequelBar& bar, int crosshair, std::string_view centre, int width,
                       int height);

// Draws the crosshair as the player sets it (hud/crosshair.hpp: `crosshair`, `crosshaircolor`,
// `crosshairsize`, `crosshairalpha`) centred on a `width` x `height` screen, at the bar's scale:
// mark 1 the pack's own `ch1` (with the sequel loaded, `s.pictures`: mark N its `chN`, the game's
// own meaning), palette numbers from that picture's own colours. Draw it before the bar, so it lies
// under it. Nothing when the settings hide it or the pack has no such picture.
void draw_sequel_crosshair(ge::ui::DrawList& dl, SequelBar& bar,
                           const ge::hud::CrosshairSettings& s, int width, int height);

// Draws `bar.ops` on `dl`, `scale` px a bar pixel; true when the bar drew anything.
bool draw_sequel_bar(ge::ui::DrawList& dl, SequelBar& bar, int scale);

// The world's string `index` as a sequel client holds it: image names at 544 + n, item names at
// 1056 + n, each player's `name\male/grunt` at 1312 + slot; `scratch` holds a built one.
std::string_view sequel_world_string(const server::RulesWorld& world, int index,
                                     std::string& scratch);

}  // namespace shooters_edge::client
