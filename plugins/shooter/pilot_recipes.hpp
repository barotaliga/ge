// THE PILOT PLAYS PEOPLE'S HOP STARTS (`recipes`, pilot.hpp; the book: recipes.hpp): out of a stand
// or a walk, the start a person made from about the same state, copied key by key and view by view.
//
// THE CHOICE (`recipe_choose`): the book's entries of the pilot's level whose reach the hop's
// stretch runs on past (the start used up, the hop has room), each also mirrored (its
// view and swing to the other side of the way, its side keys swapped), scored by how far their low
// is from the pilot's state: the speed's difference a hundred units/s a point, and moving, the
// heading off the way's difference 45 degrees a point; the nearest `kRecipeTries` (ties off the
// pilot's own stream, so two starts from one spot differ as a person's do) are turned to the
// pilot's way and FLOWN by the move from where it stands with its own velocity (`play_recipe` on
// the map's solids): the first that leaves the ground at nearly its recorded speed, never drops
// into a pit, slides on no more than a step's worth of walls and ends near the route's line is
// committed. None, and the book is looked at again only after a while.
//
// THE PLAY (`recipe_play`): each command the keys and the view of the frame its clock has reached,
// forward and a side key as a held curve (the keyboard holds them whole), the jump pressed on the
// command that leaves the ground; CLOSED LOOP: the feet against the proof's at the same time, and
// off by more than `kRecipeOff` before the takeoff it aborts (planned on from where it is). Once
// in the air it is done: the hop flies on.
//
// The pilot's own parts (`pilot_parts`): only pilot.cpp and pilot_recipes.cpp include this.
//
// ge:summary The pilot plays people's hop starts: the nearest of the book proven, then key by key
// ge:keywords pilot recipe recipes hop-start recipe_choose recipe_play closed-loop mirror book
// ge:keywords stand-start copy-movement prove abort
// ge:skill shooter bots
// ge:test tests/shooter_pilot_recipes_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pilot.hpp"

namespace ge::shooter::pilot_parts {

inline constexpr int kRecipeTries = 6;     // entries flown by the move at a choice
inline constexpr float kRecipeOff = 40;    // the feet this far off the proof's: abort, units
inline constexpr float kRecipeLook = 240;  // the pilot's way: the route this far ahead, units

struct RecipeWant {
  float yaw = 0, forward = 0, side = 0;  // degrees; shares of maxspeed
  bool jump = false;
  int8_t curve = kNoCurveKeys;  // forward and the side key held whole (pilot.hpp `curve_keys`)
};

// A start chosen and committed (`p.recipe` set) for the pilot at `s`, its way `way` degrees, the
// hop's stretch ending at route index `end`; false when none proves.
bool recipe_choose(Pilot& p, const PilotSense& s, float way, int32_t end);

// The committed start's command: true with `w` while it plays on the ground; false once it is in
// the air (done) or aborted (`p.recipe` -1 either way).
bool recipe_play(Pilot& p, const PilotSense& s, float dt, RecipeWant& w);

}  // namespace ge::shooter::pilot_parts
