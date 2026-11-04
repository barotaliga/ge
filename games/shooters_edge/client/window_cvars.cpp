// The window's own settings (window_cvars.hpp).
//
// ge:summary The window's console settings: vid_fullscreen, vid_display, vid_displays
// ge:keywords add_window_cvars
// ge:skill platform shooter
#include "shooters_edge/client/window_cvars.hpp"

#include <string>
#include <vector>

#include "platform/window.hpp"

namespace shooters_edge::client {

void add_window_cvars(ge::console::Console& con, bool& fullscreen, int& display) {
  con.cvars().bind({.name = "vid_fullscreen",
                    .fallback = "0",
                    .help = "1: the window over the whole screen (the desktop's own mode), "
                            "0: a window",
                    .kind = ge::console::CvarKind::boolean},
                   &fullscreen);
  con.cvars().bind({.name = "vid_display",
                    .fallback = "0",
                    .help = "the display vid_fullscreen covers: 0 the one the window is on, n "
                            "the n-th that vid_displays lists",
                    .kind = ge::console::CvarKind::integer},
                   &display);
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
