// THE TEAM GAME WATCHED (the `se-team` lab, se_team_lab.cpp): what the lab notes of every bot on
// every command of a match (the hop and its flights by skill, where and why they stand still, the
// view by what turned it, facing an enemy, the bumps and what took the speed) and the metrics and
// tables it reports from them, with every flight of the match's own recording put down to the
// pilot's mode at its takeoff; and the pilot's link as the trace writes it. Beside the lab,
// compiled into the `labs` executable.
//
// ge:summary The se-team lab's watch: hops, flights, still, the view, facing, bumps, links
// ge:keywords lab se-team watch hop flights still bumps bump_by facing appear view-writers
// ge:keywords turnview hop_metrics still_view_metrics facing_metrics flights_by_mode team-watch
// ge:keywords link_of link_brink link2_of trace
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "lab/lab.hpp"
#include "shooter/server.hpp"
#include "shooter/vec.hpp"
#include "shooterbots/pilot.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/labs/why_log.hpp"
#include "shooters_edge/net/qw_team.hpp"

namespace shooters_edge::labs::se_team {

namespace sb = ge::shooterbots;

// THE HOP, measured on every command of every bot, by skill (`team.hop.<skill>.*`): its share of
// the time alive, its speed's median, the time alive past 340 u/s across; and every FLIGHT (a
// hop's landing jump through the hopping commands after it, five or more): the takeoff speed,
// the air's gain (the last hopping command's speed less the takeoff's), a crash (80 u/s or more
// lost), a takeoff past 360, chained (the next command jumps again off a landing) or the
// stretch ended. People hop 36 % of their time; the bots' gap jumps need 420-500 at takeoff.
// WHY AND WHERE THEY STAND STILL: the share of every bot's commands that wait for a mover in the
// way, have no route, or walk the route under 20 u/s, and the top spots of each (64-unit cells,
// with the goal and the fight mode: feed one to se-spot). THE VIEW BY WHAT TURNED IT: each view
// writer's (aim, look, calm, air, swim, pilot and the game's overrides: shooterbots/bot.hpp
// `view_by`) share of the time and its reversals a second (the yaw's turn flipping between two
// commands, both past 60 degrees a second; people 0.09 a second in se-people's
// `hand.reversals-per-s`).
inline constexpr int kStillKinds = 3;
inline constexpr const char* kStillNames[kStillKinds] = {"mover", "no-route", "walk"};

struct HopSkill {
  int64_t alive = 0, hop = 0, fast = 0;
  int flights = 0, crashes = 0, takeoffs_360 = 0, chained = 0;
  std::vector<float> hop_speed, gain;
};
struct Watch {
  // FACING (se-people's rule, every 4th command alive): an enemy in a clear line within the bots'
  // sight anywhere round (`seen`), and one of them on the 120-degree screen (`faced`), by the
  // bot's mode and what wrote its view: the time an enemy in plain sight lies off the screen.
  std::map<std::string, std::array<int64_t, 2>> facing;
  // The time off the screen by where the nearest enemy in sight lies (beside, behind, over or
  // under), how far, and whether he faces the bot (it in his own screen: coming at it).
  std::map<std::string, int64_t> facing_off;
  // AN ENEMY APPEARING (se-people's `appear`): one newly in a clear line, by the bot's mode and
  // view writer then: appearances, and those met on the screen.
  std::map<std::string, std::array<int64_t, 2>> appear;
  std::map<std::string, std::array<int64_t, 2>> appear_way;  // roaming: by the way it moves
  std::array<uint64_t, 64> clear_was{};  // by fighter: the enemy slots in a clear line last look
  std::ofstream* out = nullptr;          // the trace, when asked for
  shooters_edge::labs::WhyLog* log = nullptr;  // the decision log, when asked for (--why)
  int every = 4;
  std::array<int, 64> count{};
  struct Flight {
    bool on = false;
    float v0 = 0, last = 0;
    int n = 0;
  };
  std::array<Flight, 64> flight{};
  std::array<HopSkill, 4> skill{};  // by sb::AimSkill
  // Why and where they stand (`team.still.*`): a mover in the way, no route, walking under 20 u/s.
  int64_t commands = 0;
  std::array<int64_t, kStillKinds> still{};
  std::array<std::map<std::tuple<int, int, int, std::string, std::string>, int>, kStillKinds> spots;
  // The view's turn by what turned it (`team.view.*`): each bot's last yaw and rate.
  struct View {
    double t = -1;
    float yaw = 0, rate = 0;
  };
  std::array<View, 64> view{};
  // Each bot's pilot mode over the match, a row a change (the time, the mode's name): the
  // flights read off the recording afterwards are put down to the mode at their takeoff.
  std::array<std::vector<std::pair<double, const char*>>, 64> modes;
  // The bumps (`team.bumps.*`): a speed lost in one command, 120 u/s or more (a wall, a corner,
  // a player met; the move brakes at most ~40 a command), hard when it ends under 100 u/s.
  // Each is put down to what took the speed (bump_by: a wall, a stair's step, a mover, a player,
  // open) and its spot kept (64-unit cells, the pilot's mode): the corners a look ahead must see.
  std::array<float, 64> last_speed{};
  std::array<ge::shooter::Vec3f, 64> last_vel{};
  // THE VIEW IN THE RUN'S TURN (se-moves' tracks by one rule, M18r.md 6): on the ground at 200+
  // units/s, the view's yaw off the velocity's in the turn's direction while the run turns past
  // 90 degrees a second (people's p25 -11 to -18: the view leads or keeps up; a view trailing by
  // 40+ is a bot's), and the view within 15 degrees of the run of all that ground.
  std::array<double, 64> turn_t{};
  std::array<bool, 64> turn_ground{};
  int64_t run_frames = 0, run_on = 0;
  std::vector<float> turn_lead;
  std::map<std::string, std::array<int64_t, 2>> turn_by;  // the view's writer: turning, trailing
  std::array<float, 64> last_health{};
  std::array<bool, 64> moved{};
  int64_t bumps = 0, hard_bumps = 0, alive_commands = 0;
  // Each side's (red 0, blue 1) commands alive and its bumps by walls and by players, for an A/B
  // by team (`--sides=1`): which side the feature under test slowed or sped.
  std::array<int64_t, 2> side_alive{}, side_wall{}, side_player{};
  std::map<std::string, int64_t> bumps_by_mode, bumps_by;
  std::map<std::tuple<int, int, int, std::string, std::string>, int> bump_spots;
  double t_last = 0;
  std::map<std::string, std::pair<double, int64_t>> writers;  // time, reversals
};

// What took a bumped player's speed (`wall`, `step`, `player`, `mover`, `open`).
const char* bump_by(const ge::shooter::Server& sv, int ent, ge::shooter::Vec3f origin,
                    ge::shooter::Vec3f vel);
// A command of bot `k` noted: the hop's share and speed, its flights (`team.hop.<skill>.*`).
void watch_hop(Watch& w, int k, ge::shooterbots::AimSkill skill, bool alive, bool hop,
               const char* because, float speed);
// The median of `v` (0 when empty).
float median(std::vector<float> v);
// A command noted for standing still (`team.still.*`): a mover in the way, no route, a slow walk.
void watch_still(Watch& w, const shooters_edge::bots::Fighter& f, const ge::shooterbots::Pilot& p,
                 float speed, float x, float y, float z);
// A command's view noted by what turned it (`team.view.*`).
void watch_view(Watch& w, int k, double t, float yaw, const char* by);
// The metrics of the view in the run's turn, facing and appearing enemies.
void facing_metrics(ge::lab::Lab& lab, const Watch& w);
// The metrics and tables of the bumps, standing still and the view's writers.
void still_view_metrics(ge::lab::Lab& lab, const Watch& w);
// The hop's metrics by skill (`team.hop.<skill>.*`).
void hop_metrics(ge::lab::Lab& lab, const Watch& w);
// Every flight of the match's recording by the pilot's mode at its takeoff (`team.air.s380.*`).
void flights_by_mode(ge::lab::Lab& lab, const Watch& w,
                     const shooters_edge::net::TeamMatchReport& r, const std::string& basedir,
                     const std::string& game, const std::string& map, bool packed);

// THE PILOT'S LINK as the trace writes it: the kind of the link toward the route's next node
// (`link_of`), whether it runs along a lip (`link_brink`), the kind of the link after it
// (`link2_of`).
const char* link_of(const ge::shooterbots::Pilot& p);
char link_brink(const ge::shooterbots::Pilot& p);
const char* link2_of(const ge::shooterbots::Pilot& p);

}  // namespace shooters_edge::labs::se_team
