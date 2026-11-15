// THE WINDOW'S OWN SETTINGS, for any game with a window: whether it covers the screen and which
// display it covers (`vid_fullscreen`, `vid_display`), the command that lists the displays
// (`vid_displays`) and the game's sound (`volume`, 0..1). A game binds them once before its config
// runs, follows `fullscreen` each frame and reads `display` when it goes fullscreen; a setting
// only one game has (a campaign's scores key) stays in that game, bound beside these.
//
// ge:summary The window's console settings for any game: vid_fullscreen, vid_display, volume
// ge:keywords vid_fullscreen vid_display vid_displays fullscreen display window cvars volume
// ge:keywords window-cvars add_window_cvars
// ge:skill platform console
// ge:test tests/platform_window_cvars_test.cpp
#pragma once

#include "console/console.hpp"

namespace ge::platform {

struct WindowCvars {
  bool fullscreen = false;  // the window follows it each frame
  int display = 0;          // read when the window goes fullscreen
  double volume = 0.7;
};

// The cvars over `w` (it must outlive `con`), and `vid_displays`.
void add_window_cvars(console::Console& con, WindowCvars& w);

}  // namespace ge::platform
