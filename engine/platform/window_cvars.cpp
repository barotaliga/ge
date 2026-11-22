// The window's own settings (window_cvars.hpp).
//
// ge:summary The window's settings for any game: vid_fullscreen, vid_display, volume, s_auto_focus
// ge:keywords add_window_cvars vid_displays follow_window_cvars window_style
// ge:skill platform console
// ge:test tests/platform_window_cvars_test.cpp
#include "platform/window_cvars.hpp"

#include <string>
#include <vector>

#include "platform/window.hpp"

namespace ge::platform {

void add_window_cvars(console::Console& con, WindowCvars& w) {
  con.cvars().bind(
      {.name = "vid_fullscreen",
       .fallback = "0",
       .help = "Whether the window covers the whole screen, in the desktop's own mode.",
       .kind = console::CvarKind::boolean,
       .values = "0: a window (the default); 1: the whole screen"},
      &w.fullscreen);
  con.cvars().bind({.name = "vid_display",
                    .fallback = "0",
                    .help = "The display vid_fullscreen covers.",
                    .kind = console::CvarKind::integer,
                    .values = "0: the one the window is on (the default); 1..N: that one of the "
                              "displays vid_displays lists"},
                   &w.display);
  con.cvars().bind({.name = "volume",
                    .fallback = "0.7",
                    .help = "The game's sound, a played demo's sounds and voices too.",
                    .kind = console::CvarKind::number,
                    .min = 0,
                    .max = 1,
                    .values = "0: silent; 0..1: that loudness (0.7 the default)"},
                   &w.volume);
  con.cvars().bind({.name = "s_auto_focus",
                    .fallback = "0",
                    .flags = console::kArchive,
                    .help = "When the game is heard while you are in another window.",
                    .kind = console::CvarKind::integer,
                    .min = 0,
                    .max = 2,
                    .values = "0: always (the default); 1: not while the game is minimised; 2: "
                              "only while the game's window is the one you are in"},
                   &w.auto_focus);
  const struct {
    const char* name;
    const char* help;
    const char* values;
    bool* field;
  } style[] = {
      {"win_noborder", "Whether the window has no border or title bar.",
       "0: a border (the default); 1: none", &w.no_border},
      {"win_notitle", "Whether the window has no title bar (and so no border).",
       "0: a title bar (the default); 1: none", &w.no_title},
      {"win_alwaysontop", "Whether the window stays above every other window.",
       "0: as any window (the default); 1: always on top", &w.on_top},
      {"win_noresize", "Whether the window's edges no longer resize it.",
       "0: resized by its edges (the default); 1: a fixed size", &w.no_resize},
  };
  for (const auto& s : style)
    con.cvars().bind({.name = s.name,
                      .fallback = "0",
                      .flags = console::kArchive,
                      .help = s.help,
                      .kind = console::CvarKind::boolean,
                      .values = s.values},
                     s.field);
  con.add_command("vid_displays", "list the displays vid_display numbers",
                  [](console::Console& c, const console::Args&) {
                    const std::vector<Display> all = displays();
                    if (all.empty()) c.print("vid_displays: the desktop named no display");
                    for (const Display& d : all)
                      c.print(std::to_string(d.number) + ": " + d.name + ", " +
                              std::to_string(d.width_px) + "x" + std::to_string(d.height_px));
                  });
}

WindowStyle window_style(const WindowCvars& w) {
  return {.border = !w.no_border && !w.no_title, .on_top = w.on_top, .resizable = !w.no_resize};
}

void follow_window_cvars(Window& window, WindowCvars& w) {
  if (w.fullscreen != w.full_applied) {  // the size follows through `InputSnapshot::resized`
    if (!set_fullscreen(window, w.fullscreen, w.display)) w.fullscreen = w.full_applied;
    w.full_applied = w.fullscreen;
  }
  const WindowStyle want = window_style(w);
  if (!w.fullscreen &&
      (want.border != w.style_applied.border || want.on_top != w.style_applied.on_top ||
       want.resizable != w.style_applied.resizable)) {
    set_window_style(window, want);  // a refusal is said once: it is not asked again
    w.style_applied = want;
  }
}

bool alt_enter(const InputSnapshot& in, console::Console& con) {
  constexpr size_t kLeftAlt = 226, kRightAlt = 230;  // the keys' HID usages
  if (!in.tapped(Key::enter) || !(in.held[kLeftAlt] || in.held[kRightAlt])) return false;
  const console::Cvar* v = con.cvars().find("vid_fullscreen");
  con.execute(std::string("vid_fullscreen ") + (v != nullptr && v->value != "0" ? "0" : "1"));
  return true;
}

double heard_volume(const WindowCvars& w, bool focused, bool minimised) {
  const bool silent = (w.auto_focus >= 1 && minimised) || (w.auto_focus >= 2 && !focused);
  return silent ? 0 : w.volume;
}

}  // namespace ge::platform
