// THE WINDOW'S OWN SETTINGS: whether it covers the screen and which display it covers
// (`vid_fullscreen`, `vid_display`) and the command that lists the displays (`vid_displays`). Built
// into the `shooters_edge` window alone: it asks the platform for the displays.
//
// ge:summary The window's console settings: vid_fullscreen, vid_display, vid_displays
// ge:keywords vid_fullscreen vid_display vid_displays fullscreen display window cvars
// ge:skill platform shooter
#pragma once

#include "console/console.hpp"

namespace shooters_edge::client {

// The cvars over `fullscreen` and `display` (both must outlive `con`), and `vid_displays`.
void add_window_cvars(ge::console::Console& con, bool& fullscreen, int& display);

}  // namespace shooters_edge::client
