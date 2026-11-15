// The window's settings in this game (window_cvars.hpp).
//
// ge:summary The window's console settings: the engine's and scr_levelstats
// ge:keywords add_window_cvars scr_levelstats
// ge:skill platform shooter
#include "shooters_edge/client/window_cvars.hpp"

namespace shooters_edge::client {

void add_window_cvars(ge::console::Console& con, WindowCvars& w) {
  ge::platform::add_window_cvars(con, w);
  con.cvars().bind({.name = "scr_levelstats",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether the scores key in a campaign also shows the level's time, "
                            "secrets and kills, as the intermission draws them.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: the scores alone; 1: the level's figures too (the default)"},
                   &w.level_stats);
}

}  // namespace shooters_edge::client
