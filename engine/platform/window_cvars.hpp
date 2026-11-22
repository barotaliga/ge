// THE WINDOW'S OWN SETTINGS, for any game with a window: whether it covers the screen and which
// display it covers (`vid_fullscreen`, `vid_display`), the command that lists the displays
// (`vid_displays`), the game's sound (`volume`, 0..1) and when it is heard (`s_auto_focus`: 0
// always, 1 not while the window is minimised, 2 only while it has the focus), the window's border
// and title bar, its place above the others and its edges (`win_noborder`, `win_notitle`,
// `win_alwaysontop`, `win_noresize`) and its size and place (`vid_geometry`, `WxH+X+Y`;
// `follow_window_cvars` applies them all), and the sound device (`s_device`, `s_devices` lists
// them; `follow_sound_device`). A game binds them
// once before its config runs, follows `fullscreen` each frame and reads `display` when it goes
// fullscreen; a setting only one game has (a campaign's scores key) stays in that game, bound
// beside these.
//
// ge:summary The window's settings for any game: vid_fullscreen, vid_display, volume, s_auto_focus
// ge:keywords vid_fullscreen vid_display vid_displays fullscreen display window cvars volume
// ge:keywords window-cvars add_window_cvars s_auto_focus heard_volume minimised unfocused
// ge:keywords alt-enter alt_enter win_noborder win_notitle win_alwaysontop win_noresize
// ge:keywords follow_window_cvars window_style borderless always-on-top vid_geometry
// ge:keywords window_place_of window-size window-position geometry s_device s_devices
// ge:keywords sound-device follow_sound_device al_device
// ge:skill platform console
// ge:test tests/platform_window_cvars_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "console/console.hpp"
#include "platform/audio.hpp"
#include "platform/window.hpp"

namespace ge::platform {

struct WindowCvars {
  bool fullscreen = false;  // the window follows it each frame
  int display = 0;          // read when the window goes fullscreen
  double volume = 0.7;
  int auto_focus = 0;  // s_auto_focus
  // win_noborder, win_notitle (either drops the border and title bar), win_alwaysontop,
  // win_noresize: the window's style (`window_style`), followed each frame.
  bool no_border = false, no_title = false, on_top = false, no_resize = false;
  // vid_geometry: the window's size and place, `WxH+X+Y` ("" as the game opened it).
  std::string geometry;
  // s_device: the sound device by name ("" the system's default; "?" lists them), and the one
  // `follow_sound_device` last asked for.
  std::string sound_device, sound_applied;
  // What `follow_window_cvars` last set the window to.
  bool full_applied = false;
  WindowStyle style_applied;
  std::string geometry_applied;
};

// `WxH`, `WxH+X+Y` or `+X+Y` (X and Y signed, from the desktop's top left) as a place; false for
// anything else, the empty text too.
bool window_place_of(std::string_view text, WindowPlace& out);

// The cvars over `w` (it must outlive `con`), and `vid_displays`.
void add_window_cvars(console::Console& con, WindowCvars& w);
// The loudness heard now: `volume`, or 0 while `s_auto_focus` silences a window that is minimised
// (1 or 2) or has not the focus (2).
double heard_volume(const WindowCvars& w, bool focused, bool minimised);

// The style the settings ask of the window.
WindowStyle window_style(const WindowCvars& w);
// Once a frame: the window follows the settings that changed since it last did (full screen on
// `display`, its style while in a window); a full screen the platform refuses sets
// `vid_fullscreen` back.
// With `sound` and `con`, `follow_sound_device` too.
void follow_window_cvars(Window& window, WindowCvars& w, AudioOut* sound = nullptr,
                         console::Console* con = nullptr);
// Once a frame: the sound moved to `s_device` when it changed (a device not there said on `con`,
// the sound left where it was); `?` lists the devices on `con` and puts the setting back.
void follow_sound_device(AudioOut* out, WindowCvars& w, console::Console& con);

// ALT+ENTER: the window in and out of full screen, whatever the binds say (`vid_fullscreen`
// flipped as typed). True when this frame's keys asked for it.
bool alt_enter(const InputSnapshot& in, console::Console& con);

}  // namespace ge::platform
