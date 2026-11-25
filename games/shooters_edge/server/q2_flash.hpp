// QUAKE II'S MUZZLE FLASHES: a player's gun (the protocol's message 1) and a monster's (message 2)
// as data. Each flash value names its light (colour, radius, how long), its sounds (the shot and
// the tails that follow it: a shotgun's pump, a launcher's reload) and the look's flash kind; a
// monster's value also names where its gun is, ahead, right and up of the monster's origin. The
// server picks the value a shot sends; the window turns a value back into the light and the
// sounds, from the wire or from its own listen server alike (porting/shooters-edge/q2-flashes.md).
//
// A player's flash value carries the silenced bit (128): the sounds at 0.2 and the light half as
// wide. The chain gun sends 3, 4 or 5 for one, two or three bullets a frame.
//
// ge:summary Quake II's muzzle flashes as data: light, sounds and gun place by flash value
// ge:keywords q2 muzzle-flash muzzleflash muzzleflash2 silenced flash-value monster-gun
// ge:keywords weapon-sound reload-sound shot-sound
// ge:skill shooter
// ge:test tests/shooters_edge_q2_flash_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace shooters_edge::server {

inline constexpr int kQ2FlashSilenced = 128;

// One sound a flash plays: `name` (a `%d` in it is one of 1..`variants`, picked at random), on
// `channel` (0 any, 1 the weapon), `delay` s after the flash.
struct Q2FlashSound {
  const char* name = nullptr;
  int variants = 0;
  int channel = 1;
  float delay = 0;
  float attenuation = 1;  // 0 heard everywhere on the level
};

// What a flash value shows and plays. `look` is the window's flash kind ("blaster", "gun",
// "rocket", "rail", "bfg"; null: a light alone). No colour: nothing is lit (a value with no
// case).
struct Q2Flash {
  float rgb[3] = {0, 0, 0};
  int radius = 200;        // plus a random 0..31 (a silenced player's: half)
  float light_s = 0.016f;  // how long its light lasts
  Q2FlashSound sounds[3];
  const char* look = nullptr;
  bool puff = false;                   // a monster's gun: grey specks at the flash point
  bool smoke = false;                  // and a puff of smoke with a flash
  float ahead = 0, right = 0, up = 0;  // a monster's gun from its origin, units
};

// A player's flash value (the silenced bit stripped); null for one with no case.
const Q2Flash* q2_player_flash(int value);
// A monster's flash value; null for one with no case.
const Q2Flash* q2_monster_flash(int value);
// The value a monster's shot of look `look` ("blaster", "gun", "rocket", "rail", "bfg"; a
// grenade has a rocket's look) sends for a monster of `classname`; 0 for none.
int q2_monster_flash_value(std::string_view classname, std::string_view look);

}  // namespace shooters_edge::server
