// Fall Line's menus and the shell between them and the run (menus.hpp).
//
// ge:summary Fall Line's menus: the pages, the shell's modes, the backdrops and the draw
// ge:keywords fall-line front-door pause-page options shell step_shell draw_shell
// ge:skill ui
// ge:test tests/fall_line_menus_test.cpp
#include "fall_line/app/menus.hpp"

#include <string>

namespace fall_line::app {

namespace {

using ge::ui::Item;
using ge::ui::ItemKind;
using ge::ui::Menu;
using ge::ui::Page;

std::string s(std::string_view v) { return std::string(v); }

void add_options(Menu& m) {
  const MenuText& t = kMenuText;
  Page& opt = m.add_page("options", s(t.options));
  Item view{
      .id = "camera", .text = s(t.camera), .kind = ItemKind::choice, .hint = s(t.camera_hint)};
  for (std::string_view r : t.rungs) view.choices.push_back(s(r));
  opt.items = {{.id = "h-camera", .text = s(t.h_camera), .kind = ItemKind::label},
               view,
               {.id = "h-sound", .text = s(t.h_sound), .kind = ItemKind::label},
               {.id = "sound", .text = s(t.sound), .kind = ItemKind::toggle},
               {.id = "volume",
                .text = s(t.volume),
                .kind = ItemKind::slider,
                .min = 0,
                .max = 100,
                .step = 5,
                .unit = "%"},
               {.id = "back", .text = s(t.back), .kind = ItemKind::back}};
}

// The options page shows the settings (when it opens) ...
void show_settings(Menu& m, const MenuSettings& set) {
  Page* opt = m.page("options");
  if (!opt) return;
  if (Item* i = opt->find("camera")) i->choice = static_cast<int>(set.rung);
  if (Item* i = opt->find("sound")) i->on = set.sound;
  if (Item* i = opt->find("volume")) i->value = set.volume;
}

// ... and writes them back on a change.
void read_settings(const Menu& m, MenuSettings& set) {
  const Page* opt = m.page("options");
  if (!opt) return;
  if (const Item* i = opt->find("camera"))
    set.rung = static_cast<camera::Rung>(i->choice % camera::kRungs);
  if (const Item* i = opt->find("sound")) set.sound = i->on;
  if (const Item* i = opt->find("volume")) set.volume = i->value;
}

// The pause menu's root stands PAUSED where the front door's logo stands.
const ge::ui::MenuStyle& style_up(const Shell& shell) {
  return shell.mode == Mode::pause ? shell.pause_style : shell.style;
}

MenuCue louder(MenuCue a, MenuCue b) { return static_cast<int>(b) > static_cast<int>(a) ? b : a; }

}  // namespace

Shell make_shell(const MenuSettings& settings, std::string_view footer) {
  const MenuText& t = kMenuText;
  Shell sh;
  sh.settings = settings;
  Page& front = sh.front.add_page("front");
  front.items = {
      {.id = "play", .text = s(t.play), .hint = s(t.play_hint), .prominent = true},
      {.id = "options", .text = s(t.options), .hint = s(t.options_hint), .opens = "options"},
      {.id = "quit", .text = s(t.quit), .hint = s(t.quit_hint)}};
  add_options(sh.front);
  Page& pause = sh.pause.add_page("pause", s(t.paused));
  pause.items = {
      {.id = "resume", .text = s(t.resume), .hint = s(t.resume_hint), .prominent = true},
      {.id = "options", .text = s(t.options), .hint = s(t.options_hint), .opens = "options"},
      {.id = "to-title", .text = s(t.to_title), .hint = s(t.to_title_hint)}};
  add_options(sh.pause);
  show_settings(sh.front, settings);
  show_settings(sh.pause, settings);
  sh.style.logo = s(t.logo);
  sh.style.help = s(t.help);
  sh.style.footer = s(footer);
  sh.style.on = s(t.on);
  sh.style.off = s(t.off);
  sh.pause_style = sh.style;
  sh.pause_style.logo = s(t.paused);
  return sh;
}

Menu* menu_up(Shell& shell) {
  return shell.mode == Mode::front   ? &shell.front
         : shell.mode == Mode::pause ? &shell.pause
                                     : nullptr;
}

const Menu* menu_up(const Shell& shell) {
  return shell.mode == Mode::front   ? &shell.front
         : shell.mode == Mode::pause ? &shell.pause
                                     : nullptr;
}

ShellStep step_shell(Shell& shell, const ge::ui::MenuInput& in) {
  ShellStep out;
  if (shell.mode == Mode::run) {
    if (in.back) {
      shell.mode = Mode::pause;
      shell.pause.reset();
      shell.pause.top().focus = -1;
      shell.pause.move_focus(0);
      show_settings(shell.pause, shell.settings);
      out.pause = true;
      out.cue = MenuCue::page;
    }
    return out;
  }
  Menu& m = *menu_up(shell);
  m.move_focus(0);  // a page shown for the first time has its first stop
  const Item* before = m.focused();
  const std::string was = before ? before->id + "@" + m.top().id : std::string();
  const bool pointer =
      !shell.layout.items.empty() && m.top().items.size() == shell.layout.items.size();
  const std::vector<ge::ui::MenuEvent> events =
      m.input(in, pointer ? shell.layout.items : std::vector<ge::ui::Rect>{});
  for (const ge::ui::MenuEvent& e : events) {
    switch (e.kind) {
      case ge::ui::EventKind::activated:
        out.cue = louder(out.cue, MenuCue::press);
        if (e.item == "play") {
          out.play = true;
        } else if (e.item == "quit") {
          out.quit = true;
        } else if (e.item == "resume") {
          out.resume = true;
        } else if (e.item == "to-title") {
          out.to_front = true;
        }
        break;
      case ge::ui::EventKind::changed:
        out.cue = louder(out.cue, MenuCue::focus);
        read_settings(m, shell.settings);
        out.settings_changed = true;
        break;
      case ge::ui::EventKind::opened:
        if (e.page == "options") show_settings(m, shell.settings);
        out.cue = louder(out.cue, MenuCue::page);
        break;
      case ge::ui::EventKind::closed:
        out.cue = louder(out.cue, MenuCue::page);
        break;
      case ge::ui::EventKind::back_at_root:
        // The pause page's back is RESUME; the front door's does nothing (QUIT is an item).
        if (shell.mode == Mode::pause) {
          out.resume = true;
          out.cue = louder(out.cue, MenuCue::page);
        }
        break;
    }
  }
  const Item* after = m.focused();
  const std::string now = after ? after->id + "@" + m.top().id : std::string();
  if (now != was && out.cue == MenuCue::none) out.cue = MenuCue::focus;
  if (out.play) {
    shell.mode = Mode::run;
    shell.front.reset();
  } else if (out.resume) {
    shell.mode = Mode::run;
  } else if (out.to_front) {
    shell.mode = Mode::front;
    shell.front.reset();
    shell.front.top().focus = -1;
    shell.front.move_focus(0);
    show_settings(shell.front, shell.settings);
  }
  if (out.settings_changed) {
    // Both options pages say the same.
    show_settings(shell.front, shell.settings);
    show_settings(shell.pause, shell.settings);
  }
  return out;
}

ge::ui::Background shell_backdrop(Mode mode) {
  if (mode == Mode::pause) {
    ge::ui::Background bg;
    bg.dim(0.45f).scrim(ge::ui::Side::left, 0.6f, 0.7f).vignette(0.4f);
    return bg;
  }
  ge::ui::Background bg;
  bg.scrim(ge::ui::Side::left, 0.8f, 0.65f).vignette(0.45f);
  return bg;
}

void draw_shell(Shell& shell, ge::ui::DrawList& list, int width, int height, double dt_s,
                double time_s) {
  list.begin(width, height);
  shell.width = width;
  shell.height = height;
  Menu* m = menu_up(shell);
  if (!m) {
    shell.layout = {};
    return;
  }
  if (m->top().focus < 0) m->move_focus(0);
  const ge::ui::MenuStyle& style = style_up(shell);
  shell.layout = ge::ui::layout_menu(*m, width, height, style);
  shell.view.frame(*m, shell.layout, dt_s);
  ge::ui::draw_background(list, shell_backdrop(shell.mode),
                          {0, 0, static_cast<float>(width), static_cast<float>(height)}, time_s);
  shell.view.draw(list, *m, shell.layout, style);
}

std::vector<std::string> shell_layout_problems(const Shell& shell, int width, int height) {
  std::vector<std::string> out;
  for (const Menu* menu : {&shell.front, &shell.pause}) {
    const ge::ui::MenuStyle& style = menu == &shell.pause ? shell.pause_style : shell.style;
    for (const char* page : {"", "options"}) {
      Menu m = *menu;
      if (*page && !m.open(page)) continue;
      m.move_focus(0);
      const ge::ui::MenuLayout L = ge::ui::layout_menu(m, width, height, style);
      for (const std::string& p : ge::ui::layout_problems(m, L, style)) {
        out.push_back(m.top().id + ": " + p);
      }
    }
  }
  return out;
}

}  // namespace fall_line::app
