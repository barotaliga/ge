// PEOPLE'S HOP STARTS, the book the bots copy theirs from (shooter/recipes.hpp `RecipeBook`, the
// pilot's `recipes`): 108 starts read off people's recorded games on two maps (the `se-recipes`
// lab's `--book`), 36 a level (amateur, intermediate, expert), a third each from a stand, a slow
// walk and a walk, spread over the way they set off and the side they turn to, each cut at its
// first takeoff and proven on an open floor. No map's own: a start is flown on the map from where
// the bot stands before it is played. No player's name is kept.
//
// ge:summary People's hop starts as the bots' recipe book: 108 starts by level, decoded once
// ge:keywords recipe recipes hop-start book hop_start_book copy-movement people levels
// ge:skill bots
// ge:test tests/shooter_pilot_recipes_test.cpp
#pragma once

#include "shooter/recipes.hpp"

namespace shooters_edge::bots {

// The book, decoded on the first call (kept for the program's life).
ge::shooter::RecipeBook hop_start_book();

}  // namespace shooters_edge::bots
