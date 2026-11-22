// The window's settings for any game (engine/platform/window_cvars.hpp): the cvars write the
// settings, volume is held to 0..1. No window is opened.
//
// ge:summary Tests the window's settings: vid_fullscreen, vid_display, volume, vid_displays
// ge:keywords test window-cvars vid_fullscreen vid_display volume vid_displays vid_geometry
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
  con.execute("s_enable 0");  // no sound at all, in focus or not
  CHECK(ge::platform::heard_volume(w, true, false) == 0);
  con.execute("s_enable 2");  // another game's other mixer: ours plays
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

TEST(vid_geometry_reads_a_size_a_place_or_both) {
  ge::platform::WindowPlace p;
  CHECK(ge::platform::window_place_of("640x480", p) && p.width == 640 && p.height == 480 &&
        !p.has_place);
  CHECK(ge::platform::window_place_of("800x600+10-20", p) && p.width == 800 && p.height == 600 &&
        p.has_place && p.x == 10 && p.y == -20);
  CHECK(ge::platform::window_place_of("+5+7", p) && p.width == 0 && p.has_place && p.x == 5 &&
        p.y == 7);
  for (const char* bad :
       {"", "640", "640x", "x480", "0x480", "640x480+5", "640x480+a+b", "640x480 ", "+5"})
    CHECK_MSG(!ge::platform::window_place_of(bad, p), bad);
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  con.execute("vid_geometry 1024x768+0+0");
  CHECK(w.geometry == "1024x768+0+0");
}
