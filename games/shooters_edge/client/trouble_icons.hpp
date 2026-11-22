// THE CLASSIC TROUBLE ICONS: the turtle (`showturtle`) and the net picture drawn in the top-left
// corner of the view, as Quake and QuakeWorld warn of them. The turtle shows once the last three
// frames have each taken 0.1 s or more (ten frames a second or fewer), only while `showturtle`
// is on; the net picture, 64 classic pixels to its right, while the server is silent
// (`net/qw_remote.hpp`'s `remote_net_trouble`: a Quake server 0.3 s without a packet, a
// QuakeWorld one with 63 of the client's packets unacknowledged). Both are the game's own
// pictures (`hud::HudArt`'s `turtle` and `net`); a set without them draws nothing.
//
// ge:summary The classic turtle and net icons: when each shows, and drawn in the view's corner
// ge:keywords showturtle turtle net-icon slow-frame trouble_icons turtle_due draw_trouble_icons
// ge:skill ui
// ge:test tests/shooters_edge_trouble_icons_test.cpp
#pragma once

#include "loop/pacing.hpp"
#include "shooters_edge/hud/art.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

inline constexpr int kTurtleFrames = 3;       // slow frames in a row before the turtle shows
inline constexpr double kTurtleFrameS = 0.1;  // a frame this long or longer is slow
inline constexpr float kNetIconX = 64;        // classic pixels right of the turtle's corner

// Whether the turtle shows: `on` (showturtle) and the last three of `times` slow.
bool turtle_due(const ge::loop::FrameTimes& times, bool on);

// The turtle (when `turtle_due(times, showturtle)`) and the net picture (when `net`), in the
// top-left corner of a `w` x `h` screen at the classic screen's whole scale; nothing for a
// picture `art` lacks.
void draw_trouble_icons(ge::ui::DrawList& dl, int w, int h, const hud::HudArt& art,
                        const ge::loop::FrameTimes& times, bool showturtle, bool net);

}  // namespace shooters_edge::client
