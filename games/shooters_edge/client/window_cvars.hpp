// THE WINDOW'S SETTINGS IN THIS GAME: the engine's (`platform/window_cvars.hpp`: vid_fullscreen,
// vid_display, vid_displays, volume) and the game's own beside them: whether the scores key in a
// campaign also shows the level's figures (`scr_levelstats`).
//
// ge:summary The window's console settings: the engine's and scr_levelstats
// ge:keywords window-cvars scr_levelstats level-stats
// ge:skill platform shooter
#pragma once

#include "console/console.hpp"
#include "platform/window_cvars.hpp"

namespace shooters_edge::client {

struct WindowCvars : ge::platform::WindowCvars {
  bool level_stats = true;
  bool show_pause = true;     // showpause
  double host_framerate = 0;  // host_framerate: game seconds a frame (0: the real time)
  double print_speed = 8;     // scr_printspeed: a finale's characters a second
};

// The engine's window cvars and scr_levelstats over `w` (it must outlive `con`).
void add_window_cvars(ge::console::Console& con, WindowCvars& w);

}  // namespace shooters_edge::client
