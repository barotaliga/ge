// THE TEAM'S WORLD, PICTURED: one picture of a bot team (classic_bot_team.hpp) as its bots
// believe it, drawn on the CPU (engine/lab/raster.hpp) for a window, a snapshot or a video.
//
// THE MAP from above: the nav graph's floor, each node a square shaded by its height (the higher,
// the lighter), drawn once a map and kept, and the map's places by name (its `.loc` file's, else
// the bots' own off the items), each at its points' middle.
//
// EACH BOT in its own colour: where its own world puts it, its facing, its PATH (the line it
// means to walk: the pilot's route ahead with its lane off the line in; `BotTeamConfig::paths`, the
// console's `paths 0|1`, hides it) and its GOAL (the item it is going for, ringed, with its
// call); the ENEMIES IT BELIEVES IN: where each was last seen (a filled mark, fading over the six
// seconds it is remembered) or heard (a ring), joined to the bot that believes it.
//
// THE ITEMS as the team believes them: each key item (armours, mega, launcher, arc, quad, ring,
// shield) at its spot, filled while some bot believes it there, hollow with the seconds to its
// return (the soonest any bot believes) while taken.
//
// THE TEAM'S PACKETS TOGETHER: every enemy any bot's client was sent this tick, an orange cross at
// his place: what the four clients' packets show at once, which no single bot knows (the process
// rebuilds it from all of them; no bot reads it).
//
// THE PANEL on the right: the map, the clock and the score; each bot's line (health and armour,
// weapon, mode and role, goal, the place it stands, its pilot's next move); the team's last lines.
//
// The picture reads the team and writes nothing back.
//
// ge:summary The team's world pictured: map, bots, routes, goals, believed enemies, items, panel
// ge:keywords team-view visualizer strategy-view bots-overview beliefs route goal contacts
// ge:keywords item-timers team-packets panel video team_view_video area-labels place-names
// ge:skill bots net labs
// ge:test tests/shooters_edge_bot_team_test.cpp
#pragma once

#include <memory>
#include <string>

#include "capture/av_export.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"

namespace shooters_edge::net {

struct TeamView {
  std::string map;           // the map the background was drawn for
  ge::lab::Picture floor;    // the map's floor, drawn once a map
  sh::Vec3f lo, hi;          // the drawn bounds
  double scale = 1;          // px a unit
  int map_w = 0, map_h = 0;  // the map's part of the picture
};

// The team now, `w` x `h` (the map on the left, the panel on the right).
ge::lab::Picture team_view(TeamView& v, const BotTeam& t, int w, int h);

// The pictures written to a video file (engine/capture: an .mp4 by the platform's encoder, a
// .webm, an .mkv).
struct TeamViewVideo {
  ge::capture::AvExport out;
  bool open = false;
  int64_t frames = 0;
};
bool team_view_video_open(TeamViewVideo& v, const std::string& file, int w, int h, double fps,
                          std::string& error);
bool team_view_video_frame(TeamViewVideo& v, const ge::lab::Picture& p, std::string& error);
bool team_view_video_close(TeamViewVideo& v, std::string& error);

}  // namespace shooters_edge::net
