// WATCHING THE WAYS IN: a player standing in an area (holding it, or waiting on an item's
// return) does not stare at the wall his route last pointed him at. He watches the doorways an
// enemy comes in by, one, then another, and the one a sound came from first, so whoever comes in
// is already under his crosshair (the bot's pre-aim: half the reaction, shooter/bot.hpp).
//
// THE SPOTS: for each way into the area he stands in (bots/map_plan.hpp's `entries`), the quickest
// route from it to where he stands, walked back from him to the last node he still sees from his
// eye: where someone coming that way first shows. Spots within `kSameSpot_deg` of one another are
// one. Worked out once for the node he stands on, again when he stands on another.
//
// THE CHOICE: each spot is watched for a while (`kWatchMin_s` to `kWatchMax_s`, drawn off his own
// stream), then another; a sound heard lately turns him to the spot nearest its direction.
//
// ge:summary A standing bot watches the ways into its area: the doorways, a sound's first
// ge:keywords watch look pre-aim doorway entrance ways-in hold-spot watch_spots watch_choose
// ge:skill bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

// The spots `f` watches from node `node` (its eye at `eye`) on server `s` as player `me`: kept in
// `f.watch` (none when its plan gives no area there, or none is seen).
void watch_spots(Fighter& f, const server::RulesCommon& s, int me, int32_t node, sh::Vec3f eye);

// The spot to watch at time `t` (into `f.watch`; -1 none), a sound heard from `heard` (null: none)
// turning it to the nearest.
int32_t watch_choose(Fighter& f, sh::Vec3f eye, double t, const sh::Vec3f* heard);

}  // namespace shooters_edge::bots
