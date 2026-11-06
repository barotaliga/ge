// AUTOTRACK: the camera of a multiview recording following the hottest player on its own, cut by
// cut, worked out once over the whole demo before a video is made of it (demo_video.hpp's
// `autotrack`), so the cut can land before a fight rather than after it.
//
// THE RATING, every `every_s` of the timeline, for each player alive (the dead are no candidates):
//   hot = armour x armour weight + 50 x weapon weight + powerup weight + frags
//         + streak_weight x streak + recent_weight x recent
// - armour weight: green 1, yellow 2, red 3 (the items' armour bit); weapon weight: the best
//   weapon held (the item bits, ammunition not counted), in the order launcher 10, arc 10,
//   grenades 6, heavy spiker 4, spiker 2, double 4, scattergun 2, blade 1; powerup weight: the
//   ring 500, quad 900 and shield 1000, summed for each held;
// - streak: his enemy kills since his last death; recent: his enemy kills from `window_s` before
//   now to `lookahead_s` after it (the plan reads ahead: the whole demo is known);
// - a kill is the demo scan's (demo_scan.hpp): a death with the killer whose frags rose with it;
//   a teammate killed, a suicide or a death to the world adds nothing.
//
// THE CUTS: the first at the first sample with anyone alive, to the hottest. Then, in order:
// - the tracked player died `deathwait_s` ago or more (or left): to the hottest alive, no margin;
// - a challenger is hotter by the larger of `margin` and `margin_share` of the tracked player's
//   rating, and the tracked player has been on screen `dwell_s`: to him;
// - at once, the dwell waived but the margin kept, when the challenger has just taken the quad
//   or the shield, or just made a kill that lifts his recent kills to 2 or more;
// never two cuts within `gap_s`; a tie keeps the tracked player, then the lowest slot. With
// `lockteam`, only the first pick's team.
//
// A pure function of the demo and the settings: the same plan every run.
//
// ge:summary Autotrack: the hottest player's cuts over a multiview recording, by kit and streaks
// ge:keywords autotrack hottest-player streak kill-streak camera-cuts follow-player mvd
// ge:keywords demovideo track-switch rating dwell margin deathwait lookahead
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_autotrack_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_scan.hpp"

namespace shooters_edge::client {

struct AutotrackSettings {
  double every_s = 0.5;
  double streak_weight = 150, recent_weight = 200;
  double window_s = 15, lookahead_s = 3;
  double margin = 200, margin_share = 0.15;
  double dwell_s = 4, deathwait_s = 2, gap_s = 1;
  bool lockteam = false;
};

struct AutotrackCut {
  double t = 0;  // on the demo's timeline, s
  int slot = -1;
  std::string why;  // "start", "death", "hotter", "quad", "shield", "kills"
  double hot = 0;   // his rating then
};

struct AutotrackPlan {
  std::vector<AutotrackCut> cuts;  // in time order
  int64_t samples = 0;
  double length_s = 0;
};

// The rating's kit part (armour, weapons, powerups, frags) for a player's stats.
double autotrack_kit(int32_t items, int32_t armour, int32_t frags);

// `opened` (as `open_demo` left it) and its scan into the plan. False with the reason and the
// next step when it is not a multiview recording or a frame does not read.
bool autotrack_plan(const Demo& opened, const DemoScan& scan, const AutotrackSettings& set,
                    AutotrackPlan& out, std::string& error);

// The slot the plan tracks at `t` (-1 before its first cut).
int autotrack_at(const AutotrackPlan& plan, double t);

}  // namespace shooters_edge::client
