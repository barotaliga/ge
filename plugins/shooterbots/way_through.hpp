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
//   `kWayGiveUp_s` past twice the walk there on without doing is left for a while;
// - else the nearest place he has not been (exploring: a way may open where he has not looked);
//   one he spends `kWayGiveUp_s` on without getting there counts as seen;
// - else any step left (a trigger that only lights a lamp or says a word);
// - nothing at all for `kWayIdle_s` (a ledge a pool let him out on, its doors opened from the
//   other side): back into the water, which leads him out to the map's main part.
// The plan is his `objective` (fighter.hpp): he fights what he meets on the way like anyone, and
// goes on.
//
// THE 100% RUN (`complete`, set before way_init): the exit waits until nothing is left that a
// route reaches: no place unseen, no step not done once (every button pressed, every trigger walked
// through), no SECRET not found (Quake's `trigger_secret` boxes, Quake II's triggers that fire a
// `target_secret`; a secret wall a shot opens, `func_door_secret` or a shot `func_door` with no
// name, is a shot button of its own, `way_complete_init`), and no MONSTER alive he can get near
// (the nearest by the way there, gone for once nothing is unseen; he fights it as he fights any). A
// thing tried `kWayTries` times without being done is given up for good, so a map he cannot clear
// still ends at its exit. The map's time limit is the runner's (`EpisodeOptions::map_seconds`).
//
// Off the hot path: a plan is a few routes on the graph twice a second.
//
// ge:summary The way through a campaign map: the exit, else a button, key or trigger, else explore
// ge:keywords way-through way_init way_plan campaign exit changelevel button key trigger explore
// ge:keywords leader objective shut-door barred play-through episode
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/tour.hpp"

namespace ge::shooterbots {

constexpr double kWayPlan_s = 0.5;   // he plans again this often, s
constexpr double kWayGiveUp_s = 25;  // a step not done in this long past twice its walk is left
constexpr double kWayRetry_s = 60;   // and tried again after this
constexpr double kWayNoRoute_s = 5;  // an exit his pilot finds no route to this long is left
constexpr double kWayIdle_s = 5;     // nothing to do this long: back into the water
constexpr float kKeyCovered = 96;    // a locked door this near a key's height covers it, units
constexpr float kWaySeen = 192;      // a node this near where he has stood is explored, units

constexpr double kWayTour_s = 4;  // (`complete`) the tour worked out again this often, s
constexpr int kWayTourMax = 96;   // and over the nearest this many stops at most
constexpr int kWayTries = 2;      // (`complete`) a step or a monster left this often is given up
constexpr double kWayForever = 1e30;  // the time a thing given up for good is left until

enum class WayKind : uint8_t { exit, button, key, trigger, explore, monster, secret };
inline constexpr int kWayKinds = 7;
const char* way_kind_name(WayKind k);

struct WayStep {
  sh::Vec3f lo, hi;  // its box (the item's spot grown a little)
  std::string target, name;
  int32_t node = -1;  // where it is reached from
  int button = -1;    // a button's index in the fighter's spots
  int key_bit = 0;    // a key's item bit (Quake's), or
  int ent = -1;       // the key lying on the map (Quake II's: taken when it is gone)
  double left_until = -1;
  WayKind kind = WayKind::exit;
  bool done = false;
  bool opens = false;    // it opens a door that only something else opens (or counts toward one)
  bool keyed = false;    // a trigger that opens only for a key held: done once its doors open
  bool revisit = false;  // an exit to a map he has played: taken last
  bool leaves = false;   // a button that leads to a level change (base2's lift down to bunk1)
  bool drains = false;   // a button that moves water (it strands him in the pool): pressed last
  bool ever = false;     // (`complete`) done once: a lift's button shut again is not pressed again
  int tries = 0;         // (`complete`) times it was left
  std::vector<int> doors;   // those doors' entities
  std::vector<int> covers;  // a key's: the locked doors (WayThrough::locked) lying over it
};

// An exit: a trigger walked through, or a button pressed, that changes the level.
inline bool way_exit(const WayStep& st) { return st.kind == WayKind::exit || st.leaves; }

struct WayStats {
  int64_t plans = 0, by_kind[kWayKinds] = {}, steps_done = 0, left = 0;
  int64_t given_up = 0;  // (`complete`) steps and monsters given up for good
};

struct WayThrough {
  bool complete = false;                // the 100% run (set before way_init)
  double complete_until = kWayForever;  // and after this game time, only the way out (a map he
                                        // cannot clear still ends at its exit in its time)
  std::vector<std::string> avoid;  // maps an exit to is no way on (a secret level; before way_init)
  std::vector<WayStep> steps;
  std::vector<int> locked;            // doors something else opens: their entities
  std::vector<sh::Vec3f> locked_at;   // and where each stands shut (a turning bridge: its angles)
  std::vector<uint8_t> locked_turns;  // and whether each is a turning bridge (map_nav.hpp)
  // The maps he has played (set before way_init): an exit to one is taken last, once nothing else
  // is left (base3's way on is back through base2, by another exit than the one he came in by).
  std::vector<std::string> been;
  std::vector<int> turners;  // turning doors a step opens (`func_door_rotating`): their entities
  std::vector<sh::Vec3f> turners_at;  // and the angles each rests shut at
  std::vector<int> hatches;  // turners lying over an exit (base2's floor hatch over its shaft)
  std::vector<int> cars;     // doors a button moves that bar no one (a lift's car)
  std::vector<sh::Vec3f> cars_at;   // and where each rests
  std::vector<int32_t> barred;      // those shut now (the plan's)
  std::vector<int32_t> barred_was;  // and at the last plan
  std::vector<int32_t> node_door;   // each node: the bridge it stands on (map_nav.hpp), -1 none
  // The links whose line crosses a secret wall at rest (a drop past it down a hidden lift's shaft
  // carries no mover of its own): (the link's index, its from node, the wall's entity).
  struct Crossing {
    int32_t link = 0, from = 0;
    int door = -1;
  };
  std::vector<Crossing> crossings;
  std::vector<int> walls;  // the secret walls among `locked`
  struct KeyDoor {
    int door = -1, bit = 0;  // a Quake key door among `locked`, and its key's item bit
  };
  std::vector<KeyDoor> key_doors;
  std::vector<uint8_t> on_top;  // each node: standing on its mover's top (a lift's floor)
  std::vector<int> lifts;       // the shut doors whose button is pressed from their top
  // The way back (`reach_back`): each node's incoming links, and the nodes he returns from.
  std::vector<int32_t> rev_first, rev_link, rev_from, back_stack;
  std::vector<uint8_t> returns, back_here, back_exit;  // and from here, and to an exit
  double ride_since = -1;          // riding a mover since (the plan held meanwhile)
  std::vector<int> plats;          // plats that wait for a trigger: shut until they first move
  std::vector<uint8_t> link_shut;  // scratch: each link, shut now
  std::vector<int64_t> gain;       // each locked door: the places not yet seen its opening reaches
  std::vector<int64_t> gain_back;  // and the places at all (seen or not)
  std::vector<uint8_t> seen;       // nodes he has stood near
  ge::ai::Search search;
  WayStats stats;
  std::vector<std::string>* log = nullptr;  // each change of plan said, when given
  double plan_at = -1, step_since = 0;
  double step_for = kWayGiveUp_s;  // how long the step he is on is given (its walk's worth more)
  int step = -1;                   // the step he is on, -1 none (exploring)
  double idle_since = -1;          // nothing to do since then, -1 something
  double no_route_since = -1;      // the exit chosen and his pilot with no route to it since then
  int dive = -1;                 // the water's entry (MapNav::entries) he goes back in by, -1 none
  int out = -1;                  // and the one on the far side he swims to and climbs out by
  std::vector<uint8_t> trapped;  // the nodes he could walk to when he went back in
  int32_t explore = -1;          // the place not seen he is going to
  double explore_since = 0;      // and since when
  WayKind kind = WayKind::explore;
  // (`complete`) The monster he goes for (an entity, -1 none), since when, and each entity's
  // tries and the time it is left until.
  int prey = -1;
  double prey_since = 0;
  std::vector<double> prey_left;
  std::vector<uint8_t> prey_tries;
  // (`complete`) THE TOUR (tour.hpp): the stops open now (a step's index, or -1 - a monster's
  // entity) in the quickest order through them all and out, worked out again every kWayTour_s or
  // when the first stop is gone; the time from each to each, and its own search.
  bool toured = true;  // the tour's order rather than the nearest stop each time
  Tour tour;
  std::vector<int32_t> tour_stops, tour_nodes;
  std::vector<double> tour_cost;
  ge::ai::Search tour_search;
  double tour_at = -1;
  int64_t tours = 0;
};

// The steps off `spec`'s entities (the fighter's buttons already read by `fighter_init`), the
// doors something else opens.
void way_init(WayThrough& w, const Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              const ge::shooter::MapSpec& spec);
// (`complete`) The secret walls a shot opens (`func_door_secret`, a `func_door` with `health` and
// no name) added to the fighter's buttons as shot ones, before way_init reads them.
void way_complete_init(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                       const ge::shooter::MapSpec& spec);
// The plan for the fighter in entity `me`: his objective (and his poke at a button) set.
void way_plan(WayThrough& w, Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              int me, double t);

// The command after a fighter's own (the leader's or a companion's): wedged on his way (no further
// than kWayStuck in kWayStuck_s while he means to move), he jumps and steps aside, a side a
// second, as a person shoves at a corner.
// The leader's command after his own, going back into the water (the plan's `dive`): over the
// edge, then across to the far side's entry (`out`) and up onto its land.
void way_dive(const WayThrough& w, const MapNav& nav, const ge::shooterbots::BotView& s, int me,
              sh::MoveCmd& cmd);

constexpr float kWayStuck = 24;
constexpr double kWayStuck_s = 1.5;
struct Wedge {
  sh::Vec3f still_at;      // where he last moved from
  double still_since = 0;  // and since when he has not
  int64_t shoves = 0;      // commands he jumped and stepped aside to come free
};
void unwedge(Wedge& w, const ge::shooterbots::BotView& s, int me, double t, sh::MoveCmd& cmd);

// THE PIT'S LIP: on dry ground outside a trap (map_nav.hpp: no way on foot back out), a wish
// that would carry him over the lip of one (a stride ahead, no floor within kEdgeDrop, or only
// the trap's) is turned back, whatever pressed it (a dodge, a U-turn): a fight's side step in
// base2 dropped him into its south pool for good. A route of his own into the trap is let be.
// True when it turned the keys.
constexpr float kEdgeLook = 40;  // the stride looked along, units
constexpr float kEdgeDrop = 64;  // a floor this far down is a step, units
bool way_edge(const MapNav& nav, const Fighter& f, const ge::shooterbots::BotView& s, int me,
              sh::MoveCmd& cmd);

// INTO THE TRIGGER: a step's trigger, secret or exit out of his box though he is within
// kTouchReach of it (a thin one past its node at a corridor's end: base3's way back to base2, one
// that fires only for a player facing into it), or within kTouchNear over open floor (its node a
// step outside it, or the pilot waiting on a mover beside it: e2m6's trigger round the pillar it
// lowers), is walked into, facing it. True when it turned the keys.
constexpr float kTouchReach = 64;
constexpr float kTouchNear = 96;
bool way_touch(const WayThrough& w, const ge::shooterbots::BotView& s, int me, sh::MoveCmd& cmd);

}  // namespace ge::shooterbots
