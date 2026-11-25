// A QUAKE II MUZZLE FLASH AS THE WINDOW SHOWS IT: a flash value (server/q2_flash.hpp) at an entity
// turned into the sounds it plays (the shot, then a tail such as a shotgun's pump, each
// `DemoSound::delay` after) and the flash the look draws (a `te_monster_flash.<look>` effect at
// the gun, which world_fx.cpp moves to the model's gun tip and lights). Read off the wire
// (demo_q2.cpp: the protocol's messages 1 and 2) and off the listen server's events alike.
//
// A player's flash is 18 units ahead of him and 16 to his right, turned by his angles; a
// monster's at its gun's place by its value. The own player's flash plays its sounds only: the
// hand draws its own gun's flash. A silenced player's shot plays at 0.2.
//
// ge:summary Quake II's muzzle flash turned into its sounds and the look's flash at the gun
// ge:keywords q2 muzzle-flash muzzleflash muzzleflash2 flash-sound silenced q2_flash_fx
// ge:skill shooter
// ge:test tests/shooters_edge_q2_flash_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

// Flash `value` of entity `entity` at `origin` facing `angles` (degrees) into `sounds` and
// `effects`; `monster` the protocol's message 2 (no silenced bit). `pick` picks among a sound's
// variants (the machine gun's five). False for a value with no case (nothing added).
bool q2_flash_fx(int value, bool monster, int entity, sh::Vec3f origin, sh::Vec3f angles, bool own,
                 uint32_t pick, std::vector<DemoSound>& sounds, std::vector<DemoEffect>& effects);

}  // namespace shooters_edge::client
