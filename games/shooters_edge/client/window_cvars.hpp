// THE WINDOW'S OWN SETTINGS: whether it covers the screen and which display it covers
// (`vid_fullscreen`, `vid_display`) and the command that lists the displays (`vid_displays`), the
// game's sound (`volume`, 0..1) and whether the scores key in a campaign also shows the level's
// figures (`scr_levelstats`). Built into the `shooters_edge` window alone: it asks the platform
// for the displays.
//
// ge:summary The window's console settings: vid_fullscreen, vid_display, volume, scr_levelstats
// ge:keywords vid_fullscreen vid_display vid_displays fullscreen display window cvars volume
// ge:keywords scr_levelstats window-cvars
// ge:skill platform shooter
#pragma once

#include "console/console.hpp"

namespace shooters_edge::client {

struct WindowCvars {
  bool fullscreen = false;  // the window follows it each frame
  int display = 0;          // read when the window goes fullscreen
  double volume = 0.7;
  bool level_stats = true;
};
// The cvars over `w` (it must outlive `con`), and `vid_displays`.
void add_window_cvars(ge::console::Console& con, WindowCvars& w);

}  // namespace shooters_edge::client
