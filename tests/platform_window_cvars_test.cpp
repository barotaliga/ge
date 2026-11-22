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

TEST(s_auto_focus_silences_a_window_minimised_or_out_of_focus) {
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  CHECK(ge::platform::heard_volume(w, false, true) == 0.7);  // 0: always heard
  con.execute("s_auto_focus 1");
  CHECK(ge::platform::heard_volume(w, false, false) == 0.7);
  CHECK(ge::platform::heard_volume(w, true, true) == 0);
  con.execute("s_auto_focus 2");
  CHECK(ge::platform::heard_volume(w, false, false) == 0);
  CHECK(ge::platform::heard_volume(w, true, false) == 0.7);
}

TEST(the_window_style_follows_win_noborder_notitle_alwaysontop_and_noresize) {
  // Either win_noborder or win_notitle drops the border and title bar; the defaults are a
  // window's usual: a border, among the others, resized by its edges.
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  ge::platform::WindowStyle s = ge::platform::window_style(w);
  CHECK(s.border && !s.on_top && s.resizable);
  con.execute("win_notitle 1; win_alwaysontop 1; win_noresize 1");
  s = ge::platform::window_style(w);
  CHECK(!s.border && s.on_top && !s.resizable);
  con.execute("win_notitle 0; win_noborder 1");
  CHECK(!ge::platform::window_style(w).border);
  con.execute("win_noborder 0");
  CHECK(ge::platform::window_style(w).border);
}
