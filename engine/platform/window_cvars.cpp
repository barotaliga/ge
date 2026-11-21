// The window's own settings (window_cvars.hpp).
//
// ge:summary The window's console settings for any game: vid_fullscreen, vid_display, volume
// ge:keywords add_window_cvars vid_displays
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
  con.add_command("vid_displays", "list the displays vid_display numbers",
                  [](console::Console& c, const console::Args&) {
                    const std::vector<Display> all = displays();
                    if (all.empty()) c.print("vid_displays: the desktop named no display");
                    for (const Display& d : all)
                      c.print(std::to_string(d.number) + ": " + d.name + ", " +
                              std::to_string(d.width_px) + "x" + std::to_string(d.height_px));
                  });
}

bool alt_enter(const InputSnapshot& in, console::Console& con) {
  constexpr size_t kLeftAlt = 226, kRightAlt = 230;  // the keys' HID usages
  if (!in.tapped(Key::enter) || !(in.held[kLeftAlt] || in.held[kRightAlt])) return false;
  const console::Cvar* v = con.cvars().find("vid_fullscreen");
  con.execute(std::string("vid_fullscreen ") + (v != nullptr && v->value != "0" ? "0" : "1"));
  return true;
}

}  // namespace ge::platform
