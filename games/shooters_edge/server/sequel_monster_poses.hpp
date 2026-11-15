// THE SEQUEL'S MONSTERS' RUNS OF FRAMES as data (sequel_poses.hpp's `SqMonsterPoses`), header-only
// so the classic campaign's rules can play a sequel monster's frames too (classic remix's
// `cr_monsters`, rules/campaign_sequel_kinds.cpp).
//
// ge:summary The sequel's monsters' frame runs by kind: stand, walk, run, attacks, pains, deaths
// ge:keywords sequel poses monster-animation frame-table sq_monster_poses cr_monsters
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_poses_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/server/sequel_poses.hpp"

namespace shooters_edge::server {

// Each kind's runs, measured from its model's frame names (the frames of one name in order); which
// run plays which action is read from the names (stand, walk, run, attack, pain, death).
inline constexpr SqMonsterPoses sq_soldier_poses(const char* classname, SqSpan a0, SqSpan a1,
                                                 int16_t hold) {
  SqMonsterPoses p{classname, {146, 175}, {215, 224}, {99, 104}};
  p.attack[2][0] = a0;
  p.attack[2][1] = a1;
  p.hold[2] = hold;
  p.pain[0] = {50, 54};
  p.pain[1] = {55, 61};
  p.pain[2] = {62, 79};
  p.death[0] = {272, 307};
  p.death[1] = {308, 342};
  p.death[2] = {343, 387};
  p.death[3] = {388, 440};
  p.death[4] = {441, 464};
  p.death[5] = {465, 474};
  return p;
}

inline constexpr SqMonsterPoses kSqMonsterPoses[] = {
    sq_soldier_poses("monster_soldier_light", {0, 11}, {12, 29}, -1),
    sq_soldier_poses("monster_soldier", {0, 11}, {12, 29}, -1),
    sq_soldier_poses("monster_soldier_ss", {39, 44}, {},
                     41),  // holds its third frame through the burst
    {.classname = "monster_infantry",
     .stand = {1, 71},
     .walk = {72, 91},
     .run = {92, 99},
     .attack = {{{199, 206}}, {}, {{184, 198}}},
     .pain = {{100, 109}, {110, 119}},
     .death = {{125, 144}, {145, 169}, {170, 178}}},
    {.classname = "monster_gunner",
     .stand = {0, 69},
     .walk = {70, 93},
     .run = {94, 101},
     .attack = {{}, {}, {{129, 158}}, {{108, 128}}},
     .pain = {{159, 176}, {177, 184}, {185, 189}},
     .death = {{190, 200}}},
    {.classname = "monster_berserk",
     .stand = {0, 4},
     .walk = {25, 35},
     .run = {36, 41},
     .attack = {{{42, 54}, {55, 75}}},
     .pain = {{199, 202}, {203, 222}},
     .death = {{223, 235}, {236, 243}}},
    {.classname = "monster_parasite",
     .stand = {83, 117},
     .walk = {68, 82},
     .run = {68, 82},
     .attack = {{}, {}, {{39, 56}}},
     .pain = {{57, 67}},
     .death = {{32, 38}}},
    {.classname = "monster_flyer",
     .stand = {13, 57},
     .walk = {13, 57},
     .run = {13, 57},
     .attack = {{{79, 95}}, {}, {{58, 78}}},
     .pain = {{134, 142}, {143, 146}, {147, 150}}},
    {.classname = "monster_tank",
     .stand = {0, 29},
     .walk = {30, 54},
     .run = {30, 54},
     .attack = {{}, {}, {{55, 76}}, {{115, 167}}, {{77, 114}}},
     .pain = {{197, 200}, {201, 205}, {206, 221}},
     .death = {{222, 253}}},
    {.classname = "monster_tank_commander",
     .stand = {0, 29},
     .walk = {30, 54},
     .run = {30, 54},
     .attack = {{}, {}, {{55, 76}}, {{115, 167}}, {{77, 114}}},
     .pain = {{197, 200}, {201, 205}, {206, 221}},
     .death = {{222, 253}}},
    {.classname = "monster_medic",
     .stand = {12, 101},
     .walk = {0, 11},
     .run = {102, 107},
     .attack = {{}, {}, {{177, 236}}},
     .pain = {{108, 115}, {116, 130}},
     .death = {{147, 176}}},
    {.classname = "monster_brain",
     .stand = {162, 221},
     .walk = {0, 12},
     .run = {0, 12},
     .attack = {{{53, 70}}, {{71, 87}}},
     .pain = {{88, 108}, {109, 116}, {117, 122}},
     .death = {{123, 140}, {141, 145}}},
    {.classname = "monster_chick",
     .stand = {121, 150},
     .walk = {181, 207},
     .run = {181, 207},
     .attack = {{{32, 47}}, {}, {{0, 31}}},
     .pain = {{90, 94}, {95, 99}, {100, 120}},
     .death = {{48, 59}, {60, 82}}},
    {.classname = "monster_gladiator",
     .stand = {0, 6},
     .walk = {7, 22},
     .run = {23, 28},
     .attack = {{{29, 45}}, {}, {{46, 54}}},
     .pain = {{55, 60}},
     .death = {{61, 82}}},
    {.classname = "monster_mutant",
     .stand = {62, 125},
     .walk = {126, 148},
     .run = {56, 61},
     .attack = {{{0, 14}}, {}, {{0, 14}}},
     .pain = {{34, 38}, {39, 44}, {45, 55}},
     .death = {{15, 23}, {24, 33}}},
    {.classname = "monster_floater",
     .stand = {144, 195},
     .walk = {144, 195},
     .run = {144, 195},
     .attack = {{{70, 103}}, {{45, 69}}, {{31, 44}}},
     .pain = {{117, 123}, {124, 131}, {132, 143}},
     .death = {{104, 116}}},
    {.classname = "monster_hover",
     .stand = {0, 29},
     .walk = {30, 64},
     .run = {30, 64},
     .attack = {{}, {}, {{197, 204}}},
     .pain = {{113, 140}, {141, 152}, {153, 161}},
     .death = {{162, 172}}},
    {.classname = "monster_flipper",
     .stand = {41, 64},
     .walk = {41, 64},
     .run = {41, 64},
     .attack = {{{0, 19}}},
     .pain = {{94, 98}, {99, 103}},
     .death = {{104, 159}}},
    {.classname = "monster_supertank",
     .stand = {194, 253},
     .walk = {128, 145},
     .run = {128, 145},
     .attack = {{}, {}, {{0, 19}}, {{20, 46}}},
     .pain = {{164, 167}, {168, 171}, {172, 175}},
     .death = {{98, 121}}},
    {.classname = "monster_boss2",
     .stand = {0, 49},
     .walk = {50, 69},
     .run = {50, 69},
     .attack = {{}, {}, {{70, 109}}, {{70, 109}}},
     .pain = {{110, 131}},
     .death = {{132, 180}}},
    {.classname = "monster_jorg",
     .stand = {112, 162},
     .walk = {163, 187},
     .run = {163, 187},
     .attack = {{}, {}, {{0, 17}}, {{18, 30}}},
     .pain = {{81, 83}, {84, 86}, {87, 111}},
     .death = {{31, 80}}},
    {.classname = "monster_makron",
     .stand = {414, 473},
     .walk = {474, 490},
     .run = {474, 490},
     .attack = {{}, {}, {{201, 208}}, {{209, 234}}, {{235, 250}}},
     .pain = {{379, 382}, {383, 386}, {387, 413}},
     .death = {{251, 345}}},
    {.classname = "monster_boss3_stand", .stand = {414, 473}, .walk = {}, .run = {}},
    {.classname = "misc_insane",
     .stand = {0, 98},
     .walk = {173, 198},
     .run = {173, 198},
     .pain = {{199, 209}},
     .death = {{210, 226}}},
};

}  // namespace shooters_edge::server
