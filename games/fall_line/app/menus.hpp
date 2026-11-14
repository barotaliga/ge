// FALL LINE'S MENUS: the front door, its options page and the pause page, as engine/ui menus
// (ui/menu.hpp's model, ui/menu_view.hpp's view), and the app's shell between them and the run.
//
// THE SHELL has three modes. FRONT: the front door over the live mountain (the bot skis the map
// behind it, the picture softened by a scrim on the column's side and a vignette); PLAY starts a
// run, OPTIONS opens the options page, QUIT leaves. RUN: the player's run, no menu; the back press
// (Escape) pauses it. PAUSE: the pause page over the stopped run, dimmed; RESUME or the back press
// returns to it, OPTIONS opens the same options page, QUIT TO TITLE goes back to the front door.
//
// THE SAVES (docs/saves.md § Fall Line's saves in the game), once the app has set its store's
// with `set_saves`: CONTINUE on the front door loads the newest save, LOAD opens a page of the
// newest saves, each showing its picture beside the column while it has the focus; on the pause
// page SAVE opens the slots to save into (a used one shows its picture) and LOAD the same list.
// The shell answers the slot (`ShellStep::load`, `save`); the app reads and writes the store.
//
// THE OPTIONS are what the app can change today: the camera's rung, the sound and the master
// volume. They live in `MenuSettings`, the one copy the app reads; the options page shows it when
// it opens and writes it back on every change. (The console's cvars, M21, take them over.)
//
// WHAT THE PLAYER READS comes from `kMenuText` here, the game's strings table for its menus;
// nothing else in the shell spells a word.
//
// A frame of the shell is `step_shell` (the frame's presses, as ui::MenuInput: the app's input
// layer makes it) and then `draw_shell` (the menu, if one is up, into a draw list the frame draws
// over the picture). `step_shell` answers what the app must do (start a run, pause, quit) and which
// of the menu's sounds this frame makes; it calls nothing, so the shell is driven and tested
// headless.
//
// ge:summary Fall Line's front door, options and pause page, and the shell between them and the run
// ge:keywords fall-line front-door main-menu pause-page options shell mode play resume quit
// ge:keywords strings menu-sounds menu-settings scrim live-scene continue load-card save-card
// ge:keywords saves slots save-picture
// ge:skill ui
// ge:test tests/fall_line_menus_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fall_line/camera/rungs.hpp"
#include "save/store.hpp"
#include "ui/background.hpp"
#include "ui/draw.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"

namespace fall_line::app {

// Every word of the menus.
struct MenuText {
  std::string_view logo = "FALL LINE";
  std::string_view play = "PLAY";
  std::string_view play_hint = "Ski the mountain from the top";
  std::string_view options = "OPTIONS";
  std::string_view options_hint = "Camera and sound";
  std::string_view quit = "QUIT";
  std::string_view quit_hint = "Leave the mountain";
  std::string_view resume = "RESUME";
  std::string_view resume_hint = "Back to the run";
  std::string_view to_title = "QUIT TO TITLE";
  std::string_view to_title_hint = "Leave the run for the front door";
  std::string_view paused = "PAUSED";
  std::string_view h_camera = "CAMERA";
  std::string_view camera = "VIEW";
  std::string_view camera_hint = "Where the camera rides; C changes it on the run";
  std::string_view h_sound = "SOUND";
  std::string_view sound = "SOUND";
  std::string_view volume = "MASTER VOLUME";
  std::string_view back = "BACK";
  std::string_view help = "ENTER SELECT    ESC BACK    ARROWS MOVE";
  std::string_view on = "ON", off = "OFF";
  std::string_view continue_ = "CONTINUE";
  std::string_view continue_hint = "Back to your last save";
  std::string_view load = "LOAD";
  std::string_view load_hint = "Pick up a run you saved";
  std::string_view save = "SAVE";
  std::string_view save_hint = "Keep the run as it stands";
  std::string_view saved_runs = "SAVED RUNS";
  std::string_view save_into = "SAVE INTO";
  std::string_view empty_slot = "EMPTY";
  std::string_view quicksave = "QUICKSAVE";
  std::string_view autosave = "AUTOSAVE";
  std::string_view slot = "SLOT";
  std::string_view played = "played";
  // The rungs' names on the VIEW choice, in Rung order.
  std::string_view rungs[camera::kRungs] = {"TIPS", "HELMET", "CHASE", "FAR", "HIGH", "ORBIT"};
};
inline constexpr MenuText kMenuText{};

enum class Mode : uint8_t { front, run, pause };

// What the options set; the app reads it every frame.
struct MenuSettings {
  camera::Rung rung = camera::Rung::chase;
  bool sound = true;
  double volume = 80;  // %, the master fader
};

// Which of the menu's sounds a frame makes (one at most: the biggest).
enum class MenuCue : uint8_t { none, focus, press, page };

// What a frame of the shell asks of the app.
struct ShellStep {
  bool play = false;      // start a new run (the shell is now in RUN)
  bool pause = false;     // the run stopped under the pause page
  bool resume = false;    // the run goes on
  bool to_front = false;  // the run left for the front door
  bool quit = false;      // leave the app
  bool settings_changed = false;
  std::string load;  // a slot to load (CONTINUE: the newest); the shell is now in RUN
  std::string save;  // a slot to save the run into; the pause page stays up
  MenuCue cue = MenuCue::none;
};

// A save as the menus show it: its slot, its words and its picture (the store's, read off the
// file's front). The app fills them from its save store (engine/save/store.hpp) with `set_saves`.
struct SaveRow {
  std::string slot;   // "slot-3", "quick", "auto-1"
  std::string title;  // the save's meta title
  std::string place;  // where in the run
  double play_s = 0;
  int64_t saved_unix = 0;
  ge::ui::Image picture;  // empty: none
};

// How many saves the LOAD page lists (the newest), and the slots the SAVE page offers.
inline constexpr int kLoadRows = 7;
inline constexpr int kSaveSlots = 6;

struct Shell {
  Mode mode = Mode::front;
  ge::ui::Menu front;  // root "front", "options"
  ge::ui::Menu pause;  // root "pause", "options"
  ge::ui::MenuView view;
  ge::ui::MenuStyle style;        // the front door's
  ge::ui::MenuStyle pause_style;  // the pause page's: PAUSED where the logo stands
  ge::ui::MenuLayout layout;      // the last drawn frame's, for the pointer
  MenuSettings settings;
  int width = 0, height = 0;
  // The saves the menus offer (set_saves): CONTINUE and LOAD on the front door, SAVE and LOAD on
  // the pause page. Until a store is set the menus offer none of them.
  bool saving = false;
  std::vector<SaveRow> saves;  // the newest first
};

Shell make_shell(const MenuSettings& settings = {}, std::string_view footer = {});

// The menus' saves, newest first: CONTINUE and LOAD on the front door, SAVE and LOAD on the pause
// page, each slot with its picture. Call again after every save so the pages show it.
void set_saves(Shell& shell, std::vector<SaveRow> saves);

// The SAVE page's line for a slot and the LOAD page's for a save: "SLOT 3  Seed 38, the red run".
std::string save_line(const SaveRow& row);
std::string slot_name(std::string_view slot);

// The menus' rows from the app's save store: every save of this run's setup (`setup_text`, the
// meta's: a save of another seed or field does not load into this run), newest first, each with
// its picture read off the file's front.
std::vector<SaveRow> rows_of_store(ge::save::Store& store, std::string_view setup_text);

// The menu up in the shell's mode (null on the run).
ge::ui::Menu* menu_up(Shell& shell);
const ge::ui::Menu* menu_up(const Shell& shell);

// One frame's presses. On the run only `back` counts (it pauses).
ShellStep step_shell(Shell& shell, const ge::ui::MenuInput& in);

// The backdrop the menu up stands on, over the live picture: the front door's scrim and vignette,
// or the pause page's dimming.
ge::ui::Background shell_backdrop(Mode mode);

// The menu up (none on the run) into `list`, sized width x height, its view stepped by `dt_s`.
// `begin`s the list either way.
void draw_shell(Shell& shell, ge::ui::DrawList& list, int width, int height, double dt_s,
                double time_s);

// What is wrong with the layout of every page of both menus at width x height (ui/menu_view.hpp's
// `layout_problems`), each line naming its page; empty when every page reads.
std::vector<std::string> shell_layout_problems(const Shell& shell, int width, int height);

}  // namespace fall_line::app
