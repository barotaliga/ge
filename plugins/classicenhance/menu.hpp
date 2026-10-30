// THE CLASSIC-ENHANCE MENU: where a player points the game at their own classic packs, starts the
// enhancing, watches every stage of it until it is done, and switches each enhancement on or off.
// It is a model on the engine's menus (ui/menu.hpp) and a panel drawn beside them on the 2D draw
// list, so a lab pictures it headless and a game's window mounts it as it is.
//
// - **The pages**: `classic` (the pack to use, ENHANCE, the switches, back), `enhancing` (the
//   progress panel beside CANCEL, then PLAY once the cache is ready), `switches` (a page a group)
//   and `switches.<group>` (one toggle a switch, switches.hpp's catalog, plus every switch of the
//   group on or off together and the defaults back).
// - **Events** (`apply_menu_event`): a toggle sets its `ce_` cvar, so the console, a config and
//   the menu are one setting; the pack chosen, ENHANCE, CANCEL and PLAY come back as an action for
//   the game to take (start an `EnhanceJob`, stop it, load a map). `sync_switches` sets the
//   toggles from the values after the console changed one.
// - **The panel** (`draw_progress`): the run's whole fraction and its time so far and left; every
//   stage a row with its state, its bar, its count and the item it is on; the last textures done,
//   the pack's picture beside the enhanced one; and the result: READY with the next step, or what
//   failed and how to go on.
//
// ge:summary The classic-enhance menu: the pack, the enhancing watched stage by stage, the switches
// ge:keywords classicenhance menu ui page enhance progress panel stage bar thumbnail switches
// ge:keywords toggle cvar cancel play pack source draw_progress
// ge:skill classicenhance ui
// ge:test tests/classicenhance_menu_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "classicenhance/progress.hpp"
#include "classicenhance/switches.hpp"
#include "console/cvar.hpp"
#include "ui/draw.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"

namespace ge::classicenhance {

// The menu over the packs found (`packs`: what the player reads for each, e.g. its folder) and
// the switches' values.
ui::Menu classic_menu(std::span<const std::string> packs, const Switches& values);

enum class MenuAction : uint8_t { none, enhance, cancel, play, back };

// One menu event applied: a switch toggled is set through its cvar in `cvars` (which writes
// `values`); a group's ALL ON, ALL OFF and DEFAULTS set each of its switches the same way and
// the toggles follow. Returns what the game must do; `*pack` is the pack chosen.
MenuAction apply_menu_event(const ui::MenuEvent& e, ui::Menu& menu, console::Cvars& cvars,
                            Switches* values, int* pack);

// Every toggle set from `values` (after a change from the console or a config).
void sync_switches(ui::Menu& menu, const Switches& values);

// The enhancing page's PLAY enabled once the board is finished, CANCEL while it runs.
void sync_enhancing(ui::Menu& menu, const BoardView& board);

struct PanelStyle {
  ui::Color panel = ui::hex(0x0b1622, 0.78f);
  ui::Color border = ui::hex(0xffffff, 0.10f);
  ui::Color text = ui::hex(0xffffff);
  ui::Color dim = ui::hex(0xffffff, 0.62f);
  ui::Color track = ui::hex(0xffffff, 0.14f);
  ui::Color running = ui::hex(0xf0a63f);
  ui::Color done = ui::hex(0x5fc27a);
  ui::Color skipped = ui::hex(0x7f95ad);
  ui::Color failed = ui::hex(0xe5483c);
  float scale = 1;  // the menu layout's scale (menu_view.hpp), so the panel reads at its size
};

// The board drawn into `area`. The thumbnails' pictures are kept in `store` (cleared and
// refilled), which must live until the list is drawn.
void draw_progress(ui::DrawList& list, const BoardView& board, ui::Rect area,
                   const PanelStyle& style, std::vector<ui::Image>& store,
                   const std::string& pack = {});

// Where the panel goes beside a laid-out menu: right of the column and above the help and footer
// in landscape, above the column in portrait.
ui::Rect panel_area(const ui::MenuLayout& layout);

// "1:05", "12 s": seconds as the panel shows them.
std::string clock_text(double seconds);

}  // namespace ge::classicenhance
