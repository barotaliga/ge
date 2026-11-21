// THE FIGHTER IN PARTS (fighter.hpp): what fighter.cpp shares with the files it is split over,
// each defined in its own: what an item is worth and which one is gone for (fighter_items.cpp), and
// the bridges and doors that change the way (fighter_doors.cpp), and the goal handed to the pilot
// (fighter_goto.cpp). The fighter's own parts
// (`fighter_parts`): only those and fighter.cpp include this.
//
// ge:summary The fighter in parts: item worth and choice, the bridges and doors on the way
// ge:keywords fighter fighter-parts protection strength item_value door_shut best_item
// ge:keywords chain_after on_the_way pack_near watch_bridges locked_errands door_errand
// ge:keywords go_to go_item item_passed
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>

#include "ai/why.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots::fighter_parts {

// A short goal on the way (`on_the_way`): within this many units.
inline constexpr float kOnTheWay = 256;

inline float protection(float type, float value) { return type * value; }

inline float strength(const Stats& st) { return st.health + protection(st.armortype, st.armor); }

// WHAT AN ITEM IS WORTH AND WHICH ONE IS GONE FOR (fighter_items.cpp).
double item_value(const ItemSpot& it, const Stats& st);
bool door_shut(const ge::shooterbots::BotView& s, const ItemSpot& it);
int32_t best_item(const Fighter& f, const ge::shooterbots::BotView& s, const Stats& st,
                  sh::Vec3f me, double t, bool heal, const sh::Vec3f* enemy, ge::ai::Why* why,
                  int32_t skip = -1);
int32_t chain_after(const Fighter& f, const ge::shooterbots::BotView& s, const Stats& st, int32_t i,
                    double t, bool heal, const sh::Vec3f* enemy);
int32_t on_the_way(const Fighter& f, const ge::shooterbots::BotView& s, const Stats& st,
                   sh::Vec3f me, sh::Vec3f velocity, double t);
// Why item `i` is no short goal on the way now (a literal: "the goal", "a mate's", "behind its
// run", ...), or null when it is one (distance aside: within kOnTheWay, the nearest wins).
const char* on_the_way_bar(const Fighter& f, const ge::shooterbots::BotView& s, const Stats& st,
                           sh::Vec3f me, sh::Vec3f velocity, double t, int32_t i);
int32_t pack_near(const Fighter& f, const ge::shooterbots::BotView& s, const Stats& st,
                  sh::Vec3f me, float reach);

// THE GOAL HANDED TO THE PILOT (fighter_goto.cpp).
// Goes to node `node`; with `then` (a node, `chains`) on through it (the point `at`) to `then`.
void go_to(Fighter& f, int32_t node, int32_t then = -1, sh::Vec3f at = {});
// Goes for item `i`: through its button first while its door is shut. With `then` (an item,
// `chains`) and `i` there to take, on through it to `then` without a stop.
void go_item(Fighter& f, const ge::shooterbots::BotView& s, int32_t i, int32_t then = -1);
// THE KEY ITEMS PASSED (the decision log's `item_passed`): each key item within kOnTheWay that is
// not the short goal taken (`near`), and why: not known there, not looked for (`unasked`) or the
// first thing that bars it (on_the_way_bar).
void item_passed(Fighter& f, const ge::shooterbots::BotView& s, const Stats& st, sh::Vec3f me,
                 sh::Vec3f velocity, double t, int32_t near, const char* unasked, ge::ai::Why* why);

// THE BRIDGES AND DOORS ON THE WAY (fighter_doors.cpp).
void watch_bridges(Fighter& f, const ge::shooterbots::BotView& s, int me);
bool locked_errands(const Fighter& f);
// Records the errand's start and end on `why` (the decision `door_errand`: the door, its button,
// shot or touched, and why it ended: open, no way but through it, its time up).
void door_errand(Fighter& f, const ge::shooterbots::BotView& s, double t,
                 ge::ai::Why* why = nullptr);
// (`fields`) THE ERRAND KNOWN BEFORE THE DOOR: the way to the goal item read off its field starts
// with a button whose door rests shut: that door's errand begun now, from wherever the fighter
// stands (door_errand runs it on), not when the route meets the door.
void field_errand(Fighter& f, const ge::shooterbots::BotView& s, double t,
                  ge::ai::Why* why = nullptr);
// (`bridgebutton`) The button that runs bridge `k` out (-1 none), and the errand to it when the
// route planned crosses that bridge while it is in.
int32_t bridge_button_of(Fighter& f, size_t k);
void bridge_errand(Fighter& f, double t);
// (`noroute`) The goal item the pilot finds no route to: through a bridge's button, or set aside.
// `no_route_to`: the node the pilot found no route to on its last steps (-1: none), going for item
// `item` (-1: none).
void noroute_watch(Fighter& f, sh::Vec3f me, double t, int32_t no_route_to, int32_t item,
                   ge::ai::Why* why);
// Whether item `i` is set aside as a goal now (`noroute`).
inline bool route_lost(const Fighter& f, int32_t i, double t) {
  return i >= 0 && i == f.noroute_item && t < f.noroute_until;
}

}  // namespace shooters_edge::bots::fighter_parts
