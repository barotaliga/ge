// QUAKE II'S STATUS BAR IN THE WINDOW: the program Quake II's rules send (rules_world.hpp's
// `status_bar`) run over the player's stats (hud/q2_layout.hpp) and drawn in the game's own
// pictures, read from its packs as the program names them (`pics/<name>.pcx`, the last colour
// see-through) and kept for the session, its text in the console's letters (`conchars`).
//
// The language places in screen pixels and draws pictures at their own size; on a large window
// that is a small bar, so the window runs it on a smaller screen, a whole number of times smaller
// (scr_scale; by the window's size, `q2_bar_scale`), and draws every pixel that many times over,
// sharp. The crosshair (under the bar), the centre print (over it) and the lag graph are drawn the
// same way. scr_draw2d leaves out the bar, the layouts or all of it; scr_alpha fades it.
//
// ENHANCED (`ce_pictures`, `cache` set): a picture is classic-enhance's upscaled copy from the
// packs' cache (`pics/<name>`) where it has one, drawn filtered at the picture's own size from its
// more pixels; the pack's own, sharp, where it has none.
//
// ge:summary Quake II's status bar drawn in the window from its program and the game's pictures
// ge:keywords q2 status-bar layout pictures conchars pcx draw scale q2-hud crosshair
// ge:keywords centre-print ch1 layout-message scoreboard help-computer inventory hotkey
// ge:keywords scr_scale scr_draw2d scr_alpha scr_lag_draw lag-graph net-picture hud-scale
// ge:keywords dumpstatusbar dumplayout dumpclients aashot aadump scoreshot item-completion
// ge:skill ui shooter
// ge:test tests/shooters_edge_q2_screen_test.cpp
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "classic/q2_fx_switches.hpp"
#include "console/console.hpp"
#include "hud/bottom_graph.hpp"
#include "hud/crosshair.hpp"
#include "hud/net_graph.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/q2_view.hpp"
#include "shooters_edge/hud/q2_layout.hpp"
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

struct Q2Bar {
  const SearchPath* files = nullptr;  // where the pictures are read (outlives the bar)
  std::filesystem::path cache{};      // the packs' enhanced cache (ce_pictures), empty: none
  std::unordered_map<std::string, BarPicture> pictures{};  // by path; empty: not in the packs
  std::vector<hud::LayoutOp> ops{};                        // the last frame's
  // The game's binds (key to command), for the inventory's hotkeys; null: none shown.
  const std::map<std::string, std::string>* binds = nullptr;
  double now = 0;  // the real time, s (the inventory's cursor blinks by it)
  std::shared_ptr<server::RulesWorld> recorded{};  // a recording's world (draw_recorded_q2_bar)
  const Q2Screen* screen = nullptr;                // the 2D settings; null: their defaults
  // A joined server's packets (the lag graph) and its moves not yet answered (128 and over: the
  // pack's `net` picture blinks in the graph's place); null and 0 when nothing is joined.
  const ge::hud::NetHistory* lag = nullptr;
  int unanswered = 0;
  // The graph along the bottom (timegraph): each frame's time from `now`, the last frame's time.
  ScreenGraph graph{};
  int width = 0, height = 0;  // the window the last `run_q2_bar` drew for, px
  // The last frame had a screen of the rules up (the help computer, a scoreboard or the
  // inventory): ESCAPE puts it away rather than opening the menu (menu/main_menu.hpp's Host).
  bool layout_up = false;
  // F1 held this frame: 1 in a game, 2 in a recording played back. In a recording it shows the
  // last layout the rules sent whether or not the recorded flag has it up; with scr_draw2d 3 it
  // shows the layouts that setting hides.
  int f1 = 0;
  // The console commands waiting for the next frame's world (`add_q2_bar_commands`), each one's
  // words in the order typed (a bind of `aadump; dumpclients` answers both), and the console they
  // print to (null: none asked).
  std::vector<std::vector<std::string>> asked{};
  ge::console::Console* asked_by = nullptr;
  // The hit marker (classic/q2_fx_switches.hpp's `cl_hit_markers`): the switches (null: none
  // drawn), the recording's hits last seen and the real time of the last new one, s.
  const ge::classic::Q2FxSwitches* fx = nullptr;
  // The picture's brightness (`gamma`, below 1 brighter), laid on the pictures under
  // gl_gamma_scale_pics (q2_view.hpp's Q2Screen::gamma_pics); null: 1. `pics_gamma` the one the
  // loaded pictures carry: a change reads them again.
  const double* gamma = nullptr;
  double pics_gamma = 1;
  // A copy of the crosshair's settings the last draw_q2_crosshair was given (ch_scale, ch_x, ch_y:
  // a scope's picture follows them); none: their defaults. A copy, since the caller's settings may
  // not outlive the call.
  std::optional<ge::hud::CrosshairSettings> cross = std::nullopt;
  // The console a missing scr_font is told on and set back in (null: neither).
  ge::console::Console* console = nullptr;
  int64_t hits_seen = 0;
  double hit_at = -1e9;
  // The item names the last frame's world listed (Tab completes them after `use`, `drop` and
  // `give`: add_q2_bar_commands).
  std::vector<std::string> item_names{};
};

// The console's looks at the bar's program, answered by the next frame that runs a Quake II bar
// (`run_q2_bar`); "Must be in a level..." when the last frame ran none:
// `dumpstatusbar <file>` and `dumplayout <file>` (the status bar's program, or the layout up, to
// `layouts/<file>.txt`), `dumpclients` (each player's slot and name), `aashot [name]` (the bar
// and the layout up as an 80 x 40 text picture, hud/q2_layout.hpp's `layout_text`, to
// `scoreshots/<name>.txt` or the first free `scoreshots/quakeNNN.txt`) and `aadump` (the same
// picture in the console). Tab after `use`, `drop` or `give` completes the item names the last
// frame's world listed, sorted (Console::arg_values).
void add_q2_bar_commands(ge::console::Console& con, Q2Bar& bar);

// The world a Quake II recording or a joined Quake II server gives its bar (the bar's program, the
// pictures and item names, the player's stats, screen and inventory, the players' names), filled
// into `out` for `run_q2_bar` at slot `ds.player`; false when `ds` is not Quake II's or
// carries no bar program (the window's own HUD is drawn instead).
bool q2_world_of(const DemoState& ds, server::RulesWorld& out);
// A recording's (or a joined server's) Quake II bar run and drawn on `dl` over a `width` x
// `height` window, and its hit marker (`DemoState::hits`) at the centre in red, fading; false when
// `ds` has none to draw (`q2_world_of`). `centre`: the centre print shown now ("" none), written
// over it in the bar's letters (`add_q2_screen`).
bool draw_recorded_q2_bar(ge::ui::DrawList& dl, Q2Bar& bar, const DemoState& ds, int width,
                          int height, std::string_view centre = {});

// The whole number every 2D element is drawn larger by on a `width` x `height` window: `setting`
// (scr_scale) 1..10 as given, past 10 held to it; 0 by the window, as Quake II picks it: by the
// height on a wide window (4 from 2160 px, 2 from 1080, else 1), by the width on a tall one (3840
// and 1920).
int q2_bar_scale(int width, int height, int setting = 0);
// The bar's own: by its `screen`'s scr_scale.
int q2_bar_scale(const Q2Bar& bar, int width, int height);

// Hands `bar` a joined server's packets (`lag`; null: no graph), the moves it has not answered
// and the real time `now`, s, for the lag graph and the blinking net picture.
void q2_bar_net(Q2Bar& bar, const ge::hud::NetHistory* lag, int unanswered, double now);

// Runs the world's bar for client `slot` (its stats, the world's strings) on a `width` x `height`
// window into `bar.ops`, in screen pixels of the smaller screen (`q2_bar_scale`); then, while
// its stat 13 has bit 1, the layout program the rules last sent it (a scoreboard, the help
// computer) over the bar, and while bit 2 is set its inventory: a 256 x 240 box centred, the
// `inventory` picture, the header, and a row for each item it holds (the key bound to `use <its
// name>`, the count, the name), the selected one (stat 12) in the plain letters with a blinking
// cursor and the rest in the alternate set, at most 17 rows scrolled to keep the selection near
// the middle.
void run_q2_bar(Q2Bar& bar, const server::RulesWorld& world, int slot, int64_t frame, int width,
                int height);

// Adds what the window draws round the bar to `bar.ops`, in the bar's pixels (`run_q2_bar`'s
// screen): the crosshair `ch<crosshair>` (1..3; 0 none) centred on the screen, under the bar, and
// the centre print `centre` over it (its lines in the plain letters, each centred, at most 40
// letters; the first at 35% of the screen's height, or at 48 for more than 4 lines, 8 apart).
void add_q2_screen(Q2Bar& bar, int crosshair, std::string_view centre, int width, int height);

// The letters' picture the bar writes in (scr_font's, else conchars; null when the packs have
// neither): what the chat lines on screen are written in too.
const ge::ui::Image* q2_font_image(Q2Bar& bar);

// Draws the crosshair as the player sets it (hud/crosshair.hpp: `crosshair`, `crosshaircolor`,
// `crosshairsize`, `crosshairalpha`) centred on a `width` x `height` screen, at the bar's scale:
// mark 1 the pack's own `ch1` (with Quake II loaded, `s.pictures`: mark N its `chN`, the game's
// own meaning), palette numbers from that picture's own colours. Draw it before the bar, so it lies
// under it. Nothing when the settings hide it or the pack has no such picture.
// It stands off the middle by cl_crossx and cl_crossy (its ch_x and ch_y) in bar pixels, and
// takes the colour of `health` when ch_health asks.
// The hit marker over `dl`: `hits` (DemoState::hits) more than last seen starts its fade at
// `bar.now`; drawn while `bar.fx`'s cl_hit_markers and scr_hit_marker_time keep it.
// scr_showturtle (the screen's `turtle`): a frame's troubles (`DemoState::q2_trouble`) as words
// in the alternate colour, one a line from eleven lines over the bottom at the left: BADFRAME,
// OLDFRAME, NODELTA, and at 2 SUPPRESSED. Into `bar.ops`, after `run_q2_bar`.
void add_q2_turtle(Q2Bar& bar, uint8_t trouble, int width, int height);

void draw_q2_hit_marker(ge::ui::DrawList& dl, Q2Bar& bar, int64_t hits, int width, int height);
void draw_q2_crosshair(ge::ui::DrawList& dl, Q2Bar& bar, const ge::hud::CrosshairSettings& s,
                       int width, int height, int health = 100);

// Draws `bar.ops` on `dl`, `scale` px a bar pixel, at the screen's scr_alpha, and then the lag
// graph (scr_lag_draw) or the `net` picture over it; true when the bar drew anything.
bool draw_q2_bar(ge::ui::DrawList& dl, Q2Bar& bar, int scale);

// The world's string `index` as a Quake II client holds it: image names at 544 + n, item names at
// 1056 + n, each player's `name\male/grunt` at 1312 + slot; `scratch` holds a built one.
std::string_view q2_world_string(const server::RulesWorld& world, int index, std::string& scratch);

}  // namespace shooters_edge::client
