// THE FIGHTER IN PARTS (fighter.hpp): what fighter.cpp shares with the files it is split over,
// each defined in its own: what an item is worth and which one is gone for (fighter_items.cpp), and
// the bridges and doors that change the way (fighter_doors.cpp). The fighter's own parts
// (`fighter_parts`): only those and fighter.cpp include this.
//
// ge:summary The fighter in parts: item worth and choice, the bridges and doors on the way
// ge:keywords fighter fighter-parts protection strength item_value door_shut best_item
// ge:keywords chain_after on_the_way pack_near watch_bridges locked_errands door_errand
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
bool door_shut(const server::RulesCommon& s, const ItemSpot& it);
int32_t best_item(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                  double t, bool heal, const sh::Vec3f* enemy, ge::ai::Why* why, int32_t skip = -1);
int32_t chain_after(const Fighter& f, const server::RulesCommon& s, const Stats& st, int32_t i,
                    double t, bool heal, const sh::Vec3f* enemy);
int32_t on_the_way(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                   sh::Vec3f velocity, double t);
int32_t pack_near(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                  float reach);

// THE BRIDGES AND DOORS ON THE WAY (fighter_doors.cpp).
void watch_bridges(Fighter& f, const server::RulesCommon& s, int me);
bool locked_errands(const Fighter& f);
void door_errand(Fighter& f, const server::RulesCommon& s, double t);

}  // namespace shooters_edge::bots::fighter_parts
