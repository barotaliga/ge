// THE SEQUEL'S GIBS: a body thrown apart (sequel_server.hpp states the rules; facts: porting note
// sequel-gibs.md). The pieces a body throws, by who it was:
//
// - a player: 4 small meats; he becomes his head (`head2` skin 1 or the skull, even odds), raised
//   32 units and thrown by the blow unclamped, then moved as a player is with the gib's box
//   (-16, -16, 0)..(16, 16, 16) and no input: it slides and stops, never bounces;
// - a body a respawn left: 4 small meats and a head (`head2`, thrown with the rest);
// - most monsters: 2 bones, 4 small meats and a head;
// - the soldiers: 3 small meats, a chest and a head; the flipper and the hover: 2 bones and 2 small
//   meats, a third meat for a head;
// - the tanks and the final boss's second stage: a small meat, 4 metal pieces (the tanks a chest
//   too) and a gear for a head, all metal; a boss's wreck: 4 small meats, 8 metal, a chest and the
//   gear, thrown as by a 500-point blow.
//
// Each piece starts anywhere in the body's box with the body's velocity plus a random throw
// (across ±100, up 200..300 units/s, × 0.7 under a 50-point blow, else × 1.2; × 0.5 for flesh),
// held to ±300 across and 200..500 up; it spins 0..600 °/s about each axis (a head about its yaw
// alone, either way). A rules frame (0.1 s): gravity, the turn, the move along a line. Flesh stops
// where it first meets a floor and slides along a wall; metal bounces (half its push into the
// surface back) and stops on a floor below 60 units/s up. It passes through bodies, takes no
// damage and goes after 10 to 20 s. It carries the gib bit (a blood trail). At most kSequelGibs
// fly at once, the oldest taking the next.
//
// ge:summary The sequel's gibs: the pieces a body throws, their flight, spin, rest and end
// ge:keywords sequel gibs gib meat bones head skull chest metal gear thrown spin toss bounce
// ge:keywords udeath blood-trail body-thrown-apart
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_gibs_test.cpp
#pragma once

#include <string_view>

#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

// Who was thrown apart: the pieces they leave.
enum class SqGibSet : uint8_t { player, corpse, flesh, soldier, small, tank, makron, wreck };

// A body to throw apart: its box in the world, its velocity, the blow (points) that did it.
struct SqGibThrow {
  sh::Vec3f lo, hi;
  sh::Vec3f velocity;
  int damage = 0;
  SqGibSet set = SqGibSet::flesh;
};

// The set a monster of `classname` throws (flesh when it names none of the others).
SqGibSet sq_gib_set(std::string_view classname);

// The pieces' models into the model list (before the player's model, which stays last).
void gibs_precache(SequelServer& s);
// The pieces thrown from the body `t`.
void gibs_throw(SequelServer& s, const SqGibThrow& t);
// The pieces moved a rules frame (0.1 s), and gone when their time is up.
void gibs_frame(SequelServer& s);
// Every piece cleared (a new map).
void gibs_clear(SequelServer& s);
// The world's rows for the pieces, from row `first`.
void gibs_rows(SequelServer& s, size_t first);
// The velocity a gibbed player's head leaves with: the body's `velocity` and the random throw of a
// `damage`-point blow at full scale, not held to any limit.
sh::Vec3f gibs_head_throw(SequelServer& s, sh::Vec3f velocity, int damage);
// The head a gibbed player's row shows (the model list's index and the skin), by his slot.
void gibs_player_head(const SequelServer& s, int slot, int& model, int& skin);

}  // namespace shooters_edge::server
