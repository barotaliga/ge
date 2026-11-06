// THE BOT TEAM'S MAP IN THE WINDOW: the map itself, drawn by the game's world renderer from above,
// with the team's beliefs over it (`shooters_edge --bots=N --server=...`): where the window's
// bots stand and go, the enemies they see, each area of the map in the colour of what the team
// believes of it (net/classic_team_areas.hpp: green safe, red lost, orange contested, gray
// unknown), the floor's liquids, the map's team plan (bots/map_plan.hpp: its base, its forward
// area, its rushes), the key items with their believed timers, and a panel: each bot's line, the
// plan and the team's lines as they are said.
//
// THE CAMERA looks straight down on the map's floor, its middle and its span (units across the
// picture's height) moved by the keys: the arrows (or WASD with the console up) pan, `-` and `=`
// zoom, Home fits the map; a drag with the left button pans.
//
// THE OVERLAY is the 2D draw list over the world's frame: each nav node a square of its area's
// colour on the floor where the camera projects it (the higher floors drawn over the lower), a
// liquid's in its own colour, the players and the items as marks with their names.
//
// ge:summary The bot team's map in the window: the map from above, areas by belief, players, plan
// ge:keywords bot-view strategy-view commander-view top-down overhead area-colours safe lost
// ge:keywords contested unknown team-feed plan-overlay bot-camera pan zoom
// ge:skill bots render ui
// ge:test tests/shooters_edge_bot_team_test.cpp
#pragma once

#include "platform/window.hpp"
#include "render/view.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"
#include "shooters_edge/net/classic_team_areas.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

namespace sn = shooters_edge::net;

struct BotCam {
  double cx = 0, cy = 0;  // the picture's middle on the map, units
  double span = 2000;     // units across the picture's height
  bool fitted = false;
  float drag_x = -1, drag_y = -1;  // where a drag was last (-1: none)
};

// The camera moved by this frame's keys and pointer (`keys` false while the console has them).
void bot_cam_input(BotCam& cam, const sn::BotTeam& t, const ge::platform::InputSnapshot& in,
                   bool keys, double dt, int h, float px_per_unit);
// The world's view from above for the camera, `w` x `h`.
ge::render::FrameView bot_cam_view(BotCam& cam, const sn::BotTeam& t, int w, int h, double t_s);
// The team's beliefs over the frame `view` shows.
void draw_bot_view(ge::ui::DrawList& o, const ge::render::FrameView& view, int w, int h,
                   const sn::BotTeam& t, const sn::TeamAreas& areas);

}  // namespace shooters_edge::client
