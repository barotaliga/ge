// A MAP'S GOALS: every goal a team game on a map offers a player, generated from the map itself
// (its items, the plan's areas and ways in, map_plan.hpp, and its tricks, rocket_tricks.hpp), so
// any map has them and a goal its map cannot have is never made (no pent goal on dm2: no pent).
//
// A GOAL is what a player sets out to do and holds to until it is done, fails, runs out, or an
// event in the game makes another clearly better (the decision engine, mind.hpp). Each has:
// - its KIND and TARGET: an item, an area, a way in, a mate;
// - its HORIZON: how long it lives once taken up: the whole match (`match`), until its item is
//   back (`until_back`: timing it), until a power ends (`power`: about 30 s), or `seconds`;
// - its NEEDS: what must be true, as the player believes it, for the goal to be taken up or kept
//   (`Need`, bits: the item there, an area lost, a power held, a weapon lacking ...); a goal whose
//   needs stop holding is dropped;
// - its WORTH: the base the engine weighs against the rest (the item's or the area's worth).
//
// THE KINDS, what each is made from:
// - TAKE an item (each key item: armours, mega, powers, the launchers, the arc, the heavy spiker);
// - TIME an item: be at it when it is back (the armours, the mega, the powers, the launcher);
// - HOLD an area (each of the plan's areas but a power's): the team's ground, all match;
// - RETAKE an area believed lost (needs it lost: no mate there and the enemy seen or called in it);
// - CLEAR an area with a power (needs a power held, and the area lost or an enemy seen in it);
// - RUSH a power (needs it due within the plan's lead) and ESCORT its carrier (needs a mate on it);
// - ARM: get a weapon the player lacks (each launcher, the arc, the heavy spiker);
// - SPAM a way into the team's base (needs an enemy expected there and a blast weapon);
// - TRICK: a way the map's people take that the nav proved (a rocket jump to a ledge), a means
//   other goals' routes use, listed so a plan can ask for it;
// - FETCH a pack a mate pointed at (needs it pointed), DROP a launcher for a mate with a power
//   (needs a mate with the quad near, a launcher held, little health: a rocket at one's own feet);
// - SURFACE for air (needs the head under long), HEAL (needs little health), HUNT a seen enemy
//   (needs strength over his).
//
// Pure: the same map makes the same goals, in the same order.
//
// ge:summary A map's goals from the map: kinds, targets, horizons, needs and worths, any map
// ge:keywords goals objectives goal-catalogue horizon needs requirements take time hold retake
// ge:keywords clear rush escort arm spam trick fetch drop surface heal hunt map_goals commitment
// ge:skill bots
// ge:test tests/shooters_edge_map_plan_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/map_plan.hpp"

namespace shooters_edge::bots {

enum class GoalKind : uint8_t {
  take,
  time,
  hold,
  retake,
  clear,
  rush,
  escort,
  arm,
  spam,
  trick,
  fetch,
  drop,
  surface,
  heal,
  hunt,
};
inline constexpr int kGoalKinds = 15;
const char* goal_kind_name(GoalKind k);

enum class Horizon : uint8_t { match, until_back, power, seconds };
const char* horizon_name(Horizon h);

// What must hold, as the player believes it, for a goal to be taken up or kept (bits).
enum Need : uint16_t {
  kNeedPresent = 1u << 0,     // the item is there (or due within a few seconds)
  kNeedTaken = 1u << 1,       // the item is gone, its time back known
  kNeedLost = 1u << 2,        // the area is believed the enemy's
  kNeedPower = 1u << 3,       // a power held (the quad or the pent)
  kNeedMatePower = 1u << 4,   // a mate holds one
  kNeedDue = 1u << 5,         // the power due within the plan's rush lead
  kNeedLacking = 1u << 6,     // the weapon not held
  kNeedEnemyNear = 1u << 7,   // an enemy expected at the way in (seen, heard or called)
  kNeedBlast = 1u << 8,       // a blast weapon held, rockets for it
  kNeedPointed = 1u << 9,     // a mate pointed at the pack
  kNeedLauncher = 1u << 10,   // the rocket launcher held
  kNeedWeak = 1u << 11,       // little health and armour
  kNeedUnder = 1u << 12,      // the head under water a while
  kNeedStronger = 1u << 13,   // stronger than the enemy seen
  kNeedEnemySeen = 1u << 14,  // an enemy seen lately
};
inline constexpr int kNeeds = 15;
// "present+lost", "-" none.
std::string needs_text(uint32_t needs);

struct Goal {
  GoalKind kind = GoalKind::take;
  int32_t item = -1;   // into the items
  int32_t area = -1;   // into the plan's areas
  int32_t node = -1;   // a way in's or a trick's start node
  int32_t trick = -1;  // into the map's tricks
  Horizon horizon = Horizon::match;
  float seconds = 0;  // a `seconds` horizon's length
  uint32_t needs = 0;
  double worth = 0;
  std::string name;  // "take ra", "clear ya [ya-hi]", "spam ya #2"
};

// Every goal `plan` and `items` make on the map `map` ("dm3"; its tricks by name), in the kinds'
// order, each kind's by worth.
std::vector<Goal> map_goals(const MapPlan& plan, const std::vector<ItemSpot>& items,
                            const std::string& map);

// The goals as a few lines a kind, for a log or a lab.
std::string goals_text(const std::vector<Goal>& goals);

}  // namespace shooters_edge::bots
