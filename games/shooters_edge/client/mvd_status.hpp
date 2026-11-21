// THE WATCHED PLAYER'S RECORD IN A MULTIVIEW DEMO (`mvd_status`): what he took, lost and killed
// with, his lives and his quad runs, gathered frame by frame from every player's stats and frags
// as the demo plays (`mvd_status_frame`) and drawn as a block of lines (`mvd_status_lines`;
// demo_playback.hpp draws it at `mvd_status_x`, `mvd_status_y`, in the demo clock's cells, the
// first line at the point itself).
//
// WHAT IS COUNTED, per player:
// - TOOK: the launcher, the beam and the grenades each time the weapon newly shows in his items
//   (a pack's too); the red, yellow and green armour each time it newly shows or its value rises;
//   the ring, quad and shield each time one newly shows; the megahealth each time his health rises
//   to over 100.
// - DEATHS, and LIVES: each life's time (from health over 0 to 0 or under; the current one grows),
//   frags (the times his frags rose) and team kills (the times they fell). The averages are over
//   every life, the current included; the last runs are the lives from three back to now.
// - THE LAST FIRED WEAPON: the weapon in hand when his gun last flashed this life (none after a
//   spawn, `tp_name_none`).
// - LOST: deaths with the launcher, the beam or the grenades in hand, and holding the quad.
// - KILLS and TEAMKILLS: a rise of his frags is credited to the last fired weapon by its size, a
//   fall likewise to teamkills (a suicide counts there); with no shot yet this life, to SPAWN.
// - QUAD RUNS: from the quad showing to its going, the time held and the frags and team kills in
//   it; the last three and the current, a run of no time skipped.
// A seek, a new level or another demo starts it all over. Shown: the tracked player; one not in
// the list, the first listed; none listed, a blank name and zeroes.
//
// ge:summary The watched player's took, lost and kills record in a multiview demo (mvd_status)
// ge:keywords mvd_status mvd_status_x mvd_status_y took lost kills teamkills lives quad-runs
// ge:keywords last-fired-weapon mvd_status_frame mvd_status_lines
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_mvd_info_test.cpp
#pragma once

#include <array>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

inline constexpr int kStatusWeapons = 8;  // rl lg gl sng ng ssg sg axe, the kills' order

struct MvdLife {
  double from = 0, to = -1;  // demo time, s (-1: still alive)
  int frags = 0, tks = 0;
};

struct MvdPlayerRecord {
  // Took: rl lg gl ra ya ga ring quad pent mh.
  std::array<int, 10> took{};
  int deaths = 0;
  std::vector<MvdLife> lives;
  int last_fired = -1;        // the weapon's index in the kills' order (-1: none this life)
  std::array<int, 4> lost{};  // rl lg gl quad
  std::array<int, kStatusWeapons + 1> kills{}, teamkills{};  // the last: SPAWN
  std::vector<MvdLife> quad_runs;
  // The last frame's state:
  bool seen = false;
  int items = 0, armour = 0, health = 0, frags = 0;
  double flashed = -1;
};

struct MvdStatus {
  std::array<MvdPlayerRecord, 32> players;
  int levels = -1;
  double time = -1;  // the last frame's demo time
};

// One frame of `d` (a multiview demo; another kind is left alone) into `m`; a seek back or a new
// level starts it over.
void mvd_status_frame(MvdStatus& m, const Demo& d);

// The block's lines for the player the view rides in `d` (`con` gives the weapon words): the
// section titles (Took, Lost, Kills, Teamkills) flagged `title`.
struct MvdStatusLine {
  std::string text;
  bool title = false;
};
std::vector<MvdStatusLine> mvd_status_lines(const MvdStatus& m, const Demo& d,
                                            const ge::console::Console& con);

}  // namespace shooters_edge::client
