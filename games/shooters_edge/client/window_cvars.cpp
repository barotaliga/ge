// The window's settings in this game (window_cvars.hpp).
//
// ge:summary The window's console settings: the engine's, scr_levelstats, showpause, host_framerate
// ge:keywords add_window_cvars scr_levelstats showpause host_framerate slow-motion
// ge:skill platform shooter
#include "shooters_edge/client/window_cvars.hpp"

#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

void add_window_cvars(ge::console::Console& con, WindowCvars& w) {
  {
    ge::console::CvarLayer engine(con.cvars(), kLayerEngine);
    ge::platform::add_window_cvars(con, w);
  }
  ge::console::CvarLayer shell(con.cvars(), kLayerShell);
  con.cvars().bind({.name = "scr_levelstats",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether the scores key in a campaign also shows the level's time, "
                            "secrets and kills, as the intermission draws them.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: the scores alone; 1: the level's figures too (the default)"},
                   &w.level_stats);
  con.cvars().bind({.name = "showpause",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether a paused campaign shows the pause picture.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: no picture; 1: the picture while paused (the default)"},
                   &w.show_pause);
  con.cvars().bind({.name = "host_framerate",
                    .fallback = "0",
                    .help = "Slow or fast motion in a game this window runs: each frame moves the "
                            "game this many seconds instead of the time that passed.",
                    .kind = ge::console::CvarKind::number,
                    .min = 0,
                    .max = 0.1,
                    .values = "0: the real time (the default); up to 0.1: seconds a frame"},
                   &w.host_framerate);
}

}  // namespace shooters_edge::client
