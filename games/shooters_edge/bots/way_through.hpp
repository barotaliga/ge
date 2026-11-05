// THE WAY THROUGH A CAMPAIGN MAP: a fighter (fighter.hpp) playing a map to its exit, as a player
// does the first time through, off nothing but the map's own entities and his nav graph. Every
// half second he plans again:
// - the EXIT (each `trigger_changelevel`'s box) is gone for when a route leads there past every
//   door now shut that only something else opens (a door with a `targetname`: a button, a trigger
//   or a key's pickup moves it);
// - else the nearest STEP a route leads to that is not done yet: a button (walked into, or shot
//   from where he sees it: `button_poke_cmd`), a key, a trigger that fires something (walked
//   through) that OPENS a shut door (its target the door's name, or a counter's or relay's that
//   names it) whose opening would let him reach places he has not seen, or the exit; one he spends
//   `kWayGiveUp_s` on without doing is left for a while;
// - else the nearest place he has not been (exploring: a way may open where he has not looked);
//   one he spends `kWayGiveUp_s` on without getting there counts as seen;
// - else any step left (a trigger that only lights a lamp or says a word);
// - nothing at all for `kWayIdle_s` (a ledge a pool let him out on, its doors opened from the
//   other side): back into the water, which leads him out to the map's main part.
// The plan is his `objective` (fighter.hpp): he fights what he meets on the way like anyone, and
// goes on.
//
// Off the hot path: a plan is a few routes on the graph twice a second.
//
// ge:summary The way through a campaign map: the exit, else a button, key or trigger, else explore
// ge:keywords way-through way_init way_plan campaign exit changelevel button key trigger explore
// ge:keywords leader objective shut-door barred play-through episode
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/map_nav.hpp"

namespace shooters_edge::bots {

constexpr double kWayPlan_s = 0.5;   // he plans again this often, s
constexpr double kWayGiveUp_s = 25;  // a step not done in this long is left
constexpr double kWayRetry_s = 60;   // and tried again after this
constexpr double kWayIdle_s = 5;     // nothing to do this long: back into the water
constexpr float kWaySeen = 192;      // a node this near where he has stood is explored, units

enum class WayKind : uint8_t { exit, button, key, trigger, explore };
inline constexpr int kWayKinds = 5;
const char* way_kind_name(WayKind k);

struct WayStep {
  sh::Vec3f lo, hi;  // its box (the item's spot grown a little)
  std::string target, name;
  int32_t node = -1;  // where it is reached from
  int button = -1;    // a button's index in the fighter's spots
  int key_bit = 0;    // a key's item bit
  double left_until = -1;
  WayKind kind = WayKind::exit;
  bool done = false;
  bool opens = false;      // it opens a door that only something else opens (or counts toward one)
  std::vector<int> doors;  // those doors' entities
};

struct WayStats {
  int64_t plans = 0, by_kind[kWayKinds] = {}, steps_done = 0, left = 0;
};

struct WayThrough {
  std::vector<WayStep> steps;
  std::vector<int> locked;           // doors something else opens: their entities
  std::vector<sh::Vec3f> locked_at;  // and where each stands shut
  std::vector<int32_t> barred;       // those shut now (the plan's)
  std::vector<int32_t> barred_was;   // and at the last plan
  std::vector<int32_t> node_door;    // each node: the bridge it stands on (map_nav.hpp), -1 none
  std::vector<int64_t> gain;  // each locked door: the places not yet seen its opening reaches
  std::vector<uint8_t> seen;  // nodes he has stood near
  ge::ai::Search search;
  WayStats stats;
  std::vector<std::string>* log = nullptr;  // each change of plan said, when given
  double plan_at = -1, step_since = 0;
  int step = -1;                 // the step he is on, -1 none (exploring)
  double idle_since = -1;        // nothing to do since then, -1 something
  int dive = -1;                 // the water's entry (MapNav::entries) he goes back in by, -1 none
  int out = -1;                  // and the one on the far side he swims to and climbs out by
  std::vector<uint8_t> trapped;  // the nodes he could walk to when he went back in
  int32_t explore = -1;          // the place not seen he is going to
  double explore_since = 0;      // and since when
  WayKind kind = WayKind::explore;
};

// The steps off `spec`'s entities (the fighter's buttons already read by `fighter_init`), the
// doors something else opens.
void way_init(WayThrough& w, const Fighter& f, const MapNav& nav, const server::RulesCommon& s,
              const server::MapSpec& spec);
// The plan for the fighter in entity `me`: his objective (and his poke at a button) set.
void way_plan(WayThrough& w, Fighter& f, const MapNav& nav, const server::RulesCommon& s, int me,
              double t);

// The command after a fighter's own (the leader's or a companion's): wedged on his way (no further
// than kWayStuck in kWayStuck_s while he means to move), he jumps and steps aside, a side a
// second, as a person shoves at a corner.
// The leader's command after his own, going back into the water (the plan's `dive`): over the
// edge, then across to the far side's entry (`out`) and up onto its land.
void way_dive(const WayThrough& w, const MapNav& nav, const server::RulesCommon& s, int me,
              sh::MoveCmd& cmd);

constexpr float kWayStuck = 24;
constexpr double kWayStuck_s = 1.5;
struct Wedge {
  sh::Vec3f still_at;      // where he last moved from
  double still_since = 0;  // and since when he has not
  int64_t shoves = 0;      // commands he jumped and stepped aside to come free
};
void unwedge(Wedge& w, const server::RulesCommon& s, int me, double t, sh::MoveCmd& cmd);

}  // namespace shooters_edge::bots
