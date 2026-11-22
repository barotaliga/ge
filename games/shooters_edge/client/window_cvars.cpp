// The window's settings in this game (window_cvars.hpp).
//
// ge:summary The window's console settings: the engine's, scr_levelstats, showpause, host_framerate
// ge:keywords add_window_cvars scr_levelstats showpause host_framerate slow-motion scr_printspeed
// ge:keywords vid_restart
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
  con.cvars().bind({.name = "scr_printspeed",
                    .fallback = "8",
                    .flags = ge::console::kArchive,
                    .help = "How fast the end of an episode types its text out, in characters a "
                            "second.",
                    .kind = ge::console::CvarKind::number,
                    .min = 1,
                    .max = 1000,
                    .values = "1..1000: that many characters a second (8 the default)"},
                   &w.print_speed);
  // vid_restart: the window applies its video settings the moment they change, so a typed one is
  // refused and says why (a server's stuffed one is dropped quietly).
  con.add_command("vid_restart", "vid_restart: not needed; video settings apply at once",
                  [said = false](ge::console::Console& c, const ge::console::Args& a) mutable {
                    if (a.source != ge::console::Source::local) return;
                    c.print("Manual 'vid_restart' command ignored.");
                    if (said) return;
                    said = true;
                    c.print(
                        "Video settings are applied as soon as you change them, so a video "
                        "restart is never needed.");
                  });
}

}  // namespace shooters_edge::client
