// Fall Line's menus and shell (games/fall_line/app/menus.hpp): the app opens on the front door,
// PLAY starts the run, the back press pauses it and RESUME or back returns, QUIT TO TITLE goes
// back to the front door; the options page shows and writes the settings, both menus' options
// agree; every move makes its sound cue; and every page reads at the three reference sizes.
//
// ge:summary Tests for Fall Line's front door, pause page, options and the shell's modes
// ge:keywords test front-door pause-page options shell step_shell menu-settings layout-problems
// ge:skill ui testing
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/app/menus.hpp"
#include "save/disk.hpp"
#include "save/picture.hpp"

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

namespace {

std::vector<app::SaveRow> two_saves() {
  std::vector<app::SaveRow> rows(2);
  rows[0] = {"quick", "Seed 38, the race", "the second gate", 75, 2000, {}};
  rows[0].picture = ge::ui::make_image(32, 18, {0.2f, 0.4f, 0.8f, 1});
  rows[1] = {"slot-2", "Seed 7, the race", "the start", 12, 1000, {}};
  return rows;
}

}  // namespace

TEST(no_saves_offered_until_the_app_sets_its_store) {
  app::Shell sh = app::make_shell();
  CHECK(sh.front.top().find("continue") == nullptr && sh.front.top().find("load") == nullptr);
  app::set_saves(sh, {});
  CHECK(sh.front.top().find("continue") == nullptr);  // nothing to continue yet
  CHECK(sh.front.top().find("play")->prominent);
  CHECK(sh.pause.top().find("save") != nullptr && sh.pause.top().find("load")->disabled);
}

TEST(continue_loads_the_newest_save_and_load_lists_them_with_their_pictures) {
  app::Shell sh = app::make_shell();
  app::set_saves(sh, two_saves());
  app::step_shell(sh, {});
  CHECK(app::menu_up(sh)->focused()->id == "continue");
  CHECK(!sh.front.top().find("play")->prominent);
  // LOAD: the second save's line, then its slot loads.
  app::step_shell(sh, down());
  app::step_shell(sh, down());
  CHECK(app::menu_up(sh)->focused()->id == "load");
  app::step_shell(sh, accept());
  CHECK(sh.front.top().id == "load");
  const ge::ui::Item* first = app::menu_up(sh)->focused();
  CHECK(first->id == "load:quick" && first->picture != nullptr && first->picture->width == 32);
  CHECK(first->text == "QUICKSAVE  Seed 38, the race");
  CHECK(first->hint == "the second gate, 1:15 played");
  app::step_shell(sh, down());
  CHECK(app::menu_up(sh)->focused()->picture == nullptr);  // slot-2 has none
  app::ShellStep s = app::step_shell(sh, accept());
  CHECK(s.load == "slot-2" && sh.mode == app::Mode::run);
  // From the front door again, CONTINUE: the newest.
  app::Shell again = app::make_shell();
  app::set_saves(again, two_saves());
  s = app::step_shell(again, accept());
  CHECK(s.load == "quick" && again.mode == app::Mode::run);
}

TEST(the_pause_page_saves_into_a_slot_and_stays_up) {
  app::Shell sh = app::make_shell();
  app::set_saves(sh, two_saves());
  app::step_shell(sh, accept());  // CONTINUE: on the run
  app::step_shell(sh, back());
  app::step_shell(sh, down());
  CHECK(app::menu_up(sh)->focused()->id == "save");
  app::step_shell(sh, accept());
  CHECK(sh.pause.top().id == "save");
  CHECK(app::menu_up(sh)->focused()->text == "SLOT 1  EMPTY");
  app::step_shell(sh, down());
  CHECK(app::menu_up(sh)->focused()->text == "SLOT 2  Seed 7, the race");
  const app::ShellStep s = app::step_shell(sh, accept());
  CHECK(s.save == "slot-2" && sh.mode == app::Mode::pause && sh.pause.at_root());
  // The app saved: the pages show it, and the page up still reads.
  auto rows = two_saves();
  rows.insert(rows.begin(), {"slot-2", "Seed 38, saved now", "", 90, 3000, {}});
  rows.pop_back();
  app::set_saves(sh, rows);
  CHECK(sh.pause.page("save")->find("save:slot-2")->text == "SLOT 2  Seed 38, saved now");
  CHECK(app::shell_layout_problems(sh, 1280, 720).empty());
  CHECK(app::shell_layout_problems(sh, 390, 844).empty());
}

TEST(the_store_gives_this_runs_saves_newest_first_with_their_pictures) {
  ge::save::SimDisk disk;
  ge::save::Store store(disk, {.game = "fall_line"});
  CHECK_MSG(store.open(), store.problem());
  auto file = [](std::string setup, int64_t when, bool pictured) {
    ge::save::Meta meta;
    meta.game = "fall_line";
    meta.title = "run " + std::to_string(when);
    meta.saved_unix = when;
    meta.setup = std::move(setup);
    std::vector<uint8_t> out;
    ge::save::FileWriter w(out, meta);
    if (pictured) {
      std::vector<uint8_t> rgba(size_t{16} * 8 * 4, 200);
      ge::save::put_picture(w, 16, 8, rgba, 16);
    }
    w.put(ge::save::tag("DATA"), 0, 1, when);
    CHECK(w.finish());
    return out;
  };
  CHECK_MSG(store.write("slot-1", file("seed 38", 100, true)), store.problem());
  CHECK(store.write("slot-2", file("seed 7", 300, true)));
  CHECK(store.write("quick", file("seed 38", 200, false)));
  const std::vector<app::SaveRow> rows = app::rows_of_store(store, "seed 38");
  CHECK(rows.size() == 2);
  CHECK(rows[0].slot == "quick" && rows[0].picture.empty());
  CHECK(rows[1].slot == "slot-1" && rows[1].picture.width == 16 && rows[1].picture.height == 8);
}
