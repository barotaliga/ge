// Fall Line's menus and shell (games/fall_line/app/menus.hpp): the app opens on the front door,
// PLAY starts the run, the back press pauses it and RESUME or back returns, QUIT TO TITLE goes
// back to the front door; the options page shows and writes the settings, both menus' options
// agree; every move makes its sound cue; and every page reads at the three reference sizes.
//
// ge:summary Tests for Fall Line's front door, pause page, options and the shell's modes
// ge:keywords test front-door pause-page options shell step_shell menu-settings layout-problems
// ge:skill ui testing
#include <string>

#include "check.hpp"
#include "fall_line/app/menus.hpp"

namespace app = fall_line::app;
using ge::ui::MenuInput;

namespace {

MenuInput down() { return {.down = true}; }
MenuInput right() { return {.right = true}; }
MenuInput accept() { return {.accept = true}; }
MenuInput back() { return {.back = true}; }

}  // namespace

TEST(the_front_door_opens_on_play_and_play_starts_the_run) {
  app::Shell sh = app::make_shell();
  CHECK(sh.mode == app::Mode::front);
  app::ShellStep s = app::step_shell(sh, {});
  CHECK(app::menu_up(sh)->focused() && app::menu_up(sh)->focused()->id == "play");
  CHECK(s.cue == app::MenuCue::none);
  s = app::step_shell(sh, accept());
  CHECK(s.play && sh.mode == app::Mode::run && s.cue == app::MenuCue::press);
  CHECK(app::menu_up(sh) == nullptr);
}

TEST(back_on_the_run_pauses_and_resume_or_back_returns) {
  app::Shell sh = app::make_shell();
  app::step_shell(sh, accept());
  app::ShellStep s = app::step_shell(sh, back());
  CHECK(s.pause && sh.mode == app::Mode::pause && s.cue == app::MenuCue::page);
  CHECK(app::menu_up(sh)->focused()->id == "resume");
  s = app::step_shell(sh, accept());
  CHECK(s.resume && sh.mode == app::Mode::run);
  app::step_shell(sh, back());
  s = app::step_shell(sh, back());
  CHECK(s.resume && sh.mode == app::Mode::run);
}

TEST(quit_to_title_goes_back_to_the_front_door_and_quit_leaves) {
  app::Shell sh = app::make_shell();
  app::step_shell(sh, accept());
  app::step_shell(sh, back());
  app::step_shell(sh, down());
  app::ShellStep s = app::step_shell(sh, down());
  CHECK(app::menu_up(sh)->focused()->id == "to-title" && s.cue == app::MenuCue::focus);
  s = app::step_shell(sh, accept());
  CHECK(s.to_front && sh.mode == app::Mode::front);
  CHECK(app::menu_up(sh)->focused()->id == "play");
  app::step_shell(sh, down());
  app::step_shell(sh, down());
  s = app::step_shell(sh, accept());
  CHECK(s.quit);
  // Back on the front door's root does nothing.
  app::Shell other = app::make_shell();
  s = app::step_shell(other, back());
  CHECK(!s.quit && !s.play && other.mode == app::Mode::front);
}

TEST(the_options_page_shows_and_writes_the_settings_for_both_menus) {
  app::Shell sh = app::make_shell({.rung = fall_line::camera::Rung::far, .volume = 40});
  app::step_shell(sh, down());
  app::ShellStep s = app::step_shell(sh, accept());
  CHECK(sh.front.top().id == "options" && s.cue == app::MenuCue::page);
  CHECK(sh.front.focused()->id == "camera" && sh.front.focused()->choice == 3);
  s = app::step_shell(sh, right());
  CHECK(s.settings_changed && sh.settings.rung == fall_line::camera::Rung::high);
  app::step_shell(sh, down());
  s = app::step_shell(sh, accept());  // SOUND off
  CHECK(s.settings_changed && !sh.settings.sound);
  app::step_shell(sh, down());
  s = app::step_shell(sh, right());  // the volume up a step
  CHECK(sh.settings.volume == 45);
  // The pause menu's options page says the same.
  CHECK(sh.pause.page("options")->find("camera")->choice == 4);
  CHECK(!sh.pause.page("options")->find("sound")->on);
  CHECK(sh.pause.page("options")->find("volume")->value == 45);
  s = app::step_shell(sh, back());
  CHECK(sh.front.top().id == "front" && s.cue == app::MenuCue::page);
}

TEST(every_page_reads_at_the_three_reference_sizes) {
  const app::Shell sh = app::make_shell({}, "seed 38");
  for (auto [w, h] : {std::pair{1280, 720}, std::pair{390, 844}, std::pair{844, 390}}) {
    for (const std::string& p : app::shell_layout_problems(sh, w, h)) {
      CHECK_MSG(false, std::to_string(w) + "x" + std::to_string(h) + " " + p);
    }
  }
}

TEST(the_menu_draws_only_when_one_is_up) {
  app::Shell sh = app::make_shell();
  ge::ui::DrawList list;
  app::draw_shell(sh, list, 1280, 720, 0, 0);
  CHECK(list.size() > 0 && !sh.layout.items.empty());
  sh.mode = app::Mode::run;
  app::draw_shell(sh, list, 1280, 720, 0, 0);
  CHECK(list.size() == 0 && sh.layout.items.empty());
}
