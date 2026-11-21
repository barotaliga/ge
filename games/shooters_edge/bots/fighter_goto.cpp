// GOING TO A NODE AND FOR AN ITEM (fighter_parts.hpp): the pilot's goal set from the fighter's,
// on through a chain's pickup, never into a trap or the water for a fight; an item gone for through
// its button or its dive; the key items passed on the way, and why.
//
// ge:summary The fighter's goal handed to the pilot: a node, an item, a chain, the items passed
// ge:keywords fighter go_to go_item item_passed chains chain-goal goal-node dive button-first
// ge:keywords trap-guard pass-reasons item-passed
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include <cmath>
#include <cstring>
#include <iterator>

#include "shooterbots/bot.hpp"
#include "shooters_edge/bots/bot_arsenal.hpp"
#include "shooters_edge/bots/fighter_parts.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::bots::fighter_parts {

namespace {

// A chain's pickup run through (`chains`): the item still believed there this long after is the
// news of its taking still to come, not a miss to go back for, s.
constexpr double kViaNews_s = 0.6;

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

// Goes to node `node`; with `then` (a node, `chains`) on through it (the point `at`) to `then`.
void go_to(Fighter& f, int32_t node, int32_t then, sh::Vec3f at) {
  if (node < 0) return;
  if (f.reach_at >= 0 && !f.reach.reached(node)) return;  // no way there from here
  // Never into a trap (a pit no way on foot leads back out of) from outside it: a fight's place
  // to stand or a foe's last spot down there would drop him in for good. The way through the map
  // keeps its goal: an exit is often one (base1's, down a lift's shaft).
  if (f.trap && node != f.objective && static_cast<size_t>(node) < f.trap->size() &&
      (*f.trap)[static_cast<size_t>(node)]) {
    const int32_t here = f.reach_from;  // the node his reach was worked out from: where he stands
    if (here < 0 || static_cast<size_t>(here) >= f.trap->size() ||
        !(*f.trap)[static_cast<size_t>(here)])
      return;
  }
  // Nor down into the water from dry ground for a fight (base1: a soldier on the river's bank
  // walked him off its edge back into the river he had just climbed out of). The way through the
  // map keeps its goal wherever it is, and outside a fight every goal does.
  if (f.wet && f.mode != FightMode::roam && node != f.objective &&
      static_cast<size_t>(node) < f.wet->size() && (*f.wet)[static_cast<size_t>(node)]) {
    const int32_t here = f.reach_from;
    if (here >= 0 && static_cast<size_t>(here) < f.wet->size() &&
        !(*f.wet)[static_cast<size_t>(here)])
      return;
  }
  sb::Pilot& p = f.bot.pilot.pilot;
  const sb::PilotMode pm = p.mode;
  const bool active = pm != sb::PilotMode::idle && pm != sb::PilotMode::arrived;
  if (f.features[sb::Feature::chains]) {
    if (then == node) then = -1;
    if (active && node == f.goal_node) {
      if (p.via_k >= 0 && p.route[static_cast<size_t>(p.via_k)] == node) {
        // Through it already: on to the chain's goal, or another, or none.
        if (then == p.goal) return;
        sb::pilot_unchain(p);
        f.bot.pilot.goals.assign(1, p.goal);
        f.chain_node = -1;
        if (then >= 0 && sb::bot_chain(f.bot, then, at)) f.chain_node = then;
        return;
      }
      if (p.goal == node) {  // on the way to it: chained on now, if asked
        if (then >= 0 && sb::bot_chain(f.bot, then, at)) {
          f.chain_node = then;
          f.stats.chained++;
        }
        return;
      }
      // Just run through it on a chain: the pickup's news is a moment off.
      if (node == p.via_left && p.clock_s - p.via_left_at < kViaNews_s) return;
      // (otherwise the pilot is on to a goal the fighter has left: planned again below)
    } else if (active && p.goal == node && node == f.chain_node) {
      // The chain's goal is the goal now (its pickup run through, or taken): on, no plan again.
      if (p.via_k >= 0) {
        p.via_left = p.route[static_cast<size_t>(p.via_k)];
        p.via_left_at = p.clock_s;
        p.via_k = -1;
      }
      f.goal_node = node;
      f.chain_node = -1;
      if (then >= 0 && sb::bot_chain(f.bot, then, at)) {
        f.chain_node = then;
        f.stats.chained++;
      }
      return;
    } else if (pm == sb::PilotMode::arrived && node == f.goal_node) {
      // Standing on it (waiting for it): on to the next when there is one.
      if (then >= 0 && sb::bot_chain(f.bot, then, at)) {
        f.chain_node = then;
        f.stats.chained++;
      }
      return;
    }
    f.goal_node = node;
    f.chain_node = then;
    if (then >= 0) f.stats.chained++;
    sb::bot_goal(f.bot, node, then, at);
    return;
  }
  if (node == f.goal_node && pm != sb::PilotMode::idle && pm != sb::PilotMode::arrived) return;
  if (node == f.goal_node && pm == sb::PilotMode::arrived) return;  // standing on it
  f.goal_node = node;
  sb::bot_goal(f.bot, node);
}

// THE KEY ITEMS PASSED (the decision log's `item_passed`): each key item (an armour, the mega,
// the quad, the launcher) there within kOnTheWay that is not the short goal taken, and why: not
// known there (believed taken: knowledge.hpp), the short
// goal not looked for (`unasked`: its goal near, retreating, ...) or the first thing that bars it
// (on_the_way_bar). One decision an item; the log keeps a row when its reason changes.
void item_passed(Fighter& f, const ge::shooterbots::BotView& s, const Stats& st, sh::Vec3f me,
                 sh::Vec3f velocity, double t, int32_t near, const char* unasked,
                 ge::ai::Why* why) {
  for (size_t k = 0; k < f.items.size(); k++) {
    const ItemSpot& it = f.items[k];
    const auto i = static_cast<int32_t>(k);
    const int kind = it.kind == ItemKind::armor3                                       ? 0
                     : it.kind == ItemKind::armor2                                     ? 1
                     : it.kind == ItemKind::mega                                       ? 2
                     : it.kind == ItemKind::quad                                       ? 3
                     : it.kind == ItemKind::weapon && it.weapon == f.arsenal->launcher ? 4
                                                                                       : -1;
    // There on the server (its model sent), whatever the fighter believes: the record's truth.
    const bool there = it.ent >= 0 && static_cast<size_t>(it.ent) < s.world.entities.size() &&
                       s.world.entities[static_cast<size_t>(it.ent)].modelindex > 0;
    if (kind < 0 || !there || i == f.goal_item) continue;
    const float d = dist(me, it.at);
    if (d > kOnTheWay) continue;
    const char* bar = i == near     ? "taken on the way"
                      : !it.present ? "not known there"
                      : unasked     ? unasked
                                    : on_the_way_bar(f, s, st, me, velocity, t, i);
    if (bar == nullptr) bar = "farther than another";
    for (size_t r = 0; r < std::size(kPassReasons); r++)
      if (std::strcmp(kPassReasons[r], bar) == 0) f.stats.passed[kind][r]++;
    if (!why || i == near) continue;
    why->decide("item_passed");
    why->fact("item", i);
    why->fact("dist", d);
    why->fact("health", st.health);
    why->fact("x", me.x);
    why->fact("y", me.y);
    why->fact("z", me.z);
    why->choose(bar, it.label.c_str());
  }
}

// Goes for item `i`: through its button first while its door is shut. With `then` (an item,
// `chains`) and `i` there to take, on through it to `then` without a stop.
void go_item(Fighter& f, const ge::shooterbots::BotView& s, int32_t i, int32_t then) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (f.goal_item != i) f.stats.goal_changes++;
  f.goal_item = i;
  if (it.dive && f.reach_at >= 0 && !f.reach.reached(it.node)) {
    // Through the water: to the dive's node, then the field (fighter_command) takes it in.
    f.unlocking = false;
    if (f.last_dive != i) f.dives++;
    f.last_dive = i;
    f.diving = i;
    go_to(f, it.dive_node);
    return;
  }
  f.diving = -1;
  const bool unlock = door_shut(s, it) && it.button_node >= 0;
  if (unlock && !f.unlocking) f.buttons++;
  f.unlocking = unlock;
  int32_t then_node = -1;
  if (then >= 0 && then != i && !unlock && it.present) {
    const ItemSpot& n = f.items[static_cast<size_t>(then)];
    const bool by_dive = n.dive && f.reach_at >= 0 && !f.reach.reached(n.node);
    if (n.node >= 0 && n.node != it.node && !by_dive && !door_shut(s, n)) then_node = n.node;
  }
  f.chain_item = then_node >= 0 ? then : -1;
  go_to(f, unlock ? it.button_node : it.node, then_node, it.at);
}

}  // namespace shooters_edge::bots::fighter_parts
