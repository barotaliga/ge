// The window's settings for any game (engine/platform/window_cvars.hpp): the cvars write the
// settings, volume is held to 0..1. No window is opened.
//
// ge:summary Tests the window's settings: vid_fullscreen, vid_display, volume, vid_displays
// ge:keywords test window-cvars vid_fullscreen vid_display volume vid_displays
// ge:skill platform testing
#include "platform/window_cvars.hpp"

#include "check.hpp"
#include "console/console.hpp"

TEST(the_window_cvars_write_the_settings) {
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  CHECK(!w.fullscreen && w.display == 0 && w.volume == 0.7);
  con.execute("vid_fullscreen 1; vid_display 2; volume 3");
  CHECK(w.fullscreen);
  CHECK(w.display == 2);
  CHECK(w.volume == 1);  // clamped
  CHECK(con.has_command("vid_displays"));
}

TEST(alt_enter_flips_full_screen) {
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  ge::platform::InputSnapshot in;
  in.pressed[static_cast<size_t>(ge::platform::Key::enter)] = true;
  CHECK(!ge::platform::alt_enter(in, con));  // ENTER alone
  in.held[226] = true;
  CHECK(ge::platform::alt_enter(in, con) && w.fullscreen);
  CHECK(ge::platform::alt_enter(in, con) && !w.fullscreen);
}
