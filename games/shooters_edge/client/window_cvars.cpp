// The window's own settings (window_cvars.hpp).
//
// ge:summary The window's console settings: vid_fullscreen, vid_display, volume, scr_levelstats
// ge:keywords add_window_cvars
// ge:skill platform shooter
#include "shooters_edge/client/window_cvars.hpp"

#include <string>
#include <vector>

#include "platform/window.hpp"

namespace shooters_edge::client {

void add_window_cvars(ge::console::Console& con, WindowCvars& w) {
  con.cvars().bind(
      {.name = "vid_fullscreen",
       .fallback = "0",
       .help = "Whether the window covers the whole screen, in the desktop's own mode.",
       .kind = ge::console::CvarKind::boolean,
       .values = "0: a window (the default); 1: the whole screen"},
      &w.fullscreen);
  con.cvars().bind({.name = "vid_display",
                    .fallback = "0",
                    .help = "The display vid_fullscreen covers.",
                    .kind = ge::console::CvarKind::integer,
                    .values = "0: the one the window is on (the default); 1..N: that one of the "
                              "displays vid_displays lists"},
                   &w.display);
  con.cvars().bind({.name = "volume",
                    .fallback = "0.7",
                    .help = "The game's sound, a played demo's sounds and voices too.",
                    .kind = ge::console::CvarKind::number,
                    .min = 0,
                    .max = 1,
                    .values = "0: silent; 0..1: that loudness (0.7 the default)"},
                   &w.volume);
  con.cvars().bind({.name = "scr_levelstats",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether the scores key in a campaign also shows the level's time, "
                            "secrets and kills, as the intermission draws them.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: the scores alone; 1: the level's figures too (the default)"},
                   &w.level_stats);
  con.add_command("vid_displays", "list the displays vid_display numbers",
                  [](ge::console::Console& c, const ge::console::Args&) {
                    const std::vector<ge::platform::Display> all = ge::platform::displays();
                    if (all.empty()) c.print("vid_displays: the desktop named no display");
                    for (const ge::platform::Display& d : all)
                      c.print(std::to_string(d.number) + ": " + d.name + ", " +
                              std::to_string(d.width_px) + "x" + std::to_string(d.height_px));
                  });
}

}  // namespace shooters_edge::client
