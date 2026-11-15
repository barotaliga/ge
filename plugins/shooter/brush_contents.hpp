// A VERSION 38 MAP'S CONTENTS BITS: what a brush is (solid, window, lava, slime, water, player
// clip, the currents, monster clip, ladder) and the masks a player's box collides with, alive or
// dead. The kit's world traces a map's brushes by them (world.hpp's `brushes` solids); a game's
// move over such a map reads the same bits.
//
// ge:summary A version 38 map's contents bits and the masks a player's box collides with
// ge:keywords contents brush-contents solid window lava slime water playerclip current ladder
// ge:keywords mask player-solid surf-slick monster-clip contents-bits
// ge:skill shooter brush
// ge:test tests/shooter_world_test.cpp
#pragma once

#include <cstdint>

namespace ge::shooter {

// Contents bits (a version 38 map's).
inline constexpr int32_t kSqSolid = 0x1, kSqWindow = 0x2, kSqLava = 0x8, kSqSlime = 0x10,
                         kSqWater = 0x20, kSqPlayerClip = 0x10000, kSqCurrent0 = 0x40000,
                         kSqCurrent90 = 0x80000, kSqCurrent180 = 0x100000, kSqCurrent270 = 0x200000,
                         kSqCurrentUp = 0x400000, kSqCurrentDown = 0x800000, kSqMonster = 0x2000000,
                         kSqLadder = 0x20000000;
inline constexpr int32_t kSqMaskWater = kSqWater | kSqLava | kSqSlime;
inline constexpr int32_t kSqMaskCurrent =
    kSqCurrent0 | kSqCurrent90 | kSqCurrent180 | kSqCurrent270 | kSqCurrentUp | kSqCurrentDown;
// What a living player's and a dead one's box collide with.
inline constexpr int32_t kSqMaskPlayerSolid = kSqSolid | kSqPlayerClip | kSqWindow | kSqMonster;
inline constexpr int32_t kSqMaskDeadSolid = kSqSolid | kSqPlayerClip | kSqWindow;
// A surface's slick flag: no ground friction.
inline constexpr int32_t kSqSurfSlick = 0x2;

}  // namespace ge::shooter
