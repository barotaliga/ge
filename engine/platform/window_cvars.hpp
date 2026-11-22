// THE WINDOW'S OWN SETTINGS, for any game with a window: whether it covers the screen and which
// display it covers (`vid_fullscreen`, `vid_display`), the command that lists the displays
// (`vid_displays`), the game's sound (`volume`, 0..1) and when it is heard (`s_auto_focus`: 0
// always, 1 not while the window is minimised, 2 only while it has the focus). A game binds them
// once before its config runs, follows `fullscreen` each frame and reads `display` when it goes
// fullscreen; a setting only one game has (a campaign's scores key) stays in that game, bound
// beside these.
//
// ge:summary The window's settings for any game: vid_fullscreen, vid_display, volume, s_auto_focus
// ge:keywords vid_fullscreen vid_display vid_displays fullscreen display window cvars volume
// ge:keywords window-cvars add_window_cvars s_auto_focus heard_volume minimised unfocused
// ge:keywords alt-enter alt_enter
// ge:skill platform console
// ge:test tests/platform_window_cvars_test.cpp
#pragma once

#include "console/console.hpp"
#include "platform/window.hpp"

namespace ge::platform {

struct WindowCvars {
  bool fullscreen = false;  // the window follows it each frame
  int display = 0;          // read when the window goes fullscreen
  double volume = 0.7;
  int auto_focus = 0;  // s_auto_focus
};

// The cvars over `w` (it must outlive `con`), and `vid_displays`.
void add_window_cvars(console::Console& con, WindowCvars& w);
// The loudness heard now: `volume`, or 0 while `s_auto_focus` silences a window that is minimised
// (1 or 2) or has not the focus (2).
double heard_volume(const WindowCvars& w, bool focused, bool minimised);

// ALT+ENTER: the window in and out of full screen, whatever the binds say (`vid_fullscreen`
// flipped as typed). True when this frame's keys asked for it.
bool alt_enter(const InputSnapshot& in, console::Console& con);

}  // namespace ge::platform
