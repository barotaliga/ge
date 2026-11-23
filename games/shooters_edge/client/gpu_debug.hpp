// THE WINDOW'S PART OF `gpudebug` (render/gpu_debug.hpp: the frame without the console, its notes
// below it, copied to the clipboard for an agent): the game's own notes and the console's switch.
//
// - THE NOTES (`gpu_debug_notes`), after the engine's (the time, the build, the frame) and before
//   its "# gpu": # where: the game and the map, the player's place, view angles and speed in map
//   units (or the view watched in a recording), the eye in metres, and the line that draws the same
//   picture again (`r_viewfrom`, view_from.hpp); # look: r_look and the window's size.
// - `draw_console_shown`: the console drawn unless gpudebug is taking the frame.
//
// ge:summary Shooter's Edge's gpudebug notes: where the player stands and looks, the look
// ge:keywords gpudebug notes where viewfrom draw_console_shown gpu_debug_notes agent report
// ge:skill shooter render
// ge:test tests/shooters_edge_gpu_debug_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/view.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/place_seen.hpp"
#include "shooters_edge/client/view_from.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

// What the notes read. Each pointer must outlive the console.
struct GpuDebugSources {
  const ViewFrom* view_from = nullptr;
  const Listen* listen = nullptr;
  const PlaceSeen* seen = nullptr;
  const std::string* look = nullptr;  // r_look
  const int *width = nullptr, *height = nullptr;
};

// The game's notes (# where, # look) as `gpudebug` adds them.
std::vector<std::string> gpu_debug_notes(const GpuDebugSources& s);

// The console drawn as the window draws it, unless gpudebug is taking this frame (`hidden`:
// render/gpu_debug.hpp's `GpuDebug::shot.hiding()`).
inline void draw_console_shown(bool hidden, ge::ui::DrawList& list,
                               const ge::console::ConsoleView& view,
                               const menu::ClassicConsole& look, double t) {
  if (!hidden) menu::draw_game_console(list, view, look, t);
}

}  // namespace shooters_edge::client
