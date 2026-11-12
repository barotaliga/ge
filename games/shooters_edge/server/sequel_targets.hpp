// THE SEQUEL'S OTHER SINGLE-PLAYER ENTITIES: the locks, counters, hazards, effects and props a
// campaign's levels use beyond the movers and triggers (sequel_map.hpp), from the clean-room note
// porting/shooters-edge/sequel-campaign.md § The remaining single-player map entities.
//
// - **Locks and counts**: `trigger_key` (a use by a player holding its `item` spends it and fires
//   its targets once; without it a centred word of what is needed, at most every 5 s);
//   `trigger_counter` (`count` uses, 2 by default, then its targets; "N more to go" on the way
//   unless flag 1).
// - **Hazards**: `func_killbox` (a use kills everything in its box); `target_laser` (a beam of
//   2048, or at its target's middle, that hurts by `dmg` every frame while on; flag 1 starts it on
//   a second in); `target_blaster` (a bolt along its angle); `target_earthquake` (`count` s of
//   shaking every player on the ground up by `speed` × 100 ÷ 200); `target_splash` (a splash and
//   radius damage `dmg` within `dmg` + 40).
// - **Things that break**: `func_explosive` (100 health by default, or hidden until used with flag
//   1, or blown by a use when only named; radius damage `dmg` within `dmg` + 40, its targets,
//   gone); `misc_explobox` (a barrel: 10 health, blows 0.2 s after it is killed for 150 within
//   190); `misc_deadsoldier` (a body in one of six poses, thrown apart at -80).
// - **Light and words**: `target_lightramp` (a named light's style stepped from one letter to
//   another over `speed` s; flag 1 turns it back next time); `target_string` setting the frames of
//   its team's `target_character`s (digits 0-9, '-' 10, ':' 11, blank 12).
// - **Monsters' triggers**: `trigger_monsterjump` throws a walker that touches it along its angle
//   (`speed`, and `height` up when on the ground); `trigger_gravity` sets a toucher's gravity.
// - **Still figures**: the still ships and poses, a black hole a use removes; `func_conveyor`'s
//   switch; `target_temp_entity` sends its effect.
// - **Set pieces that move**: `func_object` (with any flag hidden until a use; it falls when
//   nothing holds it and crushes by `dmg`, 100 by default, what it lands on); `target_spawner`
//   (a monster from four spares waiting unseen, counted when they come out, or an item);
//   `misc_viper_bomb` (thrown on at the first ship's speed, nose down over a second, blowing on
//   whatever it touches: radius damage `dmg`, 1000 by default, within `dmg` + 40). The fly-by
//   ships (`misc_viper`, `misc_strogg_ship`) are the map's trains (sequel_map.cpp), hidden and
//   passable until used.
// - **The turret**: `turret_breach` turns in pitch and yaw toward what its driver sees, at most
//   `speed` (50) degrees a second, held within its limits, its `turret_base` turning with it; the
//   `turret_driver` (a roster monster) rides it and fires a rocket from its muzzle marker once it
//   has seen a player for 3 - skill seconds, then every 4 - skill. With its driver dead it levels.
//   The prisoner (`misc_insane`) is a roster monster that never fights and is never counted.
//
// ge:summary The sequel's other entities: keys, counters, lasers, shakes, blasts, barrels, ramps
// ge:keywords sequel trigger-key trigger-counter target-laser earthquake func-explosive explobox
// ge:keywords lightramp killbox monsterjump target-string deadsoldier func-object target-spawner
// ge:keywords viper-bomb fly-by-ship turret-breach turret-base turret-driver misc-insane
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_targets_test.cpp
#pragma once

#include "shooter/sequel_move.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct SequelServer;
struct SequelMonster;

// Map entity `e` set up when it is one of these; false when it is not.
bool targets_spawn(SequelServer& s, int e);
// A use of entity `e` by `activator` (a slot or -1); false when it is not one of these.
bool targets_use(SequelServer& s, int e, int activator);
// A blow to entity `e`; false when it is not one of these or takes no damage.
bool targets_damage(SequelServer& s, int e, int attacker, int damage);
// One rules frame of the lasers, shakes, ramps and fuses.
void targets_frame(SequelServer& s);
// A walking monster's touch of the monster triggers it stands in.
void targets_monster_touch(SequelServer& s, SequelMonster& m);
// A turret driver's frame: it rides its breach and aims it at `enemy` (a slot it sees, or -1),
// firing once it has seen him for 3 - skill seconds.
void targets_turret(SequelServer& s, SequelMonster& m, int enemy);
// Their world rows: frames, models, what shows.
void targets_rows(SequelServer& s);

}  // namespace shooters_edge::server
